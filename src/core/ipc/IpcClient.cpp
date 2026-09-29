#include "IpcClient.h"
#include "IpcProtocol.h"
#include "utils/Log.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonDocument>
#include <QProcessEnvironment>
#include <QRandomGenerator>
#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

IpcClient::IpcClient(QObject *parent)
    : QObject(parent)
{
    ltTrace(LT_LOG_IPC()) << "IpcClient created";

    m_watchdog = new QTimer(this);
    m_watchdog->setInterval(30000);
    connect(m_watchdog, &QTimer::timeout, this, [this]() {
        if (m_connected) {
            sendRequest(IpcProtocol::CMD_ENV_CHECK, {});
        }
        // 扫描超时请求并清理（A2：请求超时机制）
        cleanupTimedOutRequests();
    });

    // 稳定运行计时器：到期才认为后端可靠，清零重启计数
    m_stableTimer = new QTimer(this);
    m_stableTimer->setSingleShot(true);
    m_stableTimer->setInterval(STABLE_RUN_MS);
    connect(m_stableTimer, &QTimer::timeout, this, [this]() {
        if (m_connected) {
            m_restartCount = 0;
            ltDebug(LT_LOG_IPC()) << "Backend stable for" << STABLE_RUN_MS << "ms, restart count reset";
        }
    });

    ensureJobObject();
}

IpcClient::~IpcClient()
{
    // 析构时不允许再自动重启，避免对象已半销毁时调度回调
    m_autoRestart = false;
    if (m_stableTimer) m_stableTimer->stop();
    if (m_watchdog) m_watchdog->stop();

    // 先杀整棵进程树（含训练等孙进程），再关作业句柄兜底，防止孤儿进程
    killProcessTree();
    closeJobObject();

    // m_process 是本对象子对象，由 ~QObject 统一销毁；此处仅断开信号防止回调进入
    if (m_process) {
        m_process->disconnect(this);
        m_process = nullptr;
    }
    if (m_connected) {
        m_connected = false;
        emit connectedChanged();
    }
}

bool IpcClient::connected() const
{
    return m_connected;
}

void IpcClient::startBackend(const QString &pythonPath, const QString &scriptPath)
{
    m_lastPythonPath = pythonPath;
    m_lastScriptPath = scriptPath;
    ltInfo(LT_LOG_IPC()) << "startBackend python=" << pythonPath << "script=" << scriptPath;

    if (m_process && m_process->state() != QProcess::NotRunning) {
        ltWarning(LT_LOG_IPC()) << "Backend already running";
        return;
    }

    // 用户显式启动：重新开始失败计数周期，并恢复自动重启
    m_autoRestart = true;
    m_restartCount = 0;
    launchBackend();
}

void IpcClient::launchBackend()
{
    if (m_process) {
        if (m_process->state() != QProcess::NotRunning) {
            ltWarning(LT_LOG_IPC()) << "Backend already running";
            return;
        }
        m_process->deleteLater();
        m_process = nullptr;
    }

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("PYTHONUNBUFFERED"), QStringLiteral("1"));
    QStringList candidateDirs = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/backend"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../backend"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../../../backend"),
    };
    for (const auto &dir : candidateDirs) {
        QString canonical = QDir(dir).canonicalPath();
        if (!canonical.isEmpty() && QDir(canonical).exists(QStringLiteral("labeltorch_backend"))) {
            QString existingPath = env.value(QStringLiteral("PYTHONPATH"));
            env.insert(QStringLiteral("PYTHONPATH"),
                       existingPath.isEmpty() ? canonical : canonical + QDir::listSeparator() + existingPath);
            break;
        }
    }
    m_process->setProcessEnvironment(env);

    connect(m_process, &QProcess::started,
            this, &IpcClient::onBackendStarted);
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &IpcClient::onBackendReadyRead);
    connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
        if (m_process) {
            QByteArray errData = m_process->readAllStandardError();
            if (!errData.isEmpty()) {
                QString errStr = QString::fromUtf8(errData).trimmed();
                QStringList lines = errStr.split(QStringLiteral("\n"));
                for (const QString &line : lines) {
                    QString cleanLine = line.trimmed();
                    if (!cleanLine.isEmpty()) {
                        ltError(LT_LOG_IPC()) << "[Python Stderr]" << cleanLine;
                    }
                }
            }
        }
    });
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &IpcClient::onBackendFinished);
    connect(m_process, &QProcess::errorOccurred,
            this, &IpcClient::onBackendErrorOccurred);

    // 使用启动参数缓存（自动重试路径也从这里取值）
    QString py = m_lastPythonPath.isEmpty() ? QStringLiteral("python") : m_lastPythonPath;
    QString script = m_lastScriptPath.isEmpty()
        ? QStringLiteral("-m") : m_lastScriptPath;

    // 异步启动：不再 waitForStarted 阻塞 UI，状态由 started/finished/errorOccurred 信号驱动
    if (script == QStringLiteral("-m")) {
        m_process->start(py, {QStringLiteral("-m"), QStringLiteral("labeltorch_backend.server")});
    } else {
        m_process->start(py, {script});
    }
}

void IpcClient::onBackendStarted()
{
    ltInfo(LT_LOG_IPC()) << "Backend process started, pid=" << m_process->processId();
    m_connected = true;
    emit connectedChanged();
    m_watchdog->start();

    // 将后端进程纳入 Job Object，使其子进程（训练等）同属一棵树
    assignProcessToJob();

    // 稳定运行满 STABLE_RUN_MS 后才清零重启计数，避免"一起来就崩"被误判为偶发失败
    m_stableTimer->start();
}

void IpcClient::stopBackend()
{
    ltInfo(LT_LOG_IPC()) << "stopBackend";

    m_watchdog->stop();
    m_stableTimer->stop();
    m_autoRestart = false;

    if (m_process && m_process->state() != QProcess::NotRunning) {
        sendRequest(IpcProtocol::CMD_SHUTDOWN, {});
        // 异步停止：给后端优雅退出窗口，超时后杀整棵进程树；不阻塞 UI 线程
        QTimer::singleShot(STOP_GRACE_MS, this, [this]() {
            if (m_process && m_process->state() != QProcess::NotRunning) {
                ltWarning(LT_LOG_IPC()) << "Backend did not exit gracefully, killing process tree";
                killProcessTree();
            }
        });
    }

    // connected 状态由 finished 信号统一维护，此处不再同步翻转
}

QString IpcClient::sendRequest(const QString &command, const QJsonObject &payload)
{
    if (!m_process || m_process->state() != QProcess::Running) {
        ltWarning(LT_LOG_IPC()) << "Cannot send request, backend not running";
        return {};
    }

    QString requestId = QStringLiteral("req_%1_%2")
        .arg(++m_requestCounter)
        .arg(QRandomGenerator::global()->bounded(10000));

    // 记录待响应请求（含时间戳，用于超时清理）
    m_pendingCommands[requestId] = PendingCommand{command, QDateTime::currentDateTime()};

    QJsonObject request = IpcProtocol::createRequest(requestId, command, payload);
    QByteArray data = QJsonDocument(request).toJson(QJsonDocument::Compact) + "\n";

    ltDebug(LT_LOG_IPC()) << "sendRequest id=" << requestId << "command=" << command;
    m_process->write(data);
    return requestId;
}

void IpcClient::cleanupTimedOutRequests()
{
    if (m_pendingCommands.isEmpty()) return;

    QDateTime now = QDateTime::currentDateTime();
    QStringList timedOutIds;
    for (auto it = m_pendingCommands.begin(); it != m_pendingCommands.end(); ++it) {
        if (it.value().startTime.msecsTo(now) >= REQUEST_TIMEOUT_MS) {
            timedOutIds.append(it.key());
        }
    }

    for (const QString &id : timedOutIds) {
        QString cmd = m_pendingCommands.value(id).command;
        m_pendingCommands.remove(id);
        ltWarning(LT_LOG_IPC()) << "Request timed out id=" << id << "command=" << cmd;
        emit requestTimeout(id, cmd);
    }
}

void IpcClient::onBackendReadyRead()
{
    while (m_process && m_process->canReadLine()) {
        QByteArray line = m_process->readLine().trimmed();
        if (line.isEmpty()) continue;

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        if (err.error != QJsonParseError::NoError) {
            ltWarning(LT_LOG_IPC()) << "JSON parse error:" << err.errorString();
            continue;
        }

        processMessage(doc.object());
    }
}

void IpcClient::processMessage(const QJsonObject &msg)
{
    QString type = msg[QStringLiteral("type")].toString();

    if (type == QStringLiteral("response")) {
        QString requestId = msg[QStringLiteral("request_id")].toString();
        QJsonObject response = msg;

        if (!response.contains(QStringLiteral("command"))) {
            if (m_pendingCommands.contains(requestId)) {
                response[QStringLiteral("command")] = m_pendingCommands[requestId].command;
            }
            m_pendingCommands.remove(requestId);
        }

        ltDebug(LT_LOG_IPC()) << "responseReceived id=" << requestId;
        emit responseReceived(response);
    } else if (type == QStringLiteral("event")) {
        ltDebug(LT_LOG_IPC()) << "eventReceived type=" << msg[QStringLiteral("event_type")].toString();
        emit eventReceived(msg);
    } else {
        ltWarning(LT_LOG_IPC()) << "Unknown message type:" << type;
    }
}

int IpcClient::nextRestartDelayMs() const
{
    // 指数退避：第 1 次 1s、第 2 次 2s、第 3 次 4s、第 4 次 8s，其后按 2^n 增长但不超过 30s
    // 用 qMin 而非 std::min，避免 Windows.h 的 min/max 宏展开冲突
    int shift = qMin(m_restartCount - 1, 5);
    if (shift < 0) shift = 0;
    return qMin(30000, 1000 << shift);
}

void IpcClient::onBackendFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    ltWarning(LT_LOG_IPC()) << "Backend finished exitCode=" << exitCode
                             << "exitStatus=" << exitStatus;

    m_stableTimer->stop();
    if (m_connected) {
        m_connected = false;
        emit connectedChanged();
    }
    m_watchdog->stop();

    // 本次运行未满稳定阈值，重启计数继续累加；由 onBackendStarted 启动的稳定计时器到期才清零
    if (m_autoRestart && m_restartCount < MAX_RESTART) {
        m_restartCount++;
        int delayMs = nextRestartDelayMs();
        ltInfo(LT_LOG_IPC()) << "Auto-restarting backend (attempt" << m_restartCount
                             << "/" << MAX_RESTART << ") in" << delayMs << "ms...";
        QTimer::singleShot(delayMs, this, [this]() {
            if (m_autoRestart) {
                tryStartBackend();
            }
        });
    } else if (m_restartCount >= MAX_RESTART) {
        ltError(LT_LOG_IPC()) << "Max restart attempts reached, giving up";
        emit backendError(QStringLiteral("Python 后端多次重启失败，请检查 Python 环境配置"));
    }
}

void IpcClient::onBackendErrorOccurred(QProcess::ProcessError error)
{
    ltError(LT_LOG_IPC()) << "Backend process error:" << error;

    // 启动失败也走统一的重试/退避路径（FailedToStart 不会再触发 finished 信号）
    if (error == QProcess::FailedToStart) {
        if (m_connected) {
            m_connected = false;
            emit connectedChanged();
        }
        m_stableTimer->stop();
        m_watchdog->stop();

        if (m_autoRestart && m_restartCount < MAX_RESTART) {
            m_restartCount++;
            int delayMs = nextRestartDelayMs();
            ltInfo(LT_LOG_IPC()) << "Retry backend start (attempt" << m_restartCount
                                 << "/" << MAX_RESTART << ") in" << delayMs << "ms...";
            QTimer::singleShot(delayMs, this, [this]() {
                if (m_autoRestart) {
                    tryStartBackend();
                }
            });
        } else if (m_restartCount >= MAX_RESTART) {
            ltError(LT_LOG_IPC()) << "Max restart attempts reached after FailedToStart, giving up";
            emit backendError(QStringLiteral("Python 后端多次启动失败，请检查 Python 环境配置"));
            return;
        }
    }

    emit backendError(QStringLiteral("Backend process error: %1").arg(static_cast<int>(error)));
}

void IpcClient::tryStartBackend()
{
    ltInfo(LT_LOG_IPC()) << "tryStartBackend";
    // 自动重试不清零重启计数，继续按指数退避累计；稳定运行计时器到期才清零
    launchBackend();
}

// ============================================================================
// Windows Job Object：进程树治理
// ============================================================================

void IpcClient::ensureJobObject()
{
#ifdef Q_OS_WIN
    if (m_jobHandle) return;

    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (!job) {
        ltError(LT_LOG_IPC()) << "CreateJobObject failed:" << GetLastError();
        return;
    }

    // 作业句柄关闭时强制终止作业内全部进程，避免训练等孙进程成为孤儿
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {};
    info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof(info))) {
        ltError(LT_LOG_IPC()) << "SetInformationJobObject failed:" << GetLastError();
        CloseHandle(job);
        return;
    }

    m_jobHandle = job;
    ltDebug(LT_LOG_IPC()) << "Job object created with KILL_ON_JOB_CLOSE";
#endif
}

void IpcClient::assignProcessToJob()
{
#ifdef Q_OS_WIN
    if (!m_jobHandle || !m_process) return;

    qint64 pid = m_process->processId();
    if (pid <= 0) return;

    // 需要 SET_QUOTA + TERMINATE 权限才能放入作业
    HANDLE hProcess = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION,
                                  FALSE, static_cast<DWORD>(pid));
    if (!hProcess) {
        ltError(LT_LOG_IPC()) << "OpenProcess failed for pid" << pid << "err:" << GetLastError();
        return;
    }

    if (!AssignProcessToJobObject(static_cast<HANDLE>(m_jobHandle), hProcess)) {
        // 父进程已处于不支持嵌套的作业中时会失败，此时退化为仅 kill 直接子进程
        ltWarning(LT_LOG_IPC()) << "AssignProcessToJobObject failed:" << GetLastError()
                                 << "（进程树终止将退化为仅杀直接子进程）";
    } else {
        ltDebug(LT_LOG_IPC()) << "Backend pid" << pid << "assigned to job object";
    }
    CloseHandle(hProcess);
#else
    Q_UNUSED(m_jobHandle);
#endif
}

void IpcClient::killProcessTree()
{
#ifdef Q_OS_WIN
    if (m_jobHandle) {
        // 终止作业内整棵进程树（python 及其派生的训练/导出子进程）
        if (!TerminateJobObject(static_cast<HANDLE>(m_jobHandle), 1)) {
            ltWarning(LT_LOG_IPC()) << "TerminateJobObject failed:" << GetLastError();
        } else {
            ltInfo(LT_LOG_IPC()) << "Process tree terminated via job object";
        }
    }
#endif
    // 作业不可用时的兜底：仍杀直接子进程
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
    }
}

void IpcClient::closeJobObject()
{
#ifdef Q_OS_WIN
    if (m_jobHandle) {
        // KILL_ON_JOB_CLOSE：关闭句柄时系统会终止作业内残留进程
        CloseHandle(static_cast<HANDLE>(m_jobHandle));
        m_jobHandle = nullptr;
    }
#endif
}
