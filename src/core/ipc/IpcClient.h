#ifndef IPCCLIENT_H
#define IPCCLIENT_H

#include <QObject>
#include <QProcess>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QTimer>
#include <QDateTime>

/// 待响应请求条目（含命令与发起时间戳，用于超时清理）
struct PendingCommand {
    QString command;
    QDateTime startTime;
};

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
    /// 发送 IPC 请求，返回 requestId（失败返回空字符串），调用方可用于关联响应
    Q_INVOKABLE QString sendRequest(const QString &command, const QJsonObject &payload = {});

signals:
    void connectedChanged();
    void responseReceived(const QJsonObject &response);
    void eventReceived(const QJsonObject &event);
    void backendError(const QString &error);
    /// 请求超时信号（超过 30 秒未收到响应）
    void requestTimeout(const QString &requestId, const QString &command);

private slots:
    void onBackendReadyRead();
    void onBackendFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onBackendErrorOccurred(QProcess::ProcessError error);

private:
    void processMessage(const QJsonObject &msg);
    void tryStartBackend();
    /// 扫描并清理超时请求（A2：请求超时机制）
    void cleanupTimedOutRequests();

    QProcess *m_process = nullptr;
    bool m_connected = false;
    int m_requestCounter = 0;
    QMap<QString, PendingCommand> m_pendingCommands;  ///< 待响应请求映射（含时间戳，用于超时清理）
    QTimer *m_watchdog = nullptr;
    bool m_autoRestart = true;
    int m_restartCount = 0;
    static constexpr int MAX_RESTART = 5;
    static constexpr int REQUEST_TIMEOUT_MS = 30000;  ///< 请求超时阈值（30 秒）
    QString m_lastPythonPath;
    QString m_lastScriptPath;
};

#endif // IPCCLIENT_H
