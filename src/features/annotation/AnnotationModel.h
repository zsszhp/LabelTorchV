#ifndef ANNOTATIONMODEL_H
#define ANNOTATIONMODEL_H

#include <QAbstractListModel>

class AnnotationModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit AnnotationModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
};

#endif
