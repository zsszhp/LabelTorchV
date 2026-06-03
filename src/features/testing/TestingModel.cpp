#include "TestingModel.h"
#include "Database.h"
#include "utils/Log.h"

#include <QSqlQuery>

TestingModel::TestingModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int TestingModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_tasks.count();
}

QVariant TestingModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_tasks.count()) return {};

    auto item = m_tasks[index.row()].toMap();
    switch (role) {
    case IdRole: return item["id"];
    case ProjectIdRole: return item["projectId"];
    case ModelVersionIdRole: return item["modelVersionId"];
    case SnapshotIdRole: return item["snapshotId"];
    case ConfigRole: return item["configJson"];
    case StatusRole: return item["status"];
    case MetricsRole: return item["metricsJson"];
    case CreatedAtRole: return item["createdAt"];
    case StartedAtRole: return item["startedAt"];
    case FinishedAtRole: return item["finishedAt"];
    default: return {};
    }
}

QHash<int, QByteArray> TestingModel::roleNames() const
{
    return {
        {IdRole, "taskId"},
        {ProjectIdRole, "projectId"},
        {ModelVersionIdRole, "modelVersionId"},
        {SnapshotIdRole, "snapshotId"},
        {ConfigRole, "configJson"},
        {StatusRole, "status"},
        {MetricsRole, "metricsJson"},
        {CreatedAtRole, "createdAt"},
        {StartedAtRole, "startedAt"},
        {FinishedAtRole, "finishedAt"}
    };
}

void TestingModel::setProjectId(const QString &projectId)
{
    m_projectId = projectId;
    refresh();
}

void TestingModel::refresh()
{
    beginResetModel();
    m_tasks.clear();

    auto db = Database::instance().database();
    if (!db.isOpen() || m_projectId.isEmpty()) {
        endResetModel();
        emit countChanged();
        return;
    }

    QSqlQuery query(db);
    query.prepare("SELECT id, project_id, model_version_id, snapshot_id, config_json, status, "
                  "metrics_json, created_at, started_at, finished_at "
                  "FROM testing_runs WHERE project_id = ? ORDER BY created_at DESC");
    query.addBindValue(m_projectId);

    if (query.exec()) {
        while (query.next()) {
            QVariantMap item;
            item["id"] = query.value(0).toString();
            item["projectId"] = query.value(1).toString();
            item["modelVersionId"] = query.value(2).toString();
            item["snapshotId"] = query.value(3).toString();
            item["configJson"] = query.value(4).toString();
            item["status"] = query.value(5).toString();
            item["metricsJson"] = query.value(6).toString();
            item["createdAt"] = query.value(7).toString();
            item["startedAt"] = query.value(8).toString();
            item["finishedAt"] = query.value(9).toString();
            m_tasks.append(item);
        }
    }

    endResetModel();
    emit countChanged();
}

int TestingModel::count() const
{
    return m_tasks.count();
}
