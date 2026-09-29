#ifndef TESTINGSERVICE_H
#define TESTINGSERVICE_H

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class IpcClient;
class ModelRegistry;

/**
 * @brief 测试服务 - 模型评估生命周期管理
 *
 * 负责创建/启动/停止测试任务，监听IPC事件更新状态
 */
class TestingService : public QObject
{
    Q_OBJECT

public:
    explicit TestingService(QObject *parent = nullptr);

    // 依赖注入
    void setIpcClient(IpcClient *client);
    void setModelRegistry(ModelRegistry *registry);

    // QML可调用方法
    Q_INVOKABLE QString createTestTask(const QString &projectId,
                                        const QString &modelVersionId,
                                        const QString &snapshotId,
                                        const QString &config);
    Q_INVOKABLE bool startTestTask(const QString &taskId);
    Q_INVOKABLE bool stopTestTask(const QString &taskId);
    Q_INVOKABLE QVariantMap getTestResults(const QString &taskId);
    Q_INVOKABLE QVariantMap getConfusionMatrix(const QString &taskId);
    Q_INVOKABLE QVariantList getPRCurveData(const QString &taskId);
    Q_INVOKABLE bool deleteTestTask(const QString &taskId);
    Q_INVOKABLE bool updateTestTaskStatus(const QString &taskId, const QString &status);
    Q_INVOKABLE int reconcileStaleTasks();

    /**
     * @brief 获取逐类指标（P1-18）。
     *
     * 从 metrics_json.per_class 解析，字段稳定：
     * classIndex, className, precision, recall, f1, ap50, ap,
     * support, fn_count(漏检), fp_count(超检)
     * 返回按 fn_count 降序（漏检最多在前）。
     *
     * @param taskId 测试任务 ID。
     */
    Q_INVOKABLE QVariantList getPerClassMetrics(const QString &taskId);

    /**
     * @brief 获取部署阈值推荐（P1-17）。
     *
     * 字段稳定：recommended_conf, method(max_f1|cost_weighted),
     * f1_at_threshold, precision_at_threshold, recall_at_threshold,
     * expected_miss_rate, expected_false_alarm_rate
     *
     * @param taskId 测试任务 ID。
     */
    Q_INVOKABLE QVariantMap getThresholdRecommendation(const QString &taskId);

    /**
     * @brief 获取 Go/No-Go 结论（P1-17，供 UI 结论卡）。
     *
     * 字段稳定：decision(go|no_go), gate_metric, gate_threshold, gate_value,
     * gate_passed, regression_vs_baseline, baseline_value, current_value, reasons
     *
     * @param taskId 测试任务 ID。
     */
    Q_INVOKABLE QVariantMap getGoNoGo(const QString &taskId);

    /**
     * @brief 获取逐类 FP/FN 难例队列（P1-18，接入主动学习队列数据）。
     *
     * 从 active_learning_items 表按类别聚合 false-positive / false-negative
     * 样本，与 getPerClassMetrics 的 classIndex 对齐。
     *
     * @param taskId 测试任务 ID（用于解析项目/数据集上下文，可为空）。
     * @return QVariantList，元素：classIndex, className, fnCount, fpCount,
     *         fnSamples([{samplePath, reason, confidence}]), fpSamples([...])
     */
    Q_INVOKABLE QVariantList getPerClassErrorQueue(const QString &taskId = QString());

signals:
    void testTaskStatusChanged(const QString &taskId, const QString &status);
    void testProgress(const QString &taskId, int current, int total, const QVariantMap &metrics);
    void testLog(const QString &taskId, const QString &logLine);

private:
    void handleTestingEvent(const QVariantMap &event);
    void onResponseReceived(const QJsonObject &response);

    /// 从 metrics_json 解析嵌套字段（per_class / threshold_recommendation / go_no_go）
    static QVariantMap parseMetricsJson(const QString &metricsJson);

    IpcClient *m_ipcClient = nullptr;
    ModelRegistry *m_modelRegistry = nullptr;
};

#endif // TESTINGSERVICE_H
