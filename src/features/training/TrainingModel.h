#ifndef TRAININGMODEL_H
#define TRAININGMODEL_H

#include <QAbstractListModel>

class TrainingModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit TrainingModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
};

#endif
