#include "TrainingService.h"
#include "Database.h"
#include "ipc/IpcClient.h"
#include "utils/Log.h"
#include "SnapshotService.h"
#include "MetricService.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <QDateTime>
#include <QtConcurrent>

TrainingService::TrainingService(QObject *parent) : QObject(parent)
{
    ltTrace(LT_LOG_TRAINING()) << "parent=" << parent;
}

void TrainingService::setIpcClient(IpcClient *client)
{
    ltTrace(LT_LOG_TRAINING()) << "client=" << client;
    m_ipcClient = client;

    if (m_ipcClient) {
        connect(m_ipcClient, &IpcClient::eventReceived, this, [this](const QJsonObject &event) {
            QString eventType = event[QStringLiteral("event_type")].toString();
            if (eventType.startsWith(QStringLiteral("task."))) {
                QVariantMap ev;
                ev[QStringLiteral("event_type")] = eventType;
                ev[QStringLiteral("task_id")] = event[QStringLiteral("task_id")].toString();
                ev[QStringLiteral("payload")] = event[QStringLiteral("payload")].toVariant().toMap();
                handleTrainingEvent(ev);
            }
        });
    }
}

void TrainingService::handleTrainingEvent(const QVariantMap &event)
{
    QString eventType = event[QStringLiteral("event_type")].toString();
    QString taskId = event[QStringLiteral("task_id")].toString();
    QVariantMap payload = event[QStringLiteral("payload")].toMap();

    ltDebug(LT_LOG_TRAINING()) << "handleTrainingEvent type=" << eventType << "taskId=" << taskId;

    if (eventType == QStringLiteral("task.succeeded")) {
        updateRunStatus(taskId, QStringLiteral("succeeded"));

        QString bestWeight = payload[QStringLiteral("best_weight_path")].toString();
        QString lastWeight = payload[QStringLiteral("last_weight_path")].toString();
        QVariantMap metrics = payload[QStringLiteral("metrics")].toMap();

        if (!bestWeight.isEmpty()) {
            QJsonObject metricsObj = QJsonObject::fromVariantMap(metrics);
            QString metricsJson = QString::fromUtf8(QJsonDocument(metricsObj).toJson(QJsonDocument::Compact));

            QSqlQuery runQuery(Database::instance().database());
            runQuery.prepare("SELECT project_id FROM training_runs WHERE id = ?");
            runQuery.addBindValue(taskId);
            if (runQuery.exec() && runQuery.next()) {
                QString projectId = runQuery.value(0).toString();

                QSqlQuery versionQuery(Database::instance().database());
                QString versionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
                versionQuery.prepare(
                    "INSERT INTO model_versions (id, run_id, best_weight_path, last_weight_path, "
                    "metrics_snapshot_json) VALUES (?, ?, ?, ?, ?)"
                );
                versionQuery.addBindValue(versionId);
                versionQuery.addBindValue(taskId);
                versionQuery.addBindValue(bestWeight);
                versionQuery.addBindValue(lastWeight);
                versionQuery.addBindValue(metricsJson);

                if (versionQuery.exec()) {
                    ltInfo(LT_LOG_TRAINING()) << "Auto-registered model version:" << versionId
                                              << "for run:" << taskId;
                } else {
                    ltError(LT_LOG_TRAINING()) << "Failed to register model version:"
                                               << versionQuery.lastError().text();
                }
            }
        }
    } else if (eventType == QStringLiteral("task.stopped")) {
        updateRunStatus(taskId, QStringLiteral("stopped"));
    } else if (eventType == QStringLiteral("task.failed")) {
        updateRunStatus(taskId, QStringLiteral("failed"));
    } else if (eventType == QStringLiteral("task.started")) {
        updateRunStatus(taskId, QStringLiteral("running"));
    } else if (eventType == QStringLiteral("task.progress")) {
        // 训练进度事件，转发给QML层
        int epoch = payload[QStringLiteral("epoch")].toInt();
        QVariantMap metrics = payload[QStringLiteral("metrics")].toMap();

        emit trainingProgress(taskId,
                               epoch,
                               payload[QStringLiteral("total_epochs")].toInt(),
                               payload[QStringLiteral("loss")].toDouble(),
                               metrics);

        // 持久化指标到 run_metrics 表
        QVariantMap allMetrics = metrics;
        double lossVal = payload[QStringLiteral("loss")].toDouble();
        if (lossVal != 0.0 || payload.contains(QStringLiteral("loss"))) {
            allMetrics[QStringLiteral("loss")] = lossVal;
        }
        if (!allMetrics.isEmpty() && epoch > 0) {
            MetricService metricService;
            metricService.storeEpochMetrics(taskId, epoch, allMetrics);
        }
    }
}

QString TrainingService::createRun(const QString &projectId,
                                    const QString &snapshotId,
                                    const QString &config)
{
    ltTrace(LT_LOG_TRAINING()) << "projectId=" << projectId << "snapshotId=" << snapshotId;

    auto db = Database::instance().database();
    if (!db.isOpen()) return {};

    // Validate config is valid JSON
    QJsonParseError parseError;
    QJsonDocument configDoc = QJsonDocument::fromJson(config.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        ltWarning(LT_LOG_TRAINING()) << "Invalid config JSON:" << parseError.errorString();
        return {};
    }

    QString runId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString configJson = QString::fromUtf8(configDoc.toJson(QJsonDocument::Compact));

    QSqlQuery query(db);
    query.prepare(
        "INSERT INTO training_runs (id, project_id, snapshot_id, config_snapshot_json, status) "
        "VALUES (?, ?, ?, ?, 'draft')"
    );
    query.addBindValue(runId);
    query.addBindValue(projectId);
    query.addBindValue(snapshotId);
    query.addBindValue(configJson);

    if (!query.exec()) {
        ltError(LT_LOG_TRAINING()) << "Failed to create training run:" << query.lastError().text();
        return {};
    }

    ltInfo(LT_LOG_TRAINING()) << "Created training run:" << runId << "for project:" << projectId;
    return runId;
}

bool TrainingService::startTraining(const QString &runId)
{
    ltTrace(LT_LOG_TRAINING()) << "runId=" << runId;

    auto db = Database::instance().database();
    if (!db.isOpen()) return false;

    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT status, snapshot_id, project_id FROM training_runs WHERE id = ?");
    checkQuery.addBindValue(runId);
    if (!checkQuery.exec() || !checkQuery.next()) return false;

    QString currentStatus = checkQuery.value(0).toString();
    if (currentStatus != "draft") {
        ltWarning(LT_LOG_TRAINING()) << "Cannot start training run in status:" << currentStatus;
        return false;
    }

    QString snapshotId = checkQuery.value(1).toString();
    QString projectId = checkQuery.value(2).toString();

    QSqlQuery projectQuery(db);
    projectQuery.prepare("SELECT root_path FROM projects WHERE id = ?");
    projectQuery.addBindValue(projectId);
    if (!projectQuery.exec() || !projectQuery.next()) return false;
    QString projectRoot = projectQuery.value(0).toString();

    // 立即更新状态为"准备中"，防止用户重复点击
    {
        QSqlQuery updateQuery(db);
        updateQuery.prepare("UPDATE training_runs SET status = 'preparing', started_at = ? WHERE id = ?");
        updateQuery.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
        updateQuery.addBindValue(runId);
        if (!updateQuery.exec()) return false;
    }
    emit runStatusChanged(runId, "preparing");

    // 在后台线程准备物理快照目录（大量文件拷贝，不能在UI线程执行）
    QString capturedProjectRoot = projectRoot;
    QtConcurrent::run([this, runId, snapshotId, capturedProjectRoot]() {
        SnapshotService snapshotService;
        QString dataYamlPath = snapshotService.prepareSnapshotPhysicalDir(snapshotId);

        // 回到UI线程处理结果
        QMetaObject::invokeMethod(this, [this, runId, snapshotId, dataYamlPath, capturedProjectRoot]() {
            if (dataYamlPath.isEmpty()) {
                ltError(LT_LOG_TRAINING()) << "Failed to prepare snapshot directories for run:" << runId;
                updateRunStatus(runId, "failed");
                emit runStatusChanged(runId, "failed");
                return;
            }

            auto db = Database::instance().database();

            QJsonObject runtimeEnv;
            runtimeEnv["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
            runtimeEnv["python_version"] = "";
            runtimeEnv["ultralytics_version"] = "";
            runtimeEnv["torch_version"] = "";
            runtimeEnv["cuda_available"] = false;
            QString runtimeEnvJson = QString::fromUtf8(
                QJsonDocument(runtimeEnv).toJson(QJsonDocument::Compact));

            QSqlQuery updateQuery(db);
            updateQuery.prepare(
                "UPDATE training_runs SET status = 'running', runtime_env_snapshot_json = ? WHERE id = ?");
            updateQuery.addBindValue(runtimeEnvJson);
            updateQuery.addBindValue(runId);

            if (!updateQuery.exec()) {
                ltError(LT_LOG_TRAINING()) << "Failed to update training run status:" << updateQuery.lastError().text();
                return;
            }

            if (m_ipcClient) {
                QSqlQuery runQuery(db);
                runQuery.prepare("SELECT snapshot_id, config_snapshot_json FROM training_runs WHERE id = ?");
                runQuery.addBindValue(runId);
                if (runQuery.exec() && runQuery.next()) {
                    QJsonObject payload;
                    payload["run_id"] = runId;
                    payload["snapshot_id"] = runQuery.value(0).toString();

                    QJsonObject configObj = QJsonDocument::fromJson(
                        runQuery.value(1).toString().toUtf8()).object();

                    configObj["data_yaml"] = dataYamlPath;
                    configObj["project_dir"] = capturedProjectRoot + "/models";
                    configObj["run_name"] = runId;

                    payload["config"] = configObj;
                    m_ipcClient->sendRequest("train.start", payload);
                }
            }

            ltInfo(LT_LOG_TRAINING()) << "Training run started:" << runId;
            emit runStatusChanged(runId, "running");
        }, Qt::QueuedConnection);
    });

    return true;
}

bool TrainingService::stopTraining(const QString &runId)
{
    ltTrace(LT_LOG_TRAINING()) << "runId=" << runId;

    auto db = Database::instance().database();
    if (!db.isOpen()) return false;

    // Check current status - only running can be stopped
    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT status FROM training_runs WHERE id = ?");
    checkQuery.addBindValue(runId);
    if (!checkQuery.exec() || !checkQuery.next()) return false;

    QString currentStatus = checkQuery.value(0).toString();
    if (currentStatus != "running") {
        ltWarning(LT_LOG_TRAINING()) << "Cannot stop training run in status:" << currentStatus;
        return false;
    }

    // Send train.stop via IpcClient if available
    if (m_ipcClient) {
        QJsonObject payload;
        payload["run_id"] = runId;
        m_ipcClient->sendRequest("train.stop", payload);
    }

    // Update status to cancelled, set finished_at
    QSqlQuery updateQuery(db);
    updateQuery.prepare(
        "UPDATE training_runs SET status = 'cancelled', finished_at = ? WHERE id = ?"
    );
    updateQuery.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    updateQuery.addBindValue(runId);

    if (!updateQuery.exec()) {
        ltError(LT_LOG_TRAINING()) << "Failed to update training run status:" << updateQuery.lastError().text();
        return false;
    }

    ltInfo(LT_LOG_TRAINING()) << "Training run stopped:" << runId;
    emit runStatusChanged(runId, "cancelled");
    return true;
}

QVariantList TrainingService::listRuns(const QString &projectId)
{
    ltTrace(LT_LOG_TRAINING()) << "projectId=" << projectId;

    auto db = Database::instance().database();
    QVariantList result;

    QSqlQuery query(db);
    query.prepare(
        "SELECT id, project_id, snapshot_id, config_snapshot_json, "
        "runtime_env_snapshot_json, status, log_uri, started_at, finished_at "
        "FROM training_runs WHERE project_id = ? ORDER BY started_at DESC"
    );
    query.addBindValue(projectId);

    if (!query.exec()) return result;

    while (query.next()) {
        QVariantMap run;
        run["id"] = query.value(0).toString();
        run["projectId"] = query.value(1).toString();
        run["snapshotId"] = query.value(2).toString();
        run["configJson"] = query.value(3).toString();
        run["runtimeEnvJson"] = query.value(4).toString();
        run["status"] = query.value(5).toString();
        run["logUri"] = query.value(6).toString();
        run["startedAt"] = query.value(7).toString();
        run["finishedAt"] = query.value(8).toString();
        result.append(run);
    }

    ltDebug(LT_LOG_TRAINING()) << "Listed" << result.size() << "runs for project:" << projectId;
    return result;
}

QVariantMap TrainingService::getRun(const QString &runId)
{
    ltTrace(LT_LOG_TRAINING()) << "runId=" << runId;

    auto db = Database::instance().database();
    QVariantMap result;

    QSqlQuery query(db);
    query.prepare(
        "SELECT id, project_id, snapshot_id, config_snapshot_json, "
        "runtime_env_snapshot_json, status, log_uri, started_at, finished_at "
        "FROM training_runs WHERE id = ?"
    );
    query.addBindValue(runId);

    if (!query.exec() || !query.next()) return result;

    result["id"] = query.value(0).toString();
    result["projectId"] = query.value(1).toString();
    result["snapshotId"] = query.value(2).toString();
    result["configJson"] = query.value(3).toString();
    result["runtimeEnvJson"] = query.value(4).toString();
    result["status"] = query.value(5).toString();
    result["logUri"] = query.value(6).toString();
    result["startedAt"] = query.value(7).toString();
    result["finishedAt"] = query.value(8).toString();

    return result;
}

bool TrainingService::deleteRun(const QString &runId)
{
    ltTrace(LT_LOG_TRAINING()) << "runId=" << runId;

    auto db = Database::instance().database();
    if (!db.isOpen()) return false;

    // Only allow deletion if draft/cancelled/failed
    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT status FROM training_runs WHERE id = ?");
    checkQuery.addBindValue(runId);
    if (!checkQuery.exec() || !checkQuery.next()) return false;

    QString status = checkQuery.value(0).toString();
    if (status != "draft" && status != "cancelled" && status != "failed") {
        ltWarning(LT_LOG_TRAINING()) << "Cannot delete training run in status:" << status;
        return false;
    }

    QSqlQuery deleteQuery(db);
    deleteQuery.prepare("DELETE FROM training_runs WHERE id = ?");
    deleteQuery.addBindValue(runId);

    if (deleteQuery.exec()) {
        ltInfo(LT_LOG_TRAINING()) << "Deleted training run:" << runId;
        return true;
    }
    return false;
}

bool TrainingService::updateRunStatus(const QString &runId, const QString &status)
{
    ltTrace(LT_LOG_TRAINING()) << "runId=" << runId << "status=" << status;

    auto db = Database::instance().database();
    if (!db.isOpen()) return false;

    QSqlQuery query(db);

    // If transitioning to a terminal state, also set finished_at
    if (status == "succeeded" || status == "failed" || status == "cancelled") {
        query.prepare("UPDATE training_runs SET status = ?, finished_at = ? WHERE id = ?");
        query.addBindValue(status);
        query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
        query.addBindValue(runId);
    } else {
        query.prepare("UPDATE training_runs SET status = ? WHERE id = ?");
        query.addBindValue(status);
        query.addBindValue(runId);
    }

    if (!query.exec()) {
        ltError(LT_LOG_TRAINING()) << "Failed to update run status:" << query.lastError().text();
        return false;
    }

    ltInfo(LT_LOG_TRAINING()) << "Run status updated:" << runId << "->" << status;
    emit runStatusChanged(runId, status);
    return true;
}

QStringList TrainingService::listAdapters()
{
    ltTrace(LT_LOG_TRAINING());
    QStringList result;
    result << QStringLiteral("ultralytics") << QStringLiteral("anomalib");
    return result;
}
