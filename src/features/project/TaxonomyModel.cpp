#include "TaxonomyModel.h"
#include "TaxonomyService.h"
#include "database/Database.h"
#include "utils/Log.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QSqlQuery>
#include <QSqlError>

TaxonomyModel::TaxonomyModel(QObject *parent) : QAbstractListModel(parent) {}

int TaxonomyModel::rowCount(const QModelIndex &) const { return m_classes.size(); }

QVariant TaxonomyModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_classes.size()) return {};
    switch (role) {
        case ClassNameRole: return m_classes[index.row()];
        case IndexRole: return index.row();
        case DeprecatedRole: return m_classes[index.row()].isEmpty();
        default: return {};
    }
}

QHash<int, QByteArray> TaxonomyModel::roleNames() const
{
    return {
        {ClassNameRole, "className"},
        {IndexRole, "classIndex"},
        {DeprecatedRole, "isDeprecated"},
    };
}

void TaxonomyModel::setTaxonomyId(const QString &id)
{
    ltTrace(LT_LOG_TAXONOMY()) << "setTaxonomyId id=" << id;
    if (m_taxonomyId != id) {
        m_taxonomyId = id;
        emit taxonomyIdChanged();
        refresh();
    }
}

void TaxonomyModel::refresh()
{
    ltTrace(LT_LOG_TAXONOMY()) << "refresh taxonomyId=" << m_taxonomyId;

    if (m_taxonomyId.isEmpty()) {
        beginResetModel();
        m_classes.clear();
        endResetModel();
        return;
    }

    QSqlQuery query(Database::instance().database());
    query.prepare("SELECT class_definitions_json FROM taxonomies WHERE id = ?");
    query.addBindValue(m_taxonomyId);
    if (query.exec() && query.next()) {
        QJsonDocument doc = QJsonDocument::fromJson(query.value(0).toString().toUtf8());
        beginResetModel();
        m_classes.clear();
        for (const auto &v : doc.array()) m_classes.append(v.toString());
        endResetModel();
        ltDebug(LT_LOG_TAXONOMY()) << "Refreshed" << m_classes.size() << "classes";
    } else {
        ltWarning(LT_LOG_TAXONOMY()) << "Failed to load taxonomy:" << query.lastError().text();
    }
}

bool TaxonomyModel::addClass(const QString &className)
{
    ltTrace(LT_LOG_TAXONOMY()) << "addClass name=" << className;

    if (m_taxonomyId.isEmpty() || className.trimmed().isEmpty()) return false;

    int insertRow = m_classes.size();
    beginInsertRows(QModelIndex(), insertRow, insertRow);
    m_classes.append(className.trimmed());
    endInsertRows();

    // Persist to database
    QSqlQuery query(Database::instance().database());
    QJsonArray arr;
    for (const auto &c : m_classes) arr.append(c);
    QString json = QJsonDocument(arr).toJson(QJsonDocument::Compact);
    query.prepare("UPDATE taxonomies SET class_definitions_json = ?, version = version + 1 WHERE id = ?");
    query.addBindValue(json);
    query.addBindValue(m_taxonomyId);
    bool ok = query.exec();
    if (!ok) ltWarning(LT_LOG_TAXONOMY()) << "Failed to persist addClass:" << query.lastError().text();
    return ok;
}

bool TaxonomyModel::removeClass(int index)
{
    ltTrace(LT_LOG_TAXONOMY()) << "removeClass index=" << index;

    if (m_taxonomyId.isEmpty() || index < 0 || index >= m_classes.size()) return false;

    // 与 TaxonomyService::removeClass 默认路径保持一致：废弃 + 保留 id 空位。
    // 绝不能物理删除（removeAt）：YOLO 标签按 class_id（即本列表下标）引用类别，
    // 物理删除会使后续 class_id 整体前移，导致已有标签全部错类。
    // 因此这里只清空类名占位，数组长度不变，后续 class_id 保持稳定。
    if (m_classes[index].isEmpty()) {
        // 该位已废弃，重复删除无意义
        ltDebug(LT_LOG_TAXONOMY()) << "Class already deprecated at index" << index;
        return false;
    }

    m_classes[index] = QString();
    emit dataChanged(createIndex(index, 0), createIndex(index, 0), {ClassNameRole, DeprecatedRole});

    // Persist to database（保留空串占位，长度与 class_id 对齐）
    QSqlQuery query(Database::instance().database());
    QJsonArray arr;
    for (const auto &c : m_classes) arr.append(c);
    QString json = QJsonDocument(arr).toJson(QJsonDocument::Compact);
    query.prepare("UPDATE taxonomies SET class_definitions_json = ?, version = version + 1 WHERE id = ?");
    query.addBindValue(json);
    query.addBindValue(m_taxonomyId);
    bool ok = query.exec();
    if (!ok) ltWarning(LT_LOG_TAXONOMY()) << "Failed to persist removeClass:" << query.lastError().text();
    return ok;
}

bool TaxonomyModel::renameClass(int index, const QString &newName)
{
    ltTrace(LT_LOG_TAXONOMY()) << "renameClass index=" << index << "newName=" << newName;

    if (index < 0 || index >= m_classes.size() || newName.trimmed().isEmpty()) return false;

    m_classes[index] = newName.trimmed();
    // 复活废弃位时 DeprecatedRole 也会变化，一并通知视图
    emit dataChanged(createIndex(index, 0), createIndex(index, 0), {ClassNameRole, DeprecatedRole});

    QSqlQuery query(Database::instance().database());
    QJsonArray arr;
    for (const auto &c : m_classes) arr.append(c);
    QString json = QJsonDocument(arr).toJson(QJsonDocument::Compact);
    query.prepare("UPDATE taxonomies SET class_definitions_json = ?, version = version + 1 WHERE id = ?");
    query.addBindValue(json);
    query.addBindValue(m_taxonomyId);
    bool ok = query.exec();
    if (!ok) ltWarning(LT_LOG_TAXONOMY()) << "Failed to persist renameClass:" << query.lastError().text();
    return ok;
}
