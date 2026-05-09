#include "TrainingModel.h"
#include "Database.h"

#include <QSqlQuery>
#include <QJsonDocument>
#include <QJsonObject>

TrainingModel::TrainingModel(QObject *parent) : QAbstractListModel(parent) {}

int TrainingModel::rowCount(const QModelIndex &) const { return m_runs.size(); }

QVariant TrainingModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_runs.size())
        return {};

    QVariantMap run = m_runs[index.row()].toMap();

    switch (role) {
    case IdRole: return run["id"];
    case SnapshotIdRole: return run["snapshotId"];
    case StatusRole: return run["status"];
    case ConfigRole: return run["configJson"];
    case StartedAtRole: return run["startedAt"];
    case FinishedAtRole: return run["finishedAt"];
    default: return {};
    }
}

QHash<int, QByteArray> TrainingModel::roleNames() const
{
    return {
        {IdRole, "runId"},
        {SnapshotIdRole, "snapshotId"},
        {StatusRole, "status"},
        {ConfigRole, "configJson"},
        {StartedAtRole, "startedAt"},
        {FinishedAtRole, "finishedAt"}
    };
}

void TrainingModel::setProjectId(const QString &projectId)
{
    m_projectId = projectId;
    refresh();
}

void TrainingModel::refresh()
{
    beginResetModel();
    m_runs.clear();

    auto db = Database::instance().database();
    if (!db.isOpen() || m_projectId.isEmpty()) {
        endResetModel();
        emit countChanged();
        return;
    }

    QSqlQuery query(db);
    query.prepare(
        "SELECT id, project_id, snapshot_id, config_snapshot_json, "
        "runtime_env_snapshot_json, status, log_uri, started_at, finished_at "
        "FROM training_runs WHERE project_id = ? ORDER BY started_at DESC"
    );
    query.addBindValue(m_projectId);

    if (query.exec()) {
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
            m_runs.append(run);
        }
    }

    endResetModel();
    emit countChanged();
}

int TrainingModel::count() const { return m_runs.size(); }
