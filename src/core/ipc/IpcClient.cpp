#include "IpcClient.h"
#include "IpcProtocol.h"
#include "utils/Log.h"
#include <QJsonDocument>

IpcClient::IpcClient(QObject *parent)
    : QObject(parent)
{
    ltTrace(LT_LOG_IPC()) << "IpcClient constructed";
}

IpcClient::~IpcClient()
{
    stopBackend();
}

void IpcClient::startBackend(const QString &pythonExec, const QString &serverScript)
{
    ltTrace(LT_LOG_IPC()) << "startBackend pythonExec=" << pythonExec << "script=" << serverScript;

    if (m_process && m_process->state() != QProcess::NotRunning) {
        ltWarning(LT_LOG_IPC()) << "Backend already running";
        return;
    }

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(m_process, &QProcess::readyReadStandardOutput, this, &IpcClient::onBackendOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &IpcClient::onBackendError);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &IpcClient::onBackendFinished);

    QStringList args;
    args << serverScript;
    m_process->start(pythonExec, args);

    // 启动心跳定时器（30秒间隔）
    if (!m_heartbeatTimer) {
        m_heartbeatTimer = new QTimer(this);
        connect(m_heartbeatTimer, &QTimer::timeout, this, &IpcClient::checkHeartbeat);
    }
    m_heartbeatPending = false;
    m_heartbeatTimer->start(30000);

    ltInfo(LT_LOG_IPC()) << "Starting Python backend:" << pythonExec << serverScript;
}

void IpcClient::stopBackend()
{
    ltTrace(LT_LOG_IPC()) << "stopBackend";

    // 停止心跳定时器
    if (m_heartbeatTimer) {
        m_heartbeatTimer->stop();
        m_heartbeatPending = false;
    }

    if (m_process && m_process->state() != QProcess::NotRunning) {
        // 发送关闭命令
        sendRequest("shutdown");
        m_process->waitForFinished(5000);
        if (m_process->state() != QProcess::NotRunning) {
            m_process->kill();
            ltWarning(LT_LOG_IPC()) << "Backend process killed after timeout";
        }
    }
}

void IpcClient::sendRequest(const QString &command, const QJsonObject &payload)
{
    ltTrace(LT_LOG_IPC()) << "sendRequest command=" << command;

    if (!m_process || m_process->state() != QProcess::Running) {
        ltWarning(LT_LOG_IPC()) << "Backend not running, cannot send request:" << command;
        return;
    }

    const QString requestId = QString("req_%1").arg(++m_requestCounter);
    QJsonObject request = IpcProtocol::createRequest(requestId, command, payload);
    QJsonDocument doc(request);
    QByteArray data = doc.toJson(QJsonDocument::Compact) + "\n";

    m_process->write(data);
    ltDebug(LT_LOG_IPC()) << "IPC request sent:" << command << "id=" << requestId;
}

void IpcClient::onBackendOutput()
{
    while (m_process->canReadLine()) {
        QByteArray line = m_process->readLine().trimmed();
        if (line.isEmpty()) continue;

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(line, &error);
        if (error.error != QJsonParseError::NoError) {
            ltWarning(LT_LOG_IPC()) << "IPC parse error:" << error.errorString();
            continue;
        }

        processMessage(doc.object());
    }
}

void IpcClient::onBackendError()
{
    QString errorMsg = m_process->readAllStandardError();
    if (!errorMsg.trimmed().isEmpty()) {
        ltWarning(LT_LOG_IPC()) << "Backend stderr:" << errorMsg;
    }
}

void IpcClient::onBackendFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    ltTrace(LT_LOG_IPC()) << "onBackendFinished exitCode=" << exitCode << "exitStatus=" << exitStatus;

    // 停止心跳定时器
    if (m_heartbeatTimer) {
        m_heartbeatTimer->stop();
        m_heartbeatPending = false;
    }

    m_connected = false;
    emit connectedChanged();

    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        ltError(LT_LOG_IPC()) << "Backend process exited abnormally code=" << exitCode;
        emit backendError(QString("Backend process exited with code %1").arg(exitCode));
    }

    ltInfo(LT_LOG_IPC()) << "Backend process finished:" << exitCode << exitStatus;
}

void IpcClient::processMessage(const QJsonObject &msg)
{
    QString type = msg["type"].toString();

    if (type == IpcProtocol::RESPONSE) {
        // 心跳响应：重置 pending 标志
        if (m_heartbeatPending) {
            m_heartbeatPending = false;
        }

        bool wasConnected = m_connected;
        if (!m_connected && msg["success"].toBool()) {
            m_connected = true;
            if (!wasConnected) {
                ltInfo(LT_LOG_IPC()) << "Backend connected";
                emit connectedChanged();
            }
        }
        emit responseReceived(msg);
    } else if (type == IpcProtocol::EVENT) {
        ltDebug(LT_LOG_IPC()) << "Event received:" << msg["event"].toString();
        emit eventReceived(msg);
    }
}

void IpcClient::checkHeartbeat()
{
    if (!m_process || m_process->state() != QProcess::Running) {
        return;
    }

    // 如果上次心跳尚未收到响应，判定后端无响应
    if (m_heartbeatPending) {
        ltWarning(LT_LOG_IPC()) << "Heartbeat timeout: backend not responding";
        m_connected = false;
        emit connectedChanged();
        return;
    }

    // 发送心跳 ping 请求
    m_heartbeatPending = true;
    sendRequest("ping");
}
