#include "IpcClient.h"
#include "IpcProtocol.h"
#include <QJsonDocument>
#include <QDebug>

IpcClient::IpcClient(QObject *parent)
    : QObject(parent)
{
}

IpcClient::~IpcClient()
{
    stopBackend();
}

void IpcClient::startBackend(const QString &pythonExec, const QString &serverScript)
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        qWarning() << "Backend already running";
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

    qDebug() << "Starting Python backend:" << pythonExec << serverScript;
}

void IpcClient::stopBackend()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        // 发送关闭命令
        sendRequest("shutdown");
        m_process->waitForFinished(5000);
        if (m_process->state() != QProcess::NotRunning) {
            m_process->kill();
        }
    }
}

void IpcClient::sendRequest(const QString &command, const QJsonObject &payload)
{
    if (!m_process || m_process->state() != QProcess::Running) {
        qWarning() << "Backend not running, cannot send request:" << command;
        return;
    }

    const QString requestId = QString("req_%1").arg(++m_requestCounter);
    QJsonObject request = IpcProtocol::createRequest(requestId, command, payload);
    QJsonDocument doc(request);
    QByteArray data = doc.toJson(QJsonDocument::Compact) + "\n";

    m_process->write(data);
    qDebug() << "IPC request sent:" << command;
}

void IpcClient::onBackendOutput()
{
    while (m_process->canReadLine()) {
        QByteArray line = m_process->readLine().trimmed();
        if (line.isEmpty()) continue;

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(line, &error);
        if (error.error != QJsonParseError::NoError) {
            qWarning() << "IPC parse error:" << error.errorString();
            continue;
        }

        processMessage(doc.object());
    }
}

void IpcClient::onBackendError()
{
    QString errorMsg = m_process->readAllStandardError();
    if (!errorMsg.trimmed().isEmpty()) {
        qWarning() << "Backend stderr:" << errorMsg;
    }
}

void IpcClient::onBackendFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_connected = false;
    emit connectedChanged();

    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        emit backendError(QString("Backend process exited with code %1").arg(exitCode));
    }

    qDebug() << "Backend process finished:" << exitCode << exitStatus;
}

void IpcClient::processMessage(const QJsonObject &msg)
{
    QString type = msg["type"].toString();

    if (type == IpcProtocol::RESPONSE) {
        bool wasConnected = m_connected;
        if (!m_connected && msg["success"].toBool()) {
            m_connected = true;
            if (!wasConnected) emit connectedChanged();
        }
        emit responseReceived(msg);
    } else if (type == IpcProtocol::EVENT) {
        emit eventReceived(msg);
    }
}

void IpcClient::checkHeartbeat()
{
    // TODO: 实现心跳检测
}
