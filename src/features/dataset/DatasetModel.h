#ifndef DATASETMODEL_H
#define DATASETMODEL_H

#include <QAbstractListModel>
#include <QString>
#include <QVector>

class DatasetModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString projectId READ projectId WRITE setProjectId NOTIFY projectIdChanged)

public:
    explicit DatasetModel(QObject *parent = nullptr);

    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        ImageRootRole,
        SampleCountRole,
        ImportStatusRole,
        CreatedAtRole
    };

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString projectId() const { return m_projectId; }
    void setProjectId(const QString &projectId);

    Q_INVOKABLE void refresh();

signals:
    void projectIdChanged();

private:
    struct DatasetInfo {
        QString id;
        QString name;
        QString imageRoot;
        int sampleCount;
        QString importStatus;
        QString createdAt;
    };

    QString m_projectId;
    QVector<DatasetInfo> m_datasets;
};

#endif // DATASETMODEL_H
