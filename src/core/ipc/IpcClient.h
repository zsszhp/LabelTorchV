#ifndef IPCCLIENT_H
#define IPCCLIENT_H

#include <QObject>
#include <QProcess>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QTimer>

class IpcClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)

public:
    explicit IpcClient(QObject *parent = nullptr);
    ~IpcClient();

    bool connected() const;

    Q_INVOKABLE void startBackend(const QString &pythonPath = QString(),
                                   const QString &scriptPath = QString());
    Q_INVOKABLE void stopBackend();
    Q_INVOKABLE void sendRequest(const QString &command, const QJsonObject &payload = {});

signals:
    void connectedChanged();
    void responseReceived(const QJsonObject &response);
    void eventReceived(const QJsonObject &event);
    void backendError(const QString &error);

private slots:
    void onBackendReadyRead();
    void onBackendFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onBackendErrorOccurred(QProcess::ProcessError error);

private:
    void processMessage(const QJsonObject &msg);
    void tryStartBackend();

    QProcess *m_process = nullptr;
    bool m_connected = false;
    int m_requestCounter = 0;
    QMap<QString, QString> m_pendingCommands;
    QTimer *m_watchdog = nullptr;
    bool m_autoRestart = true;
    int m_restartAttempts = 0;
    static constexpr int MAX_RESTART_ATTEMPTS = 5;
};

#endif // IPCCLIENT_H
