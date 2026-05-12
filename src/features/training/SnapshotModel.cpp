#include "SnapshotModel.h"
#include "Database.h"
#include "utils/Log.h"

#include <QSqlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

SnapshotModel::SnapshotModel(QObject *parent) : QAbstractListModel(parent)
{
    ltTrace(LT_LOG_TRAINING()) << "parent=" << parent;
}

int SnapshotModel::rowCount(const QModelIndex &) const
{
    return m_snapshots.size();
}

QVariant SnapshotModel::data(const QModelIndex &index, int role) const
{
    ltTrace(LT_LOG_TRAINING()) << "row=" << index.row() << "role=" << role;

    if (!index.isValid() || index.row() < 0 || index.row() >= m_snapshots.size())
        return {};

    QVariantMap snapshot = m_snapshots[index.row()].toMap();

    switch (role) {
    case IdRole: return snapshot["id"];
    case DatasetIdRole: return snapshot["datasetId"];
    case SampleCountRole: return snapshot["sampleCount"];
    case TrainCountRole: return snapshot["trainCount"];
    case ValCountRole: return snapshot["valCount"];
    case TaxonomyVersionRole: return snapshot["taxonomyVersion"];
    case RevisionBoundaryRole: return snapshot["revisionBoundary"];
    case CreatedAtRole: return snapshot["createdAt"];
    default: return {};
    }
}

QHash<int, QByteArray> SnapshotModel::roleNames() const
{
    return {
        {IdRole, "snapshotId"},
        {DatasetIdRole, "datasetId"},
        {SampleCountRole, "sampleCount"},
        {TrainCountRole, "trainCount"},
        {ValCountRole, "valCount"},
        {TaxonomyVersionRole, "taxonomyVersion"},
        {RevisionBoundaryRole, "revisionBoundary"},
        {CreatedAtRole, "createdAt"}
    };
}

void SnapshotModel::setDatasetId(const QString &datasetId)
{
    ltTrace(LT_LOG_TRAINING()) << "datasetId=" << datasetId;
    m_datasetId = datasetId;
    refresh();
}

void SnapshotModel::refresh()
{
    ltTrace(LT_LOG_TRAINING()) << "datasetId=" << m_datasetId;

    beginResetModel();
    m_snapshots.clear();

    auto db = Database::instance().database();
    if (!db.isOpen() || m_datasetId.isEmpty()) {
        endResetModel();
        emit countChanged();
        return;
    }

    QSqlQuery query(db);
    query.prepare("SELECT id, dataset_id, sample_manifest_json, split_manifest_json, "
                  "taxonomy_version, annotation_revision_boundary, created_at "
                  "FROM dataset_snapshots WHERE dataset_id = ? ORDER BY created_at DESC");
    query.addBindValue(m_datasetId);

    if (query.exec()) {
        while (query.next()) {
            QVariantMap snapshot;
            snapshot["id"] = query.value(0).toString();
            snapshot["datasetId"] = query.value(1).toString();

            // Parse sample count
            QJsonDocument manifestDoc = QJsonDocument::fromJson(query.value(2).toString().toUtf8());
            snapshot["sampleCount"] = manifestDoc.array().size();

            // Parse split counts
            QJsonDocument splitDoc = QJsonDocument::fromJson(query.value(3).toString().toUtf8());
            QJsonObject splitObj = splitDoc.object();
            snapshot["trainCount"] = splitObj["train"].toArray().size();
            snapshot["valCount"] = splitObj["val"].toArray().size();

            snapshot["taxonomyVersion"] = query.value(4).toString();
            snapshot["revisionBoundary"] = query.value(5).toString();
            snapshot["createdAt"] = query.value(6).toString();

            m_snapshots.append(snapshot);
        }
    }

    endResetModel();
    emit countChanged();

    ltDebug(LT_LOG_TRAINING()) << "Refreshed" << m_snapshots.size() << "snapshots for dataset:" << m_datasetId;
}

int SnapshotModel::count() const
{
    return m_snapshots.size();
}
