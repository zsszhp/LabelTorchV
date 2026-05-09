#ifndef TAXONOMYMODEL_H
#define TAXONOMYMODEL_H

#include <QAbstractListModel>
#include <QString>

class TaxonomyModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString taxonomyId READ taxonomyId WRITE setTaxonomyId NOTIFY taxonomyIdChanged)

public:
    explicit TaxonomyModel(QObject *parent = nullptr);

    enum Roles { ClassNameRole = Qt::UserRole + 1, IndexRole };

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString taxonomyId() const { return m_taxonomyId; }
    void setTaxonomyId(const QString &id);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool addClass(const QString &className);
    Q_INVOKABLE bool removeClass(int index);
    Q_INVOKABLE bool renameClass(int index, const QString &newName);

signals:
    void taxonomyIdChanged();

private:
    QString m_taxonomyId;
    QStringList m_classes;
};

#endif
