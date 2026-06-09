#include "ModelVersionModel.h"
#include "Database.h"
#include "utils/Log.h"

#include <QSqlQuery>
#include <QJsonDocument>
#include <QJsonObject>

ModelVersionModel::ModelVersionModel(QObject *parent) : QAbstractListModel(parent)
{
    ltTrace(LT_LOG_MODEL()) << "parent=" << parent;
}

int ModelVersionModel::rowCount(const QModelIndex &) const
{
    return m_versions.size();
}

QVariant ModelVersionModel::data(const QModelIndex &index, int role) const
{
    ltTrace(LT_LOG_MODEL()) << "row=" << index.row() << "role=" << role;

    if (!index.isValid() || index.row() < 0 || index.row() >= m_versions.size())
        return {};

    QVariantMap version = m_versions[index.row()].toMap();

    switch (role) {
    case IdRole: return version["id"];
    case RunIdRole: return version["runId"];
    case BestWeightRole: return version["bestWeightPath"];
    case LastWeightRole: return version["lastWeightPath"];
    case MetricsRole: return version["metricsJson"];
    case ParentVersionRole: return version["parentVersionId"];
    case CreatedAtRole: return version["createdAt"];
    case SourceRole: return version["source"];
    case ImportSourceRole: return version["importSourceJson"];
    default: return {};
    }
}

QHash<int, QByteArray> ModelVersionModel::roleNames() const
{
    return {
        {IdRole, "versionId"},
        {RunIdRole, "runId"},
        {BestWeightRole, "bestWeightPath"},
        {LastWeightRole, "lastWeightPath"},
        {MetricsRole, "metricsJson"},
        {ParentVersionRole, "parentVersionId"},
        {CreatedAtRole, "createdAt"},
        {SourceRole, "source"},
        {ImportSourceRole, "importSourceJson"}
    };
}

void ModelVersionModel::setProjectId(const QString &projectId)
{
    ltTrace(LT_LOG_MODEL()) << "projectId=" << projectId;
    m_projectId = projectId;
    refresh();
}

void ModelVersionModel::refresh()
{
    ltTrace(LT_LOG_MODEL()) << "projectId=" << m_projectId;

    beginResetModel();
    m_versions.clear();

    auto db = Database::instance().database();
    if (!db.isOpen() || m_projectId.isEmpty()) {
        endResetModel();
        emit countChanged();
        return;
    }

    QSqlQuery query(db);
    // 通过 project_id 字段直接查询（训练模型和导入模型都关联了 project_id）
    query.prepare(
        "SELECT mv.id, mv.run_id, mv.parent_model_version_id, "
        "mv.best_weight_path, mv.last_weight_path, "
        "mv.metrics_snapshot_json, mv.export_registry_json, mv.created_at, "
        "mv.source, mv.import_source_json "
        "FROM model_versions mv "
        "WHERE mv.project_id = ? "
        "ORDER BY mv.created_at DESC"
    );
    query.addBindValue(m_projectId);

    if (query.exec()) {
        while (query.next()) {
            QVariantMap version;
            version["id"] = query.value(0).toString();
            version["runId"] = query.value(1).toString();
            version["parentVersionId"] = query.value(2).toString();
            version["bestWeightPath"] = query.value(3).toString();
            version["lastWeightPath"] = query.value(4).toString();
            version["metricsJson"] = query.value(5).toString();
            version["exportRegistryJson"] = query.value(6).toString();
            version["createdAt"] = query.value(7).toString();
            version["source"] = query.value(8).toString();
            version["importSourceJson"] = query.value(9).toString();
            m_versions.append(version);
        }
    }

    endResetModel();
    emit countChanged();

    ltDebug(LT_LOG_MODEL()) << "Refreshed" << m_versions.size() << "model versions for project:" << m_projectId;
}

int ModelVersionModel::count() const
{
    return m_versions.size();
}
