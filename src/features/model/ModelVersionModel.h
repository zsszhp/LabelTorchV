#ifndef MODELVERSIONMODEL_H
#define MODELVERSIONMODEL_H

#include <QAbstractListModel>

class ModelVersionModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit ModelVersionModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
};

#endif
