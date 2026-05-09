#ifndef SNAPSHOTMODEL_H
#define SNAPSHOTMODEL_H

#include <QAbstractListModel>
#include <QVariantList>
#include <QVariantMap>

class SnapshotModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        DatasetIdRole,
        SampleCountRole,
        TrainCountRole,
        ValCountRole,
        TaxonomyVersionRole,
        RevisionBoundaryRole,
        CreatedAtRole
    };

    explicit SnapshotModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void setDatasetId(const QString &datasetId);
    Q_INVOKABLE void refresh();
    int count() const;

signals:
    void countChanged();

private:
    QVariantList m_snapshots;
    QString m_datasetId;
};

#endif // SNAPSHOTMODEL_H
