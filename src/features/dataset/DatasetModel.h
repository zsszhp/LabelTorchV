#ifndef DATASETMODEL_H
#define DATASETMODEL_H

#include <QAbstractListModel>

class DatasetModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit DatasetModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
};

#endif
