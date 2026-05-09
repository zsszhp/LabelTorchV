#ifndef PROJECTMODEL_H
#define PROJECTMODEL_H

#include <QAbstractListModel>

class ProjectModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit ProjectModel(QObject *parent = nullptr);

    enum Roles { IdRole = Qt::UserRole + 1, NameRole, PathRole, CreatedAtRole };

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void refresh();

private:
    struct ProjectInfo {
        QString id, name, path, createdAt;
    };
    QVector<ProjectInfo> m_projects;
};

#endif
