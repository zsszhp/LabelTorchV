#include "TrainingModel.h"

TrainingModel::TrainingModel(QObject *parent) : QAbstractListModel(parent) {}

int TrainingModel::rowCount(const QModelIndex &) const { return 0; }

QVariant TrainingModel::data(const QModelIndex &, int) const { return {}; }

QHash<int, QByteArray> TrainingModel::roleNames() const { return {}; }
