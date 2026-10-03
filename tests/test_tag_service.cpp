#include <QTest>
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QFile>
#include "database/Database.h"
#include "TagService.h"

/**
 * @brief 图像 Tag 体系测试（对标 DLTools 数据模型）
 *
 * 覆盖：内置 Tag 播种与保护、Tag CRUD 校验（重名/超长/快捷键冲突）、
 * 样本打标/清除、跨数据集打标拒绝、删除 Tag 解除指派、V4→V5 迁移列存在性。
 */
class TestTagService : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testMigrationColumns();
    void testEnsureBuiltinTags();
    void testEnsureBuiltinTagsIdempotent();
    void testAddTagValidation();
    void testShortcutConflict();
    void testSetSampleTag();
    void testCrossDatasetAssignRejected();
    void testRemoveTagProtection();
    void testRemoveTagUnassignsSamples();
    void testDatasetLockRejectsTagging();
    void cleanupTestCase();

private:
    QString m_dbPath;
    TagService m_service;
    QString m_datasetA;
    QString m_datasetB;
    QString m_sampleA1;

    QString insertDataset(const QString &name);
    QString insertSample(const QString &datasetId, const QString &fileName);
};

void TestTagService::initTestCase()
{
    m_dbPath = QDir::tempPath() + "/labeltorch_tag_test.db";
    QFile::remove(m_dbPath);
    QVERIFY(Database::instance().open(m_dbPath));
    QVERIFY(Database::instance().initializeSchema());

    QSqlQuery query(Database::instance().database());
    query.prepare("INSERT INTO projects (id, name, root_path) VALUES (?, ?, ?)");
    query.addBindValue("proj-1");
    query.addBindValue("标签测试项目");
    query.addBindValue("/tmp/tag_test_project");
    QVERIFY(query.exec());

    m_datasetA = insertDataset("数据集A");
    m_datasetB = insertDataset("数据集B");
    QVERIFY(!m_datasetA.isEmpty());
    QVERIFY(!m_datasetB.isEmpty());
    m_sampleA1 = insertSample(m_datasetA, "a1.png");
    QVERIFY(!m_sampleA1.isEmpty());
}

QString TestTagService::insertDataset(const QString &name)
{
    QSqlQuery query(Database::instance().database());
    query.prepare("INSERT INTO datasets (id, project_id, name, image_root, label_root) "
                  "VALUES (?, ?, ?, ?, ?)");
    QString id = "ds-" + name;
    query.addBindValue(id);
    query.addBindValue("proj-1");
    query.addBindValue(name);
    query.addBindValue("/tmp/images");
    query.addBindValue("/tmp/labels");
    if (!query.exec()) {
        qWarning() << "insertDataset failed:" << query.lastError().text();
        return {};
    }
    return id;
}

QString TestTagService::insertSample(const QString &datasetId, const QString &fileName)
{
    QSqlQuery query(Database::instance().database());
    query.prepare("INSERT INTO dataset_samples (id, dataset_id, image_path) VALUES (?, ?, ?)");
    QString id = "sample-" + fileName;
    query.addBindValue(id);
    query.addBindValue(datasetId);
    query.addBindValue("/tmp/images/" + fileName);
    if (!query.exec()) {
        qWarning() << "insertSample failed:" << query.lastError().text();
        return {};
    }
    return id;
}

void TestTagService::testMigrationColumns()
{
    // V4→V5 迁移：dataset_samples.tag_id 与 dataset_tags.builtin 必须存在
    QSqlQuery query(Database::instance().database());
    QVERIFY(query.exec("SELECT tag_id FROM dataset_samples LIMIT 0"));
    QVERIFY(query.exec("SELECT builtin FROM dataset_tags LIMIT 0"));
    QVERIFY(query.exec("SELECT version FROM schema_version WHERE version = 5"));
}

void TestTagService::testEnsureBuiltinTags()
{
    QVERIFY(m_service.ensureBuiltinTags(m_datasetA));
    QVariantList tags = m_service.listTags(m_datasetA);
    QCOMPARE(tags.size(), 6);

    // 内置顺序与 builtin 标志
    QStringList expectedNames = {"默认", "良品", "漏检", "误检", "待定", "重要"};
    QStringList names;
    for (const QVariant &v : tags) {
        QVariantMap t = v.toMap();
        QVERIFY(t["builtin"].toBool());
        names << t["name"].toString();
    }
    QCOMPARE(names, expectedNames);
}

void TestTagService::testEnsureBuiltinTagsIdempotent()
{
    // 重复播种不产生新 Tag
    QVERIFY(m_service.ensureBuiltinTags(m_datasetA));
    QCOMPARE(m_service.listTags(m_datasetA).size(), 6);
}

void TestTagService::testAddTagValidation()
{
    // 正常添加
    QString id1 = m_service.addTag(m_datasetA, "划痕", "5");
    QVERIFY(!id1.isEmpty());

    // 重名拒绝
    QVERIFY(m_service.addTag(m_datasetA, "划痕").isEmpty());

    // 超长（>30）拒绝
    QVERIFY(m_service.addTag(m_datasetA, QString(31, 'x')).isEmpty());

    // 空名拒绝
    QVERIFY(m_service.addTag(m_datasetA, "  ").isEmpty());

    // 自定义 Tag 的 builtin=false
    QVariantList tags = m_service.listTags(m_datasetA);
    for (const QVariant &v : tags) {
        QVariantMap t = v.toMap();
        if (t["name"].toString() == "划痕")
            QVERIFY(!t["builtin"].toBool());
    }

    // 清理快捷键供冲突用例复用
    QVERIFY(m_service.removeTag(id1));
}

void TestTagService::testShortcutConflict()
{
    QString id1 = m_service.addTag(m_datasetA, "凹坑", "6");
    QVERIFY(!id1.isEmpty());

    // 同数据集内快捷键冲突拒绝（大小写不敏感）
    QVERIFY(m_service.addTag(m_datasetA, "气泡", "6").isEmpty());
    QVERIFY(m_service.addTag(m_datasetA, "气泡", "6 ").isEmpty());

    // 其它数据集的同名快捷键不受影响
    QVERIFY(!m_service.addTag(m_datasetB, "凹坑", "6").isEmpty());

    QVERIFY(m_service.removeTag(id1));
}

void TestTagService::testSetSampleTag()
{
    QVariantList tags = m_service.listTags(m_datasetA);
    QString defectId;
    for (const QVariant &v : tags) {
        if (v.toMap()["name"].toString() == "漏检")
            defectId = v.toMap()["id"].toString();
    }
    QVERIFY(!defectId.isEmpty());

    // 打标
    QVERIFY(m_service.setSampleTag(m_sampleA1, defectId));
    QCOMPARE(m_service.getSampleTagId(m_sampleA1), defectId);

    // 清除（传空串）
    QVERIFY(m_service.setSampleTag(m_sampleA1, QString()));
    QCOMPARE(m_service.getSampleTagId(m_sampleA1), QString());

    // 不存在的 Tag 拒绝
    QVERIFY(!m_service.setSampleTag(m_sampleA1, "nonexistent-tag"));
}

void TestTagService::testCrossDatasetAssignRejected()
{
    // 数据集B 的 Tag 不能打给数据集A 的样本
    QVariantList tagsB = m_service.listTags(m_datasetB);
    QVERIFY(!tagsB.isEmpty());
    QString tagB = tagsB.first().toMap()["id"].toString();
    QVERIFY(!m_service.setSampleTag(m_sampleA1, tagB));
    QCOMPARE(m_service.getSampleTagId(m_sampleA1), QString());
}

void TestTagService::testRemoveTagProtection()
{
    QVariantList tags = m_service.listTags(m_datasetA);
    QCOMPARE(tags.size(), 6);

    // 内置 Tag 拒绝删除与改名
    QString builtinId = tags.first().toMap()["id"].toString();
    QVERIFY(!m_service.removeTag(builtinId));
    QVERIFY(!m_service.renameTag(builtinId, "改名尝试"));

    // 内置 Tag 不参与重名校验阻断：自定义重名仍拒绝
    QVERIFY(m_service.addTag(m_datasetA, "重要").isEmpty());
    QCOMPARE(m_service.listTags(m_datasetA).size(), 6);
}

void TestTagService::testRemoveTagUnassignsSamples()
{
    // 添加自定义 Tag，打标到样本，删除后样本应回到未打标
    QString customId = m_service.addTag(m_datasetA, "油污");
    QVERIFY(!customId.isEmpty());
    QVERIFY(m_service.setSampleTag(m_sampleA1, customId));
    QCOMPARE(m_service.getSampleTagId(m_sampleA1), customId);

    QVERIFY(m_service.removeTag(customId));
    QCOMPARE(m_service.getSampleTagId(m_sampleA1), QString());
    QCOMPARE(m_service.listTags(m_datasetA).size(), 6);
}

void TestTagService::testDatasetLockRejectsTagging()
{
    // 锁定数据集后拒绝打标（对标 DLTools「数据集已被锁定, 操作失败」）
    QVariantList tags = m_service.listTags(m_datasetA);
    QString builtinId = tags.first().toMap()["id"].toString();

    QSqlQuery lock(Database::instance().database());
    lock.prepare("UPDATE datasets SET locked = 1 WHERE id = ?");
    lock.addBindValue(m_datasetA);
    QVERIFY(lock.exec());

    QVERIFY(!m_service.setSampleTag(m_sampleA1, builtinId));
    QCOMPARE(m_service.getSampleTagId(m_sampleA1), QString());
    QCOMPARE(m_service.setSamplesTag({m_sampleA1}, builtinId), 0);

    QSqlQuery unlock(Database::instance().database());
    unlock.prepare("UPDATE datasets SET locked = 0 WHERE id = ?");
    unlock.addBindValue(m_datasetA);
    QVERIFY(unlock.exec());

    // 解锁后恢复
    QVERIFY(m_service.setSampleTag(m_sampleA1, builtinId));
    QCOMPARE(m_service.getSampleTagId(m_sampleA1), builtinId);
    QVERIFY(m_service.setSampleTag(m_sampleA1, QString()));
}

void TestTagService::cleanupTestCase()
{
    Database::instance().close();
    QFile::remove(m_dbPath);
}

#include "test_tag_service.moc"

QTEST_MAIN(TestTagService)
