#ifndef IPCCLIENT_H
#define IPCCLIENT_H

#include <QObject>
#include <QProcess>
#include <QJsonObject>

/**
 * @brief IPC客户端
 *
 * 通过QProcess启动Python后端进程，使用stdin/stdout JSON-RPC通信
 */
class IpcClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)

public:
    explicit IpcClient(QObject *parent = nullptr);
    ~IpcClient() override;

    bool connected() const { return m_connected; }

    void startBackend(const QString &pythonExec, const QString &serverScript);
    void stopBackend();

    void sendRequest(const QString &command, const QJsonObject &payload = {});

signals:
    void connectedChanged();
    void responseReceived(const QJsonObject &response);
    void eventReceived(const QJsonObject &event);
    void backendError(const QString &error);

private slots:
    void onBackendOutput();
    void onBackendError();
    void onBackendFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void processMessage(const QJsonObject &msg);
    void checkHeartbeat();

    QProcess *m_process = nullptr;
    bool m_connected = false;
    QString m_requestIdPrefix;
    int m_requestCounter = 0;
};

#endif // IPCCLIENT_H
