#ifndef TRAININGSERVICE_H
#define TRAININGSERVICE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include "MetricService.h"

class IpcClient;
class ModelRegistry;

class TrainingService : public QObject
{
    Q_OBJECT

public:
    explicit TrainingService(QObject *parent = nullptr);

    void setIpcClient(IpcClient *client);
    void setModelRegistry(ModelRegistry *registry);

    Q_INVOKABLE void handleTrainingEvent(const QVariantMap &event);

    /**
     * @brief Create a training run in draft status.
     * @param projectId The project ID.
     * @param snapshotId The dataset snapshot ID.
     * @param config Training configuration as JSON string.
     * @return Run ID on success, empty string on failure.
     */
    Q_INVOKABLE QString createRun(const QString &projectId,
                                   const QString &snapshotId,
                                   const QString &config);

    /**
     * @brief Start a training run. Transitions draft -> running.
     * @param runId The training run ID.
     * @return true on success, false on failure.
     */
    Q_INVOKABLE bool startTraining(const QString &runId);

    /**
     * @brief Stop a running training. Sends train.stop via IpcClient.
     * @param runId The training run ID.
     * @return true on success, false on failure.
     */
    Q_INVOKABLE bool stopTraining(const QString &runId);

    /**
     * @brief List training runs for a project.
     * @param projectId The project ID.
     * @return QVariantList of QVariantMap with run fields.
     */
    Q_INVOKABLE QVariantList listRuns(const QString &projectId);

    /**
     * @brief Get details of a specific training run.
     * @param runId The training run ID.
     * @return QVariantMap with run fields, or empty on not found.
     */
    Q_INVOKABLE QVariantMap getRun(const QString &runId);

    /**
     * @brief Delete a training run. Only allowed if draft/cancelled/failed.
     * @param runId The training run ID.
     * @return true on success, false on failure.
     */
    Q_INVOKABLE bool deleteRun(const QString &runId);

    /**
     * @brief Update the status of a training run.
     * @param runId The training run ID.
     * @param status The new status string.
     * @return true on success, false on failure.
     */
    Q_INVOKABLE bool updateRunStatus(const QString &runId, const QString &status);

    /**
     * @brief List available training adapters from the backend.
     * @return QStringList of adapter names.
     */
    Q_INVOKABLE QStringList listAdapters();

    /**
     * @brief 冷启动自检：修正残留的 running / preparing 状态任务
     *
     * 应用启动时调用，将上次异常退出遗留的 running / preparing 状态
     * 训练任务修正为 stopped，防止"幽灵运行"任务永远无法完成。
     *
     * @return 修正的记录数
     */
    int reconcileStaleRuns();

signals:
    /**
     * @brief 训练运行状态变更信号
     * @param runId 运行ID
     * @param status 新状态
     */
    void runStatusChanged(const QString &runId, const QString &status);

    /**
     * @brief 训练进度更新信号（每个epoch触发）
     * @param runId 运行ID
     * @param epoch 当前epoch
     * @param totalEpochs 总epoch数
     * @param loss 当前loss值
     * @param metrics 当前epoch指标
     */
    void trainingProgress(const QString &runId, int epoch, int totalEpochs,
                           double loss, const QVariantMap &metrics);

    /**
     * @brief 训练日志信号（实时日志行）
     * @param runId 运行ID
     * @param logLine 日志文本行
     */
    void trainingLog(const QString &runId, const QString &logLine);

    /**
     * @brief 训练警告信号
     * @param runId 运行ID
     * @param message 警告消息
     */
    void trainingWarning(const QString &runId, const QString &message);

private:
    void onResponseReceived(const QJsonObject &response);

    IpcClient *m_ipcClient = nullptr;
    ModelRegistry *m_modelRegistry = nullptr;
    MetricService *m_metricService = nullptr;
    QStringList m_adapters = { QStringLiteral("ultralytics"), QStringLiteral("anomalib") };
};

#endif // TRAININGSERVICE_H
