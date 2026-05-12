#include "TaxonomyService.h"
#include "database/Database.h"
#include "utils/Id.h"
#include "utils/Log.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonArray>

TaxonomyService::TaxonomyService(QObject *parent) : QObject(parent) {}

QString TaxonomyService::createTaxonomy(const QString &projectId, const QString &name, const QVariantList &classes)
{
    ltTrace(LT_LOG_TAXONOMY()) << "createTaxonomy projectId=" << projectId << "name=" << name << "classCount=" << classes.size();

    QString taxonomyId = Id::generate();

    QJsonArray arr;
    for (const auto &c : classes) arr.append(c.toString());
    QString classesJson = QJsonDocument(arr).toJson(QJsonDocument::Compact);

    QSqlQuery query(Database::instance().database());
    query.prepare("INSERT INTO taxonomies (id, project_id, name, version, class_definitions_json) VALUES (?, ?, ?, 1, ?)");
    query.addBindValue(taxonomyId);
    query.addBindValue(projectId);
    query.addBindValue(name);
    query.addBindValue(classesJson);

    if (!query.exec()) {
        ltError(LT_LOG_TAXONOMY()) << "Failed to create taxonomy:" << query.lastError().text();
        return {};
    }
    ltInfo(LT_LOG_TAXONOMY()) << "Taxonomy created:" << taxonomyId << name << "with" << classes.size() << "classes";
    return taxonomyId;
}

QVariantList TaxonomyService::listTaxonomies(const QString &projectId)
{
    ltTrace(LT_LOG_TAXONOMY()) << "listTaxonomies projectId=" << projectId;

    QVariantList result;
    QSqlQuery query(Database::instance().database());
    query.prepare("SELECT id, name, version, class_definitions_json FROM taxonomies WHERE project_id = ?");
    query.addBindValue(projectId);
    query.exec();
    while (query.next()) {
        QVariantMap t;
        t["id"] = query.value(0);
        t["name"] = query.value(1);
        t["version"] = query.value(2);
        t["classesJson"] = query.value(3);
        result.append(t);
    }

    ltDebug(LT_LOG_TAXONOMY()) << "Listed" << result.size() << "taxonomies for project" << projectId;
    return result;
}

QVariantMap TaxonomyService::getTaxonomy(const QString &taxonomyId)
{
    ltTrace(LT_LOG_TAXONOMY()) << "getTaxonomy id=" << taxonomyId;

    QSqlQuery query(Database::instance().database());
    query.prepare("SELECT id, project_id, name, version, class_definitions_json FROM taxonomies WHERE id = ?");
    query.addBindValue(taxonomyId);
    if (query.exec() && query.next()) {
        QVariantMap t;
        t["id"] = query.value(0);
        t["projectId"] = query.value(1);
        t["name"] = query.value(2);
        t["version"] = query.value(3);
        t["classesJson"] = query.value(4);
        return t;
    }
    return {};
}

bool TaxonomyService::deleteTaxonomy(const QString &taxonomyId)
{
    ltTrace(LT_LOG_TAXONOMY()) << "deleteTaxonomy id=" << taxonomyId;

    QSqlQuery query(Database::instance().database());
    query.prepare("DELETE FROM taxonomies WHERE id = ?");
    query.addBindValue(taxonomyId);
    bool ok = query.exec();

    if (ok) {
        ltInfo(LT_LOG_TAXONOMY()) << "Taxonomy deleted:" << taxonomyId;
    } else {
        ltError(LT_LOG_TAXONOMY()) << "Failed to delete taxonomy:" << query.lastError().text();
    }
    return ok;
}

bool TaxonomyService::addClass(const QString &taxonomyId, const QString &className)
{
    ltTrace(LT_LOG_TAXONOMY()) << "addClass taxonomyId=" << taxonomyId << "class=" << className;

    QVariantList classes = getClasses(taxonomyId);
    classes.append(className);

    QJsonArray arr;
    for (const auto &c : classes) arr.append(c.toString());
    QString classesJson = QJsonDocument(arr).toJson(QJsonDocument::Compact);

    QSqlQuery query(Database::instance().database());
    query.prepare("UPDATE taxonomies SET class_definitions_json = ?, version = version + 1 WHERE id = ?");
    query.addBindValue(classesJson);
    query.addBindValue(taxonomyId);
    if (!query.exec()) {
        ltError(LT_LOG_TAXONOMY()) << "Failed to add class:" << query.lastError().text();
        return false;
    }
    ltInfo(LT_LOG_TAXONOMY()) << "Class added:" << className << "to taxonomy" << taxonomyId;
    return true;
}

bool TaxonomyService::removeClass(const QString &taxonomyId, int classIndex)
{
    ltTrace(LT_LOG_TAXONOMY()) << "removeClass taxonomyId=" << taxonomyId << "index=" << classIndex;

    QVariantList classes = getClasses(taxonomyId);
    if (classIndex < 0 || classIndex >= classes.size()) return false;
    classes.removeAt(classIndex);

    QJsonArray arr;
    for (const auto &c : classes) arr.append(c.toString());
    QString classesJson = QJsonDocument(arr).toJson(QJsonDocument::Compact);

    QSqlQuery query(Database::instance().database());
    query.prepare("UPDATE taxonomies SET class_definitions_json = ?, version = version + 1 WHERE id = ?");
    query.addBindValue(classesJson);
    query.addBindValue(taxonomyId);
    if (!query.exec()) {
        ltError(LT_LOG_TAXONOMY()) << "Failed to remove class:" << query.lastError().text();
        return false;
    }
    ltInfo(LT_LOG_TAXONOMY()) << "Class removed at index" << classIndex << "from taxonomy" << taxonomyId;
    return true;
}

bool TaxonomyService::renameClass(const QString &taxonomyId, int classIndex, const QString &newName)
{
    ltTrace(LT_LOG_TAXONOMY()) << "renameClass taxonomyId=" << taxonomyId << "index=" << classIndex << "newName=" << newName;

    QVariantList classes = getClasses(taxonomyId);
    if (classIndex < 0 || classIndex >= classes.size()) return false;
    classes[classIndex] = newName;

    QJsonArray arr;
    for (const auto &c : classes) arr.append(c.toString());
    QString classesJson = QJsonDocument(arr).toJson(QJsonDocument::Compact);

    QSqlQuery query(Database::instance().database());
    query.prepare("UPDATE taxonomies SET class_definitions_json = ?, version = version + 1 WHERE id = ?");
    query.addBindValue(classesJson);
    query.addBindValue(taxonomyId);
    if (!query.exec()) {
        ltError(LT_LOG_TAXONOMY()) << "Failed to rename class:" << query.lastError().text();
        return false;
    }
    return true;
}

bool TaxonomyService::reorderClasses(const QString &taxonomyId, const QVariantList &newOrder)
{
    ltTrace(LT_LOG_TAXONOMY()) << "reorderClasses taxonomyId=" << taxonomyId << "count=" << newOrder.size();

    QJsonArray arr;
    for (const auto &c : newOrder) arr.append(c.toString());
    QString classesJson = QJsonDocument(arr).toJson(QJsonDocument::Compact);

    QSqlQuery query(Database::instance().database());
    query.prepare("UPDATE taxonomies SET class_definitions_json = ?, version = version + 1 WHERE id = ?");
    query.addBindValue(classesJson);
    query.addBindValue(taxonomyId);
    if (!query.exec()) {
        ltError(LT_LOG_TAXONOMY()) << "Failed to reorder classes:" << query.lastError().text();
        return false;
    }
    return true;
}

QVariantList TaxonomyService::getClasses(const QString &taxonomyId)
{
    ltTrace(LT_LOG_TAXONOMY()) << "getClasses taxonomyId=" << taxonomyId;

    QSqlQuery query(Database::instance().database());
    query.prepare("SELECT class_definitions_json FROM taxonomies WHERE id = ?");
    query.addBindValue(taxonomyId);
    if (query.exec() && query.next()) {
        QJsonDocument doc = QJsonDocument::fromJson(query.value(0).toString().toUtf8());
        QVariantList result;
        for (const auto &v : doc.array()) result.append(v.toVariant());
        return result;
    }
    return {};
}

int TaxonomyService::getTaxonomyVersion(const QString &taxonomyId)
{
    ltTrace(LT_LOG_TAXONOMY()) << "getTaxonomyVersion id=" << taxonomyId;

    QSqlQuery query(Database::instance().database());
    query.prepare("SELECT version FROM taxonomies WHERE id = ?");
    query.addBindValue(taxonomyId);
    if (query.exec() && query.next()) return query.value(0).toInt();
    return -1;
}
