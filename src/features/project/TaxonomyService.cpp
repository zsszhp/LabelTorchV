#include "TaxonomyService.h"
#include "database/Database.h"
#include "utils/Id.h"
#include <QSqlQuery>
#include <QJsonDocument>
#include <QJsonArray>

TaxonomyService::TaxonomyService(QObject *parent) : QObject(parent) {}

QString TaxonomyService::createTaxonomy(const QString &projectId, const QString &name, const QVariantList &classes)
{
    QString taxonomyId = Id::generate();

    QJsonArray arr;
    for (const auto &c : classes) arr.append(c.toString());
    QString classesJson = QJsonDocument(arr).toJson(QJsonDocument::Compact);

    QSqlQuery query(Database::instance().database());
    query.prepare("INSERT INTO taxonomies (id, project_id, name, class_definitions_json) VALUES (?, ?, ?, ?)");
    query.addBindValue(taxonomyId);
    query.addBindValue(projectId);
    query.addBindValue(name);
    query.addBindValue(classesJson);

    if (!query.exec()) return {};
    return taxonomyId;
}

QVariantList TaxonomyService::listTaxonomies(const QString &projectId)
{
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
        t["classes"] = query.value(3);
        result.append(t);
    }
    return result;
}

bool TaxonomyService::addClass(const QString &taxonomyId, const QString &className)
{
    Q_UNUSED(taxonomyId); Q_UNUSED(className);
    // TODO: 读取现有classes → 追加 → 更新JSON
    return true;
}

bool TaxonomyService::removeClass(const QString &taxonomyId, int classIndex)
{
    Q_UNUSED(taxonomyId); Q_UNUSED(classIndex);
    // TODO: 读取现有classes → 移除 → 更新JSON
    return true;
}
