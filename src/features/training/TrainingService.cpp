#include "TrainingService.h"
#include "Database.h"
#include "ipc/IpcClient.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <QDateTime>
#include <QDebug>

TrainingService::TrainingService(QObject *parent) : QObject(parent) {}

void TrainingService::setIpcClient(IpcClient *client)
{
    m_ipcClient = client;
}

QString TrainingService::createRun(const QString &projectId,
                                    const QString &snapshotId,
                                    const QString &config)
{
    auto db = Database::instance().database();
    if (!db.isOpen()) return {};

    // Validate config is valid JSON
    QJsonParseError parseError;
    QJsonDocument configDoc = QJsonDocument::fromJson(config.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Invalid config JSON:" << parseError.errorString();
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
        qWarning() << "Failed to create training run:" << query.lastError().text();
        return {};
    }

    return runId;
}

bool TrainingService::startTraining(const QString &runId)
{
    auto db = Database::instance().database();
    if (!db.isOpen()) return false;

    // Check current status - only draft can transition to running
    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT status FROM training_runs WHERE id = ?");
    checkQuery.addBindValue(runId);
    if (!checkQuery.exec() || !checkQuery.next()) return false;

    QString currentStatus = checkQuery.value(0).toString();
    if (currentStatus != "draft") {
        qWarning() << "Cannot start training run in status:" << currentStatus;
        return false;
    }

    // Capture runtime environment snapshot
    QJsonObject runtimeEnv;
    runtimeEnv["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    runtimeEnv["python_version"] = ""; // Will be populated by backend
    runtimeEnv["ultralytics_version"] = "";
    runtimeEnv["torch_version"] = "";
    runtimeEnv["cuda_available"] = false;
    QString runtimeEnvJson = QString::fromUtf8(
        QJsonDocument(runtimeEnv).toJson(QJsonDocument::Compact));

    // Update status to running, set started_at and runtime env
    QSqlQuery updateQuery(db);
    updateQuery.prepare(
        "UPDATE training_runs SET status = 'running', started_at = ?, "
        "runtime_env_snapshot_json = ? WHERE id = ?"
    );
    updateQuery.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    updateQuery.addBindValue(runtimeEnvJson);
    updateQuery.addBindValue(runId);

    if (!updateQuery.exec()) {
        qWarning() << "Failed to update training run status:" << updateQuery.lastError().text();
        return false;
    }

    // Send train.start via IpcClient if available
    if (m_ipcClient) {
        // Get the full run details to pass to the backend
        QSqlQuery runQuery(db);
        runQuery.prepare("SELECT snapshot_id, config_snapshot_json FROM training_runs WHERE id = ?");
        runQuery.addBindValue(runId);
        if (runQuery.exec() && runQuery.next()) {
            QJsonObject payload;
            payload["run_id"] = runId;
            payload["snapshot_id"] = runQuery.value(0).toString();
            payload["config"] = QJsonDocument::fromJson(
                runQuery.value(1).toString().toUtf8()).object();
            m_ipcClient->sendRequest("train.start", payload);
        }
    }

    emit runStatusChanged(runId, "running");
    return true;
}

bool TrainingService::stopTraining(const QString &runId)
{
    auto db = Database::instance().database();
    if (!db.isOpen()) return false;

    // Check current status - only running can be stopped
    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT status FROM training_runs WHERE id = ?");
    checkQuery.addBindValue(runId);
    if (!checkQuery.exec() || !checkQuery.next()) return false;

    QString currentStatus = checkQuery.value(0).toString();
    if (currentStatus != "running") {
        qWarning() << "Cannot stop training run in status:" << currentStatus;
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
        qWarning() << "Failed to update training run status:" << updateQuery.lastError().text();
        return false;
    }

    emit runStatusChanged(runId, "cancelled");
    return true;
}

QVariantList TrainingService::listRuns(const QString &projectId)
{
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

    return result;
}

QVariantMap TrainingService::getRun(const QString &runId)
{
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
    auto db = Database::instance().database();
    if (!db.isOpen()) return false;

    // Only allow deletion if draft/cancelled/failed
    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT status FROM training_runs WHERE id = ?");
    checkQuery.addBindValue(runId);
    if (!checkQuery.exec() || !checkQuery.next()) return false;

    QString status = checkQuery.value(0).toString();
    if (status != "draft" && status != "cancelled" && status != "failed") {
        qWarning() << "Cannot delete training run in status:" << status;
        return false;
    }

    QSqlQuery deleteQuery(db);
    deleteQuery.prepare("DELETE FROM training_runs WHERE id = ?");
    deleteQuery.addBindValue(runId);
    return deleteQuery.exec();
}

bool TrainingService::updateRunStatus(const QString &runId, const QString &status)
{
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
        qWarning() << "Failed to update run status:" << query.lastError().text();
        return false;
    }

    emit runStatusChanged(runId, status);
    return true;
}
