#include "ProjectModel.h"
#include "database/Database.h"
#include <QSqlQuery>

ProjectModel::ProjectModel(QObject *parent) : QAbstractListModel(parent) { refresh(); }

int ProjectModel::rowCount(const QModelIndex &) const { return m_projects.size(); }

QVariant ProjectModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_projects.size()) return {};
    const auto &p = m_projects[index.row()];
    switch (role) {
        case IdRole: return p.id;
        case NameRole: return p.name;
        case PathRole: return p.path;
        case CreatedAtRole: return p.createdAt;
        default: return {};
    }
}

QHash<int, QByteArray> ProjectModel::roleNames() const
{
    return {{IdRole, "projectId"}, {NameRole, "name"}, {PathRole, "path"}, {CreatedAtRole, "createdAt"}};
}

void ProjectModel::refresh()
{
    beginResetModel();
    m_projects.clear();
    QSqlQuery query(Database::instance().database());
    query.exec("SELECT id, name, root_path, created_at FROM projects ORDER BY updated_at DESC");
    while (query.next()) {
        m_projects.append({query.value(0).toString(), query.value(1).toString(),
                          query.value(2).toString(), query.value(3).toString()});
    }
    endResetModel();
}
