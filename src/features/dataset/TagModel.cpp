#include "TagModel.h"
#include "TagService.h"
#include "utils/Log.h"

TagModel::TagModel(QObject *parent)
    : QAbstractListModel(parent)
{
    ltTrace(LT_LOG_DATASET()) << "TagModel created";
}

void TagModel::setTagService(TagService *service)
{
    if (m_tagService) {
        disconnect(m_tagService, &TagService::tagsChanged,
                   this, &TagModel::onTagsChanged);
    }
    m_tagService = service;
    if (m_tagService) {
        connect(m_tagService, &TagService::tagsChanged,
                this, &TagModel::onTagsChanged);
    }
}

int TagModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_tags.size();
}

QVariant TagModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tags.size())
        return {};

    QVariantMap tag = m_tags.at(index.row()).toMap();

    switch (role) {
    case IdRole:
        return tag.value("id");
    case NameRole:
        return tag.value("name");
    case ShortcutRole:
        return tag.value("shortcut");
    case CreatedAtRole:
        return tag.value("createdAt");
    default:
        return {};
    }
}

QHash<int, QByteArray> TagModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "tagId";
    roles[NameRole] = "tagName";
    roles[ShortcutRole] = "tagShortcut";
    roles[CreatedAtRole] = "tagCreatedAt";
    return roles;
}

void TagModel::setDatasetId(const QString &id)
{
    if (m_datasetId == id) return;
    m_datasetId = id;
    emit datasetIdChanged();
    refresh();
}

void TagModel::refresh()
{
    if (!m_tagService) return;

    beginResetModel();
    m_tags = m_tagService->listTags(m_datasetId);
    endResetModel();

    ltDebug(LT_LOG_DATASET()) << "TagModel refreshed:" << m_tags.size()
                              << "tags for dataset:" << m_datasetId;
}

void TagModel::onTagsChanged(const QString &datasetId)
{
    // 只刷新当前数据集的标签
    if (datasetId == m_datasetId) {
        refresh();
    }
}
