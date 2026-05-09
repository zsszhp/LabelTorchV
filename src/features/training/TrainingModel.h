#ifndef TRAININGMODEL_H
#define TRAININGMODEL_H

#include <QAbstractListModel>
#include <QVariantList>
#include <QVariantMap>

class TrainingModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        SnapshotIdRole,
        StatusRole,
        ConfigRole,
        StartedAtRole,
        FinishedAtRole
    };

    explicit TrainingModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void setProjectId(const QString &projectId);
    Q_INVOKABLE void refresh();
    int count() const;

signals:
    void countChanged();

private:
    QVariantList m_runs;
    QString m_projectId;
};

#endif // TRAININGMODEL_H
