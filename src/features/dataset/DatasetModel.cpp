#include "DatasetModel.h"
#include "database/Database.h"
#include "utils/Log.h"

#include <QSqlQuery>

DatasetModel::DatasetModel(QObject *parent) : QAbstractListModel(parent)
{
    ltTrace(LT_LOG_DATASET()) << "DatasetModel parent=" << parent;
}

int DatasetModel::rowCount(const QModelIndex &) const
{
    return m_datasets.size();
}

QVariant DatasetModel::data(const QModelIndex &index, int role) const
{
    ltTrace(LT_LOG_DATASET()) << "data row=" << (index.isValid() ? index.row() : -1) << "role=" << role;

    if (!index.isValid() || index.row() >= m_datasets.size())
        return {};

    const auto &d = m_datasets[index.row()];
    switch (role) {
        case IdRole:          return d.id;
        case NameRole:        return d.name;
        case ImageRootRole:   return d.imageRoot;
        case SampleCountRole: return d.sampleCount;
        case ImportStatusRole: return d.importStatus;
        case CreatedAtRole:   return d.createdAt;
        default:              return {};
    }
}

QHash<int, QByteArray> DatasetModel::roleNames() const
{
    ltTrace(LT_LOG_DATASET()) << "roleNames";

    return {
        {IdRole,           "datasetId"},
        {NameRole,         "name"},
        {ImageRootRole,    "imageRoot"},
        {SampleCountRole,  "sampleCount"},
        {ImportStatusRole, "importStatus"},
        {CreatedAtRole,    "createdAt"}
    };
}

void DatasetModel::setProjectId(const QString &projectId)
{
    ltTrace(LT_LOG_DATASET()) << "setProjectId projectId=" << projectId;

    if (m_projectId != projectId) {
        m_projectId = projectId;
        emit projectIdChanged();
        ltInfo(LT_LOG_DATASET()) << "Project changed, refreshing dataset model for projectId=" << projectId;
        refresh();
    }
}

void DatasetModel::refresh()
{
    ltTrace(LT_LOG_DATASET()) << "refresh projectId=" << m_projectId;

    QSqlQuery query(Database::instance().database());

    if (m_projectId.isEmpty()) {
        query.prepare("SELECT id, name, image_root, sample_count, import_status, created_at "
                      "FROM datasets ORDER BY created_at DESC");
    } else {
        query.prepare("SELECT id, name, image_root, sample_count, import_status, created_at "
                      "FROM datasets WHERE project_id = ? ORDER BY created_at DESC");
        query.addBindValue(m_projectId);
    }

    beginResetModel();
    m_datasets.clear();

    if (query.exec()) {
        while (query.next()) {
            DatasetInfo d;
            d.id = query.value(0).toString();
            d.name = query.value(1).toString();
            d.imageRoot = query.value(2).toString();
            d.sampleCount = query.value(3).toInt();
            d.importStatus = query.value(4).toString();
            d.createdAt = query.value(5).toString();
            m_datasets.append(d);
        }
    } else {
        ltWarning(LT_LOG_DATASET()) << "refresh: query failed";
    }

    endResetModel();

    ltDebug(LT_LOG_DATASET()) << "refresh: loaded" << m_datasets.size() << "datasets";
}
