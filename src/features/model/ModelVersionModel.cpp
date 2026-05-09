#include "ModelVersionModel.h"

ModelVersionModel::ModelVersionModel(QObject *parent) : QAbstractListModel(parent) {}

int ModelVersionModel::rowCount(const QModelIndex &) const { return 0; }

QVariant ModelVersionModel::data(const QModelIndex &, int) const { return {}; }

QHash<int, QByteArray> ModelVersionModel::roleNames() const { return {}; }
