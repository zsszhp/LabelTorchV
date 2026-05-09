#include "DatasetModel.h"

DatasetModel::DatasetModel(QObject *parent) : QAbstractListModel(parent) {}

int DatasetModel::rowCount(const QModelIndex &) const { return 0; }

QVariant DatasetModel::data(const QModelIndex &, int) const { return {}; }

QHash<int, QByteArray> DatasetModel::roleNames() const { return {}; }
