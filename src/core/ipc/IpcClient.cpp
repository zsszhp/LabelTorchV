#include "IpcClient.h"
#include "IpcProtocol.h"
#include "utils/Log.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonDocument>
#include <QProcessEnvironment>
#include <QRandomGenerator>

IpcClient::IpcClient(QObject *parent)
    : QObject(parent)
{
    ltTrace(LT_LOG_IPC()) << "IpcClient created";

    m_watchdog = new QTimer(this);
    m_watchdog->setInterval(5000);
    connect(m_watchdog, &QTimer::timeout, this, [this]() {
        if (m_connected) {
            sendRequest("environment.check", {});
        }
    });
}

IpcClient::~IpcClient()
{
    stopBackend();
}

bool IpcClient::connected() const
{
    return m_connected;
}

void IpcClient::startBackend(const QString &pythonPath, const QString &scriptPath)
{
    ltInfo(LT_LOG_IPC()) << "startBackend python=" << pythonPath << "script=" << scriptPath;

    if (m_process && m_process->state() != QProcess::NotRunning) {
        ltWarning(LT_LOG_IPC()) << "Backend already running";
        return;
    }

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::ForwardedErrorChannel);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QStringList candidateDirs = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/../backend"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../../../backend"),
        QStringLiteral("F:/project/my/LabelTorchV/backend"),
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

    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &IpcClient::onBackendReadyRead);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &IpcClient::onBackendFinished);
    connect(m_process, &QProcess::errorOccurred,
            this, &IpcClient::onBackendErrorOccurred);

    QString py = pythonPath.isEmpty() ? QStringLiteral("python") : pythonPath;
    QString script = scriptPath.isEmpty()
        ? QStringLiteral("-m") : scriptPath;

    if (script == QStringLiteral("-m")) {
        m_process->start(py, {QStringLiteral("-m"), QStringLiteral("labeltorch_backend.server")});
    } else {
        m_process->start(py, {script});
    }

    if (m_process->waitForStarted(5000)) {
        ltInfo(LT_LOG_IPC()) << "Backend process started, pid=" << m_process->processId();
        m_connected = true;
        m_restartAttempts = 0;
        emit connectedChanged();
        m_watchdog->start();
    } else {
        ltError(LT_LOG_IPC()) << "Failed to start backend:" << m_process->errorString();
        emit backendError(QStringLiteral("Failed to start Python backend: %1").arg(m_process->errorString()));
    }
}

void IpcClient::stopBackend()
{
    ltInfo(LT_LOG_IPC()) << "stopBackend";

    m_watchdog->stop();
    m_autoRestart = false;

    if (m_process && m_process->state() != QProcess::NotRunning) {
        sendRequest("shutdown", {});
        m_process->waitForFinished(3000);
        if (m_process->state() != QProcess::NotRunning) {
            m_process->kill();
            m_process->waitForFinished(1000);
        }
    }

    if (m_connected) {
        m_connected = false;
        emit connectedChanged();
    }
}

void IpcClient::sendRequest(const QString &command, const QJsonObject &payload)
{
    if (!m_process || m_process->state() != QProcess::Running) {
        ltWarning(LT_LOG_IPC()) << "Cannot send request, backend not running";
        return;
    }

    QString requestId = QStringLiteral("req_%1_%2")
        .arg(++m_requestCounter)
        .arg(QRandomGenerator::global()->bounded(10000));

    m_pendingCommands[requestId] = command;

    QJsonObject request = IpcProtocol::createRequest(requestId, command, payload);
    QByteArray data = QJsonDocument(request).toJson(QJsonDocument::Compact) + "\n";

    ltDebug(LT_LOG_IPC()) << "sendRequest id=" << requestId << "command=" << command;
    m_process->write(data);
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
                response[QStringLiteral("command")] = m_pendingCommands[requestId];
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

void IpcClient::onBackendFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    ltWarning(LT_LOG_IPC()) << "Backend finished exitCode=" << exitCode
                             << "exitStatus=" << exitStatus;

    m_connected = false;
    emit connectedChanged();
    m_watchdog->stop();

    if (m_autoRestart && m_restartAttempts < MAX_RESTART_ATTEMPTS) {
        m_restartAttempts++;
        ltInfo(LT_LOG_IPC()) << "Auto-restarting backend in 3s... (attempt"
                             << m_restartAttempts << "/" << MAX_RESTART_ATTEMPTS << ")";
        QTimer::singleShot(3000, this, [this]() {
            if (m_autoRestart && m_restartAttempts <= MAX_RESTART_ATTEMPTS) {
                tryStartBackend();
            }
        });
    } else if (m_autoRestart) {
        ltError(LT_LOG_IPC()) << "Max restart attempts reached, giving up";
        m_autoRestart = false;
        emit backendError(QStringLiteral("Backend failed to start after %1 attempts").arg(MAX_RESTART_ATTEMPTS));
    }
}

void IpcClient::onBackendErrorOccurred(QProcess::ProcessError error)
{
    ltError(LT_LOG_IPC()) << "Backend process error:" << error;
    emit backendError(QStringLiteral("Backend process error: %1").arg(static_cast<int>(error)));
}

void IpcClient::tryStartBackend()
{
    ltInfo(LT_LOG_IPC()) << "tryStartBackend";
    startBackend();
}
