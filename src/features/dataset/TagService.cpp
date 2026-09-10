#include "TagService.h"
#include "Database.h"
#include "utils/Id.h"
#include "utils/Log.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>

TagService::TagService(QObject *parent)
    : QObject(parent)
{
    ltTrace(LT_LOG_DATASET()) << "TagService created";
}

QString TagService::addTag(const QString &datasetId, const QString &name,
                            const QString &shortcut)
{
    ltTrace(LT_LOG_DATASET()) << "addTag datasetId=" << datasetId
                              << "name=" << name << "shortcut=" << shortcut;

    if (datasetId.isEmpty() || name.trimmed().isEmpty()) {
        ltWarning(LT_LOG_DATASET()) << "addTag: 数据集 ID 或标签名为空";
        return {};
    }

    QString trimmedName = name.trimmed();

    // 检查同名标签是否已存在
    if (tagExists(datasetId, trimmedName)) {
        ltWarning(LT_LOG_DATASET()) << "addTag: 标签已存在 datasetId=" << datasetId
                                    << "name=" << trimmedName;
        return {};
    }

    auto db = Database::instance().database();
    if (!db.isOpen()) {
        ltError(LT_LOG_DATASET()) << "addTag: 数据库未打开";
        return {};
    }

    QString tagId = Id::generate();
    QString now = QDateTime::currentDateTime().toString(Qt::ISODate);

    QSqlQuery query(db);
    query.prepare(
        "INSERT INTO dataset_tags (id, dataset_id, name, shortcut, created_at) "
        "VALUES (?, ?, ?, ?, ?)"
    );
    query.addBindValue(tagId);
    query.addBindValue(datasetId);
    query.addBindValue(trimmedName);
    query.addBindValue(shortcut.trimmed());
    query.addBindValue(now);

    if (!query.exec()) {
        ltError(LT_LOG_DATASET()) << "addTag: 插入失败:" << query.lastError().text();
        return {};
    }

    ltInfo(LT_LOG_DATASET()) << "Tag added:" << tagId << "for dataset:" << datasetId
                             << "name:" << trimmedName;
    emit tagsChanged(datasetId);
    return tagId;
}

bool TagService::removeTag(const QString &tagId)
{
    ltTrace(LT_LOG_DATASET()) << "removeTag tagId=" << tagId;

    if (tagId.isEmpty()) {
        ltWarning(LT_LOG_DATASET()) << "removeTag: 标签 ID 为空";
        return false;
    }

    auto db = Database::instance().database();
    if (!db.isOpen()) return false;

    // 先查询 dataset_id 用于发射信号
    QSqlQuery getQuery(db);
    getQuery.prepare("SELECT dataset_id FROM dataset_tags WHERE id = ?");
    getQuery.addBindValue(tagId);
    if (!getQuery.exec() || !getQuery.next()) {
        ltWarning(LT_LOG_DATASET()) << "removeTag: 标签不存在:" << tagId;
        return false;
    }
    QString datasetId = getQuery.value(0).toString();

    QSqlQuery query(db);
    query.prepare("DELETE FROM dataset_tags WHERE id = ?");
    query.addBindValue(tagId);

    if (!query.exec()) {
        ltError(LT_LOG_DATASET()) << "removeTag: 删除失败:" << query.lastError().text();
        return false;
    }

    if (query.numRowsAffected() == 0) {
        ltWarning(LT_LOG_DATASET()) << "removeTag: 标签不存在:" << tagId;
        return false;
    }

    ltInfo(LT_LOG_DATASET()) << "Tag removed:" << tagId;
    emit tagsChanged(datasetId);
    return true;
}

QVariantList TagService::listTags(const QString &datasetId) const
{
    ltTrace(LT_LOG_DATASET()) << "listTags datasetId=" << datasetId;

    QVariantList result;
    if (datasetId.isEmpty()) return result;

    auto db = Database::instance().database();
    if (!db.isOpen()) return result;

    QSqlQuery query(db);
    query.prepare(
        "SELECT id, dataset_id, name, shortcut, created_at "
        "FROM dataset_tags WHERE dataset_id = ? ORDER BY created_at ASC"
    );
    query.addBindValue(datasetId);

    if (!query.exec()) {
        ltError(LT_LOG_DATASET()) << "listTags: 查询失败:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        QVariantMap tag;
        tag["id"] = query.value(0).toString();
        tag["datasetId"] = query.value(1).toString();
        tag["name"] = query.value(2).toString();
        tag["shortcut"] = query.value(3).toString();
        tag["createdAt"] = query.value(4).toString();
        result.append(tag);
    }

    return result;
}

bool TagService::renameTag(const QString &tagId, const QString &newName)
{
    ltTrace(LT_LOG_DATASET()) << "renameTag tagId=" << tagId << "newName=" << newName;

    if (tagId.isEmpty() || newName.trimmed().isEmpty()) {
        ltWarning(LT_LOG_DATASET()) << "renameTag: 标签 ID 或新名称为空";
        return false;
    }

    auto db = Database::instance().database();
    if (!db.isOpen()) return false;

    // 先查询 dataset_id 用于发射信号
    QSqlQuery getQuery(db);
    getQuery.prepare("SELECT dataset_id FROM dataset_tags WHERE id = ?");
    getQuery.addBindValue(tagId);
    if (!getQuery.exec() || !getQuery.next()) {
        ltWarning(LT_LOG_DATASET()) << "renameTag: 标签不存在:" << tagId;
        return false;
    }
    QString datasetId = getQuery.value(0).toString();

    QSqlQuery query(db);
    query.prepare("UPDATE dataset_tags SET name = ? WHERE id = ?");
    query.addBindValue(newName.trimmed());
    query.addBindValue(tagId);

    if (!query.exec()) {
        ltError(LT_LOG_DATASET()) << "renameTag: 更新失败:" << query.lastError().text();
        return false;
    }

    if (query.numRowsAffected() == 0) {
        ltWarning(LT_LOG_DATASET()) << "renameTag: 标签不存在:" << tagId;
        return false;
    }

    ltInfo(LT_LOG_DATASET()) << "Tag renamed:" << tagId << "to:" << newName;
    emit tagsChanged(datasetId);
    return true;
}

bool TagService::tagExists(const QString &datasetId, const QString &name) const
{
    auto db = Database::instance().database();
    QSqlQuery query(db);
    query.prepare("SELECT id FROM dataset_tags WHERE dataset_id = ? AND name = ?");
    query.addBindValue(datasetId);
    query.addBindValue(name);
    return query.exec() && query.next();
}
