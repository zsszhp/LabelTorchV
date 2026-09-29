#include "TaxonomyService.h"
#include "database/Database.h"
#include "utils/Id.h"
#include "utils/Log.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QTextStream>
#include <QDateTime>

TaxonomyService::TaxonomyService(QObject *parent) : QObject(parent) {}

// ============================================================================
// P0-2 内部工具：错误状态管理
// ============================================================================

void TaxonomyService::setLastError(const QString &code, const QString &message)
{
    m_lastErrorCode = code;
    m_lastError = message;
    ltError(LT_LOG_TAXONOMY()) << "Taxonomy error [" << code << "]:" << message;
}

void TaxonomyService::clearLastError()
{
    m_lastErrorCode = QStringLiteral("OK");
    m_lastError.clear();
}

QString TaxonomyService::lastError() const
{
    return m_lastError;
}

QString TaxonomyService::lastErrorCode() const
{
    return m_lastErrorCode;
}

void TaxonomyService::writeRemovalAudit(const QString &taxonomyId, int classIndex,
                                         const QString &className, bool physical)
{
    // 审计写入 class_mapping_revisions：记录类别删除动作供追溯
    // 映射规则 JSON 中携带操作元信息，而非真正的类别映射
    QJsonObject rules;
    rules[QStringLiteral("action")] = physical ? QStringLiteral("physical_remove")
                                                : QStringLiteral("deprecate_class");
    rules[QStringLiteral("classIndex")] = classIndex;
    rules[QStringLiteral("className")] = className;
    rules[QStringLiteral("taxonomyId")] = taxonomyId;
    rules[QStringLiteral("timestamp")] = QDateTime::currentDateTime().toString(Qt::ISODate);

    // 查找该体系所属项目的任一数据集作为审计载体（class_mapping_revisions 需要 dataset_id）
    QSqlQuery dsQuery(Database::instance().database());
    dsQuery.prepare(
        "SELECT d.id FROM datasets d "
        "JOIN projects p ON d.project_id = p.id "
        "JOIN taxonomies t ON t.project_id = p.id "
        "WHERE t.id = ? LIMIT 1");
    dsQuery.addBindValue(taxonomyId);
    QString datasetId;
    if (dsQuery.exec() && dsQuery.next()) {
        datasetId = dsQuery.value(0).toString();
    }

    // 无数据集时用类别体系 ID 充当审计载体（SQLite 默认不强制外键）
    if (datasetId.isEmpty()) {
        datasetId = taxonomyId;
    }

    // source_schema_id：尝试取该数据集的导入 schema，否则用固定标记
    QString sourceSchemaId = QStringLiteral("taxonomy_service");
    QSqlQuery schemaQuery(Database::instance().database());
    schemaQuery.prepare("SELECT id FROM imported_label_schemas WHERE dataset_id = ? LIMIT 1");
    schemaQuery.addBindValue(datasetId);
    if (schemaQuery.exec() && schemaQuery.next()) {
        sourceSchemaId = schemaQuery.value(0).toString();
    }

    QString auditId = Id::generate();
    QSqlQuery query(Database::instance().database());
    query.prepare("INSERT INTO class_mapping_revisions "
                  "(id, dataset_id, source_schema_id, target_taxonomy_id, mapping_rules_json) "
                  "VALUES (?, ?, ?, ?, ?)");
    query.addBindValue(auditId);
    query.addBindValue(datasetId);
    query.addBindValue(sourceSchemaId);
    query.addBindValue(taxonomyId);
    query.addBindValue(QString::fromUtf8(QJsonDocument(rules).toJson(QJsonDocument::Compact)));

    if (!query.exec()) {
        // 审计写入失败不阻断主流程，但必须记录（表可能不存在）
        ltWarning(LT_LOG_TAXONOMY()) << "Failed to write class removal audit:" << query.lastError().text();
    }
}

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

bool TaxonomyService::removeClass(const QString &taxonomyId, int classIndex, bool forcePhysical)
{
    ltTrace(LT_LOG_TAXONOMY()) << "removeClass taxonomyId=" << taxonomyId
                               << "index=" << classIndex << "forcePhysical=" << forcePhysical;
    clearLastError();

    QVariantList classes = getClasses(taxonomyId);
    if (classIndex < 0 || classIndex >= classes.size()) {
        setLastError(QStringLiteral("E_INDEX_INVALID"),
                     QStringLiteral("类别索引 %1 越界（共 %2 个类别）")
                         .arg(classIndex).arg(classes.size()));
        return false;
    }

    QString className = classes[classIndex].toString();

    // 该索引位已废弃（空名）→ 重复删除无意义
    if (className.isEmpty()) {
        setLastError(QStringLiteral("E_ALREADY_DEPRECATED"),
                     QStringLiteral("索引 %1 的类别已废弃").arg(classIndex));
        return false;
    }

    if (!forcePhysical) {
        // 默认路径：废弃 + 保留 id 空位
        // 将该索引位置空名，数组长度不变，后续 class_id 不前移
        classes[classIndex] = QString();

        QJsonArray arr;
        for (const auto &c : classes) arr.append(c.toString());
        QString classesJson = QJsonDocument(arr).toJson(QJsonDocument::Compact);

        QSqlQuery query(Database::instance().database());
        query.prepare("UPDATE taxonomies SET class_definitions_json = ?, version = version + 1 WHERE id = ?");
        query.addBindValue(classesJson);
        query.addBindValue(taxonomyId);
        if (!query.exec()) {
            setLastError(QStringLiteral("E_DB_ERROR"),
                         QStringLiteral("更新类别定义失败：%1").arg(query.lastError().text()));
            return false;
        }

        writeRemovalAudit(taxonomyId, classIndex, className, false);
        ltInfo(LT_LOG_TAXONOMY()) << "Class deprecated at index" << classIndex
                                  << "(" << className << ") in taxonomy" << taxonomyId;
        return true;
    }

    // 物理删除路径：先检查影响面，有引用则拒绝
    QVariantMap impact = getRemoveImpact(taxonomyId, classIndex);
    int affectedSamples = impact[QStringLiteral("affectedSampleCount")].toInt();
    int affectedSnapshots = impact[QStringLiteral("affectedSnapshotCount")].toInt();

    if (affectedSamples > 0 || affectedSnapshots > 0) {
        // 有样本或快照引用该 class_id：物理删除会导致全量标签错类
        setLastError(QStringLiteral("E_IN_USE"),
                     QStringLiteral("类别「%1」仍被 %2 个样本、%3 个快照引用，"
                                    "物理删除会导致标签错类。请先通过类别映射向导"
                                    "（ClassMappingService）迁移标签，或使用「标记废弃」。")
                         .arg(className).arg(affectedSamples).arg(affectedSnapshots));
        return false;
    }

    // 无引用：安全物理删除（索引前移）
    classes.removeAt(classIndex);

    QJsonArray arr;
    for (const auto &c : classes) arr.append(c.toString());
    QString classesJson = QJsonDocument(arr).toJson(QJsonDocument::Compact);

    QSqlQuery query(Database::instance().database());
    query.prepare("UPDATE taxonomies SET class_definitions_json = ?, version = version + 1 WHERE id = ?");
    query.addBindValue(classesJson);
    query.addBindValue(taxonomyId);
    if (!query.exec()) {
        setLastError(QStringLiteral("E_DB_ERROR"),
                     QStringLiteral("更新类别定义失败：%1").arg(query.lastError().text()));
        return false;
    }

    writeRemovalAudit(taxonomyId, classIndex, className, true);
    ltInfo(LT_LOG_TAXONOMY()) << "Class physically removed at index" << classIndex
                              << "(" << className << ") from taxonomy" << taxonomyId;
    return true;
}

QVariantMap TaxonomyService::getRemoveImpact(const QString &taxonomyId, int classIndex)
{
    ltTrace(LT_LOG_TAXONOMY()) << "getRemoveImpact taxonomyId=" << taxonomyId << "index=" << classIndex;

    QVariantMap result;
    result[QStringLiteral("classIndex")] = classIndex;
    result[QStringLiteral("className")] = QString();
    result[QStringLiteral("affectedSampleCount")] = 0;
    result[QStringLiteral("affectedSnapshotCount")] = 0;
    result[QStringLiteral("affectedDatasetCount")] = 0;

    auto db = Database::instance().database();
    if (!db.isOpen()) return result;

    // 获取类别名
    QVariantList classes = getClasses(taxonomyId);
    if (classIndex >= 0 && classIndex < classes.size()) {
        result[QStringLiteral("className")] = classes[classIndex].toString();
    }

    // 1. 受影响快照数：taxonomy_version 以 "taxonomyId:v" 开头的快照
    {
        QSqlQuery q(db);
        q.prepare("SELECT COUNT(*) FROM dataset_snapshots WHERE taxonomy_version LIKE ?");
        q.addBindValue(taxonomyId + QStringLiteral(":%"));
        if (q.exec() && q.next()) {
            result[QStringLiteral("affectedSnapshotCount")] = q.value(0).toInt();
        }
    }

    // 2. 受影响数据集数：该类别体系所属项目下的数据集
    QStringList datasetIds;
    {
        QSqlQuery q(db);
        q.prepare(
            "SELECT d.id FROM datasets d "
            "JOIN projects p ON d.project_id = p.id "
            "JOIN taxonomies t ON t.project_id = p.id "
            "WHERE t.id = ?");
        q.addBindValue(taxonomyId);
        if (q.exec()) {
            while (q.next()) {
                datasetIds.append(q.value(0).toString());
            }
        }
    }
    result[QStringLiteral("affectedDatasetCount")] = datasetIds.size();

    // 3. 受影响样本数：扫描标签文件，统计含该 class_id 的样本
    //    YOLO 标签每行首字段为 class_id，需逐行匹配避免 "1" 误匹配 "10"
    int affectedSamples = 0;
    for (const QString &dsId : datasetIds) {
        QSqlQuery sampleQuery(db);
        sampleQuery.prepare(
            "SELECT id, label_path FROM dataset_samples "
            "WHERE dataset_id = ? AND label_path IS NOT NULL AND label_path != ''");
        sampleQuery.addBindValue(dsId);
        if (!sampleQuery.exec()) continue;

        while (sampleQuery.next()) {
            QString labelPath = sampleQuery.value(1).toString();
            QFile file(labelPath);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;

            bool hasClass = false;
            QTextStream in(&file);
            while (!in.atEnd() && !hasClass) {
                QString line = in.readLine().trimmed();
                if (line.isEmpty()) continue;
                // 取行首 token 作为 class_id，精确比较避免前缀误匹配
                int spacePos = line.indexOf(QChar(' '));
                QString idToken = (spacePos > 0) ? line.left(spacePos) : line;
                bool ok = false;
                int cid = idToken.toInt(&ok);
                if (ok && cid == classIndex) {
                    hasClass = true;
                }
            }
            file.close();
            if (hasClass) affectedSamples++;
        }
    }
    result[QStringLiteral("affectedSampleCount")] = affectedSamples;

    ltDebug(LT_LOG_TAXONOMY()) << "Remove impact for class" << classIndex
                               << "samples:" << affectedSamples
                               << "snapshots:" << result[QStringLiteral("affectedSnapshotCount")].toInt();
    return result;
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
