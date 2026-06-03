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

signals:
    void testTaskStatusChanged(const QString &taskId, const QString &status);
    void testProgress(const QString &taskId, int current, int total, const QVariantMap &metrics);
    void testLog(const QString &taskId, const QString &logLine);

private:
    void handleTestingEvent(const QVariantMap &event);
    void onResponseReceived(const QJsonObject &response);

    IpcClient *m_ipcClient = nullptr;
    ModelRegistry *m_modelRegistry = nullptr;
};

#endif // TESTINGSERVICE_H
