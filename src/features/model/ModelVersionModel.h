#ifndef MODELVERSIONMODEL_H
#define MODELVERSIONMODEL_H

#include <QAbstractListModel>
#include <QVariantList>
#include <QVariantMap>

class ModelVersionModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        RunIdRole,
        BestWeightRole,
        LastWeightRole,
        MetricsRole,
        ParentVersionRole,
        CreatedAtRole
    };

    explicit ModelVersionModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void setProjectId(const QString &projectId);
    Q_INVOKABLE void refresh();
    int count() const;

signals:
    void countChanged();

private:
    QVariantList m_versions;
    QString m_projectId;
};

#endif // MODELVERSIONMODEL_H
