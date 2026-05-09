#include "TaxonomyModel.h"
#include "TaxonomyService.h"
#include "database/Database.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QSqlQuery>

TaxonomyModel::TaxonomyModel(QObject *parent) : QAbstractListModel(parent) {}

int TaxonomyModel::rowCount(const QModelIndex &) const { return m_classes.size(); }

QVariant TaxonomyModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_classes.size()) return {};
    switch (role) {
        case ClassNameRole: return m_classes[index.row()];
        case IndexRole: return index.row();
        default: return {};
    }
}

QHash<int, QByteArray> TaxonomyModel::roleNames() const
{
    return {{ClassNameRole, "className"}, {IndexRole, "classIndex"}};
}

void TaxonomyModel::setTaxonomyId(const QString &id)
{
    if (m_taxonomyId != id) {
        m_taxonomyId = id;
        emit taxonomyIdChanged();
        refresh();
    }
}

void TaxonomyModel::refresh()
{
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
    }
}

bool TaxonomyModel::addClass(const QString &className)
{
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
    return query.exec();
}

bool TaxonomyModel::removeClass(int index)
{
    if (index < 0 || index >= m_classes.size()) return false;

    beginRemoveRows(QModelIndex(), index, index);
    m_classes.removeAt(index);
    endRemoveRows();

    QSqlQuery query(Database::instance().database());
    QJsonArray arr;
    for (const auto &c : m_classes) arr.append(c);
    QString json = QJsonDocument(arr).toJson(QJsonDocument::Compact);
    query.prepare("UPDATE taxonomies SET class_definitions_json = ?, version = version + 1 WHERE id = ?");
    query.addBindValue(json);
    query.addBindValue(m_taxonomyId);
    return query.exec();
}

bool TaxonomyModel::renameClass(int index, const QString &newName)
{
    if (index < 0 || index >= m_classes.size() || newName.trimmed().isEmpty()) return false;

    m_classes[index] = newName.trimmed();
    emit dataChanged(createIndex(index, 0), createIndex(index, 0), {ClassNameRole});

    QSqlQuery query(Database::instance().database());
    QJsonArray arr;
    for (const auto &c : m_classes) arr.append(c);
    QString json = QJsonDocument(arr).toJson(QJsonDocument::Compact);
    query.prepare("UPDATE taxonomies SET class_definitions_json = ?, version = version + 1 WHERE id = ?");
    query.addBindValue(json);
    query.addBindValue(m_taxonomyId);
    return query.exec();
}
