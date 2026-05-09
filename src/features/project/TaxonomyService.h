#ifndef TAXONOMYSERVICE_H
#define TAXONOMYSERVICE_H

#include <QObject>
#include <QString>
#include <QVariantList>

class TaxonomyService : public QObject
{
    Q_OBJECT
public:
    explicit TaxonomyService(QObject *parent = nullptr);

    Q_INVOKABLE QString createTaxonomy(const QString &projectId, const QString &name, const QVariantList &classes);
    Q_INVOKABLE QVariantList listTaxonomies(const QString &projectId);
    Q_INVOKABLE bool addClass(const QString &taxonomyId, const QString &className);
    Q_INVOKABLE bool removeClass(const QString &taxonomyId, int classIndex);
};

#endif
