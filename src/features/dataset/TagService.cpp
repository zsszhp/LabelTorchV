#include "TagService.h"
#include "Database.h"
#include "utils/Id.h"
#include "utils/Log.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QStringList>

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
    // DLTools 同款约束：标签类别名称的长度应在 1 到 30 之间
    if (trimmedName.length() > 30) {
        ltWarning(LT_LOG_DATASET()) << "addTag: 标签名超长(>30) name=" << trimmedName;
        return {};
    }

    // 检查同名标签是否已存在
    if (tagExists(datasetId, trimmedName)) {
        ltWarning(LT_LOG_DATASET()) << "addTag: 标签已存在 datasetId=" << datasetId
                                    << "name=" << trimmedName;
        return {};
    }

    // 快捷键冲突校验（同数据集内唯一；快捷键存储为大写单字符）
    QString trimmedShortcut = shortcut.trimmed().toUpper();
    if (!trimmedShortcut.isEmpty() && shortcutExists(datasetId, trimmedShortcut)) {
        ltWarning(LT_LOG_DATASET()) << "addTag: 快捷键冲突 datasetId=" << datasetId
                                    << "shortcut=" << trimmedShortcut;
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
        "INSERT INTO dataset_tags (id, dataset_id, name, shortcut, builtin, created_at) "
        "VALUES (?, ?, ?, ?, 0, ?)"
    );
    query.addBindValue(tagId);
    query.addBindValue(datasetId);
    query.addBindValue(trimmedName);
    query.addBindValue(trimmedShortcut);
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

    // 先查询 dataset_id 与 builtin 标志
    QSqlQuery getQuery(db);
    getQuery.prepare("SELECT dataset_id, builtin FROM dataset_tags WHERE id = ?");
    getQuery.addBindValue(tagId);
    if (!getQuery.exec() || !getQuery.next()) {
        ltWarning(LT_LOG_DATASET()) << "removeTag: 标签不存在:" << tagId;
        return false;
    }
    QString datasetId = getQuery.value(0).toString();
    // DLTools 同款约束：内置 Tag 不允许删除
    if (getQuery.value(1).toInt() == 1) {
        ltWarning(LT_LOG_DATASET()) << "removeTag: 不允许删除内置Tag:" << tagId;
        return false;
    }

    // 先解除该 Tag 下所有样本的指派，再删除 Tag
    QSqlQuery unassign(db);
    unassign.prepare("UPDATE dataset_samples SET tag_id = NULL WHERE tag_id = ?");
    unassign.addBindValue(tagId);
    if (!unassign.exec()) {
        ltError(LT_LOG_DATASET()) << "removeTag: 解除样本指派失败:" << unassign.lastError().text();
        return false;
    }

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
    emit sampleTagsChanged(datasetId);
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
        "SELECT id, dataset_id, name, shortcut, builtin, created_at "
        "FROM dataset_tags WHERE dataset_id = ? ORDER BY builtin DESC, created_at ASC, rowid ASC"
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
        tag["builtin"] = query.value(4).toInt() == 1;
        tag["createdAt"] = query.value(5).toString();
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

    // 先查询 dataset_id 与 builtin 标志
    QSqlQuery getQuery(db);
    getQuery.prepare("SELECT dataset_id, builtin FROM dataset_tags WHERE id = ?");
    getQuery.addBindValue(tagId);
    if (!getQuery.exec() || !getQuery.next()) {
        ltWarning(LT_LOG_DATASET()) << "renameTag: 标签不存在:" << tagId;
        return false;
    }
    QString datasetId = getQuery.value(0).toString();
    // DLTools 同款约束：不允许修改内置 Tag 的名称
    if (getQuery.value(1).toInt() == 1) {
        ltWarning(LT_LOG_DATASET()) << "renameTag: 不允许修改内置Tag名称:" << tagId;
        return false;
    }

    QString trimmedName = newName.trimmed();
    if (trimmedName.length() > 30 || tagExists(datasetId, trimmedName)) {
        ltWarning(LT_LOG_DATASET()) << "renameTag: 名称超长或重名:" << trimmedName;
        return false;
    }

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

bool TagService::shortcutExists(const QString &datasetId, const QString &shortcut) const
{
    auto db = Database::instance().database();
    QSqlQuery query(db);
    query.prepare("SELECT id FROM dataset_tags WHERE dataset_id = ? AND shortcut = ? AND shortcut != ''");
    query.addBindValue(datasetId);
    query.addBindValue(shortcut);
    return query.exec() && query.next();
}

QString TagService::sampleDatasetId(const QString &sampleId) const
{
    auto db = Database::instance().database();
    QSqlQuery query(db);
    query.prepare("SELECT dataset_id FROM dataset_samples WHERE id = ?");
    query.addBindValue(sampleId);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return {};
}

bool TagService::ensureBuiltinTags(const QString &datasetId)
{
    ltTrace(LT_LOG_DATASET()) << "ensureBuiltinTags datasetId=" << datasetId;

    if (datasetId.isEmpty()) return false;

    auto db = Database::instance().database();
    if (!db.isOpen()) return false;

    // 已有任何 Tag（含内置）则不重复播种
    QSqlQuery countQuery(db);
    countQuery.prepare("SELECT COUNT(*) FROM dataset_tags WHERE dataset_id = ?");
    countQuery.addBindValue(datasetId);
    if (countQuery.exec() && countQuery.next() && countQuery.value(0).toInt() > 0) {
        return true;
    }

    // 内置评审 Tag，与 DLTools 对齐；顺序即展示顺序
    const QStringList builtinNames = {
        QString::fromUtf8("默认"), QString::fromUtf8("良品"), QString::fromUtf8("漏检"),
        QString::fromUtf8("误检"), QString::fromUtf8("待定"), QString::fromUtf8("重要")
    };

    QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    for (const QString &name : builtinNames) {
        QSqlQuery insert(db);
        insert.prepare(
            "INSERT OR IGNORE INTO dataset_tags (id, dataset_id, name, shortcut, builtin, created_at) "
            "VALUES (?, ?, ?, '', 1, ?)"
        );
        insert.addBindValue(Id::generate());
        insert.addBindValue(datasetId);
        insert.addBindValue(name);
        insert.addBindValue(now);
        if (!insert.exec()) {
            ltError(LT_LOG_DATASET()) << "ensureBuiltinTags: 播种失败:" << insert.lastError().text();
            return false;
        }
    }

    ltInfo(LT_LOG_DATASET()) << "Builtin tags seeded for dataset:" << datasetId;
    emit tagsChanged(datasetId);
    return true;
}

bool TagService::setSampleTag(const QString &sampleId, const QString &tagId)
{
    ltTrace(LT_LOG_DATASET()) << "setSampleTag sampleId=" << sampleId << "tagId=" << tagId;

    if (sampleId.isEmpty()) {
        ltWarning(LT_LOG_DATASET()) << "setSampleTag: 样本 ID 为空";
        return false;
    }

    auto db = Database::instance().database();
    if (!db.isOpen()) return false;

    // tagId 非空时校验 Tag 存在且属于同数据集，防止跨数据集指派
    if (!tagId.isEmpty()) {
        QSqlQuery check(db);
        check.prepare(
            "SELECT t.id FROM dataset_tags t "
            "JOIN dataset_samples s ON s.dataset_id = t.dataset_id "
            "WHERE t.id = ? AND s.id = ?"
        );
        check.addBindValue(tagId);
        check.addBindValue(sampleId);
        if (!check.exec() || !check.next()) {
            ltWarning(LT_LOG_DATASET()) << "setSampleTag: Tag 不存在或不属于样本所在数据集"
                                        << "sampleId=" << sampleId << "tagId=" << tagId;
            return false;
        }
    }

    QSqlQuery query(db);
    query.prepare("UPDATE dataset_samples SET tag_id = ? WHERE id = ?");
    query.addBindValue(tagId.isEmpty() ? QVariant() : tagId);
    query.addBindValue(sampleId);
    if (!query.exec()) {
        ltError(LT_LOG_DATASET()) << "setSampleTag: 更新失败:" << query.lastError().text();
        return false;
    }

    emit sampleTagsChanged(sampleDatasetId(sampleId));
    return true;
}

int TagService::setSamplesTag(const QVariantList &sampleIds, const QString &tagId)
{
    ltTrace(LT_LOG_DATASET()) << "setSamplesTag count=" << sampleIds.size() << "tagId=" << tagId;

    if (sampleIds.isEmpty()) return 0;

    auto db = Database::instance().database();
    if (!db.isOpen()) return 0;

    // 批量打标限同一数据集内（以首个样本为准），Tag 必须属于该数据集
    QString datasetId = sampleDatasetId(sampleIds.first().toString());
    if (datasetId.isEmpty()) {
        ltWarning(LT_LOG_DATASET()) << "setSamplesTag: 首个样本不存在";
        return 0;
    }

    if (!tagId.isEmpty()) {
        QSqlQuery check(db);
        check.prepare("SELECT id FROM dataset_tags WHERE id = ? AND dataset_id = ?");
        check.addBindValue(tagId);
        check.addBindValue(datasetId);
        if (!check.exec() || !check.next()) {
            ltWarning(LT_LOG_DATASET()) << "setSamplesTag: Tag 不存在或不属于该数据集 tagId=" << tagId;
            return 0;
        }
    }

    // 单条 UPDATE ... IN，避免逐样本触发信号
    QStringList placeholders;
    QVariantList ids;
    for (const QVariant &idVar : sampleIds) {
        QString sampleId = idVar.toString();
        if (sampleId.isEmpty()) continue;
        placeholders.append(QString("?"));
        ids.append(sampleId);
    }
    if (placeholders.isEmpty()) return 0;

    QSqlQuery query(db);
    query.prepare(QString("UPDATE dataset_samples SET tag_id = %1 WHERE id IN (%2)")
                      .arg(tagId.isEmpty() ? "NULL" : "?", placeholders.join(',')));
    if (!tagId.isEmpty()) query.addBindValue(tagId);
    for (const QVariant &id : ids) query.addBindValue(id);

    if (!query.exec()) {
        ltError(LT_LOG_DATASET()) << "setSamplesTag: 批量更新失败:" << query.lastError().text();
        return 0;
    }

    int updated = query.numRowsAffected();
    emit sampleTagsChanged(datasetId);
    return updated;
}

QString TagService::getSampleTagId(const QString &sampleId)
{
    if (sampleId.isEmpty()) return {};

    auto db = Database::instance().database();
    if (!db.isOpen()) return {};

    QSqlQuery query(db);
    query.prepare("SELECT tag_id FROM dataset_samples WHERE id = ?");
    query.addBindValue(sampleId);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return {};
}
