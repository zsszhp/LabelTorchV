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
    void onBackendStarted();
    void onBackendReadyRead();
    void onBackendFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onBackendErrorOccurred(QProcess::ProcessError error);

private:
    void processMessage(const QJsonObject &msg);
    /// 实际拉起后端进程（异步，不重置重启计数）
    void launchBackend();
    void tryStartBackend();
    /// 扫描并清理超时请求（A2：请求超时机制）
    void cleanupTimedOutRequests();
    /// 计算指数退避重试延迟：1s/2s/4s/8s，上限 30s
    int nextRestartDelayMs() const;

    // ---- Windows Job Object：把子进程树纳入作业，退出时整棵终止 ----
    /// 创建作业对象并设置 KILL_ON_JOB_CLOSE（仅首次生效）
    void ensureJobObject();
    /// 将刚启动的后端进程放入作业对象
    void assignProcessToJob();
    /// 终止作业内整棵进程树（含训练等孙进程），避免孤儿
    void killProcessTree();
    /// 关闭作业句柄（KILL_ON_JOB_CLOSE 兜底杀残留进程）
    void closeJobObject();

    QProcess *m_process = nullptr;
    bool m_connected = false;
    int m_requestCounter = 0;
    QMap<QString, PendingCommand> m_pendingCommands;  ///< 待响应请求映射（含时间戳，用于超时清理）
    QTimer *m_watchdog = nullptr;
    /// 稳定运行计时器：后端连续运行满 30s 才清零重启计数，防止"一起来就崩"无限重启
    QTimer *m_stableTimer = nullptr;
    bool m_autoRestart = true;
    int m_restartCount = 0;
    static constexpr int MAX_RESTART = 5;
    static constexpr int REQUEST_TIMEOUT_MS = 30000;  ///< 请求超时阈值（30 秒）
    static constexpr int STABLE_RUN_MS = 30000;       ///< 稳定运行阈值（30 秒）
    static constexpr int STOP_GRACE_MS = 3000;        ///< 停止时优雅退出等待（毫秒）
    QString m_lastPythonPath;
    QString m_lastScriptPath;
    /// 最近一次发出的请求命令（后端异常退出时写入日志，定位"点了什么之后后端挂了"）
    QString m_lastRequestCommand;
    /// 后端本次启动时刻（毫秒时间戳），用于崩溃日志的运行时长
    qint64 m_backendStartedMs = 0;
    /// Windows 作业句柄（HANDLE），非 Windows 平台保持 nullptr
    void *m_jobHandle = nullptr;
};

#endif // IPCCLIENT_H
