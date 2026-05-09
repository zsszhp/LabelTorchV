#include "AnnotationModel.h"

AnnotationModel::AnnotationModel(QObject *parent) : QAbstractListModel(parent) {}

int AnnotationModel::rowCount(const QModelIndex &) const { return 0; }

QVariant AnnotationModel::data(const QModelIndex &, int) const { return {}; }

QHash<int, QByteArray> AnnotationModel::roleNames() const { return {}; }
