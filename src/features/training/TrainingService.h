#ifndef TRAININGSERVICE_H
#define TRAININGSERVICE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class IpcClient;

class TrainingService : public QObject
{
    Q_OBJECT

public:
    explicit TrainingService(QObject *parent = nullptr);

    void setIpcClient(IpcClient *client);

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

signals:
    /**
     * @brief Emitted when a training run's status changes.
     * @param runId The run ID.
     * @param status The new status.
     */
    void runStatusChanged(const QString &runId, const QString &status);

private:
    IpcClient *m_ipcClient = nullptr;
};

#endif // TRAININGSERVICE_H
