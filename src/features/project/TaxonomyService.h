#ifndef TAXONOMYSERVICE_H
#define TAXONOMYSERVICE_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class TaxonomyService : public QObject
{
    Q_OBJECT
public:
    explicit TaxonomyService(QObject *parent = nullptr);

    // 类别体系CRUD
    Q_INVOKABLE QString createTaxonomy(const QString &projectId, const QString &name, const QVariantList &classes);
    Q_INVOKABLE QVariantList listTaxonomies(const QString &projectId);
    Q_INVOKABLE QVariantMap getTaxonomy(const QString &taxonomyId);
    Q_INVOKABLE bool deleteTaxonomy(const QString &taxonomyId);

    // 类别操作
    Q_INVOKABLE bool addClass(const QString &taxonomyId, const QString &className);
    Q_INVOKABLE bool removeClass(const QString &taxonomyId, int classIndex);
    Q_INVOKABLE bool renameClass(const QString &taxonomyId, int classIndex, const QString &newName);
    Q_INVOKABLE bool reorderClasses(const QString &taxonomyId, const QVariantList &newOrder);
    Q_INVOKABLE QVariantList getClasses(const QString &taxonomyId);

    // 版本管理
    Q_INVOKABLE int getTaxonomyVersion(const QString &taxonomyId);
};

#endif
