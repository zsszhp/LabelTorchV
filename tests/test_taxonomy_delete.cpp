/**
 * @file test_taxonomy_delete.cpp
 * @brief P0-2 类别删除防错位测试
 *
 * 期望行为：
 * 1. 删除中间类别后，其余类别的 class_id（索引）不得前移错位——
 *    默认路径应「废弃 + 保留 id 空位」
 * 2. 物理删除（forcePhysical=true）在类别仍被样本标签引用时必须拒绝，或返回影响面
 * 3. getRemoveImpact 必须能返回受影响样本/快照数量，供 UI 弹窗展示
 */
#include <QTest>
#include <QCoreApplication>
#include <QSqlQuery>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>

#include "Database.h"
#include "TaxonomyService.h"

namespace {

bool writeTextFile(const QString &path, const QByteArray &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(content);
    f.close();
    return true;
}

} // namespace

class TestTaxonomyDelete : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // P0-2 核心用例
    void testRemoveMiddleClassKeepsClassIds();
    void testPhysicalRemoveRejectedWhenReferenced();
    void testGetRemoveImpactReportsReferences();
    void testPhysicalRemoveAllowedWhenUnreferenced();
    void testRemoveDeprecatedTwiceFails();
    void testInvalidIndexRejected();

private:
    QTemporaryDir m_tmpDir;
    QString m_projectId;
    TaxonomyService *m_service = nullptr;
};

void TestTaxonomyDelete::initTestCase()
{
    QVERIFY2(m_tmpDir.isValid(), "无法创建临时目录");

    QString dbPath = m_tmpDir.path() + "/test_taxonomy_delete.db";
    QFile::remove(dbPath);
    QFile::remove(dbPath + "-journal");
    QFile::remove(dbPath + "-wal");
    Database::instance().open(dbPath);
    Database::instance().initializeSchema();

    m_projectId = "proj-tax-del";
    QSqlQuery q(Database::instance().database());
    q.prepare("INSERT INTO projects (id, name, root_path, task_type) VALUES (?, ?, ?, ?)");
    q.addBindValue(m_projectId);
    q.addBindValue("TaxDelProject");
    q.addBindValue(m_tmpDir.path() + "/proj");
    q.addBindValue("detect");
    QVERIFY(q.exec());

    m_service = new TaxonomyService(this);
}

// === P0-2 用例 ===

void TestTaxonomyDelete::testRemoveMiddleClassKeepsClassIds()
{
    // 三个类别：0=alpha, 1=beta, 2=gamma
    QVariantList classes;
    classes << "alpha" << "beta" << "gamma";
    QString taxId = m_service->createTaxonomy(m_projectId, "KeepIds", classes);
    QVERIFY(!taxId.isEmpty());

    // 默认删除中间类（废弃 + 保留 id 空位）
    QVERIFY2(m_service->removeClass(taxId, 1, false),
             qPrintable("默认删除应成功: " + m_service->lastError()));

    QVariantList after = m_service->getClasses(taxId);
    // 关键断言：数组长度不变，后续 class_id 不前移
    QCOMPARE(after.size(), 3);
    QCOMPARE(after[0].toString(), QString("alpha"));
    // 索引 1 被废弃（空名占位）
    QVERIFY2(after[1].toString().isEmpty(), "被删类别应置空名（废弃占位）");
    // 索引 2 仍然是 gamma，而不是前移成 beta
    QCOMPARE(after[2].toString(), QString("gamma"));
}

void TestTaxonomyDelete::testPhysicalRemoveRejectedWhenReferenced()
{
    QVariantList classes;
    classes << "alpha" << "beta" << "gamma";
    QString taxId = m_service->createTaxonomy(m_projectId, "RefusedPhysical", classes);
    QVERIFY(!taxId.isEmpty());

    // 构造引用：数据集样本的标签文件中含有 class_id=1
    QString lblDir = m_tmpDir.path() + "/proj/labels";
    QString lblPath = lblDir + "/ref.txt";
    QVERIFY(writeTextFile(lblPath, QByteArray("1 0.5 0.5 0.1 0.1\n")));

    QSqlQuery q(Database::instance().database());
    QString dsId = "ds-tax-del-ref";
    q.prepare("INSERT INTO datasets (id, project_id, name, image_root, label_root, format, "
              "sample_count, import_status) VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(dsId);
    q.addBindValue(m_projectId);
    q.addBindValue("RefDataset");
    q.addBindValue(m_tmpDir.path() + "/proj/images");
    q.addBindValue(lblDir);
    q.addBindValue("yolo_txt");
    q.addBindValue(1);
    q.addBindValue("completed");
    QVERIFY(q.exec());

    q.prepare("INSERT INTO dataset_samples (id, dataset_id, image_path, label_path, validation_status) "
              "VALUES (?, ?, ?, ?, ?)");
    q.addBindValue("sample-ref-1");
    q.addBindValue(dsId);
    q.addBindValue(m_tmpDir.path() + "/proj/images/ref.jpg");
    q.addBindValue(lblPath);
    q.addBindValue("valid");
    QVERIFY(q.exec());

    // 物理删除被引用的 class_id=1 必须拒绝
    bool ok = m_service->removeClass(taxId, 1, true);
    QVERIFY2(!ok, "物理删除被引用类别必须拒绝");
    QCOMPARE(m_service->lastErrorCode(), QString("E_IN_USE"));

    // 类别未被物理移除
    QVariantList after = m_service->getClasses(taxId);
    QCOMPARE(after.size(), 3);
    QCOMPARE(after[1].toString(), QString("beta"));
}

void TestTaxonomyDelete::testGetRemoveImpactReportsReferences()
{
    QVariantList classes;
    classes << "alpha" << "beta" << "gamma";
    QString taxId = m_service->createTaxonomy(m_projectId, "Impact", classes);
    QVERIFY(!taxId.isEmpty());

    // 该 taxonomy 所属项目下已有 ds-tax-del-ref（标签含 class_id=1）
    QVariantMap impact = m_service->getRemoveImpact(taxId, 1);
    QVERIFY2(impact.contains("affectedSampleCount"), "影响面必须含 affectedSampleCount");
    QVERIFY2(impact.contains("affectedSnapshotCount"), "影响面必须含 affectedSnapshotCount");
    QVERIFY2(impact.contains("affectedDatasetCount"), "影响面必须含 affectedDatasetCount");
    QVERIFY2(impact["affectedSampleCount"].toInt() >= 1,
             "含 class_id=1 的标签样本应被统计到影响面");
    QCOMPARE(impact["className"].toString(), QString("beta"));
    QCOMPARE(impact["classIndex"].toInt(), 1);
}

void TestTaxonomyDelete::testPhysicalRemoveAllowedWhenUnreferenced()
{
    QVariantList classes;
    classes << "solo0" << "solo1" << "solo2";
    QString taxId = m_service->createTaxonomy(m_projectId, "UnrefPhysical", classes);
    QVERIFY(!taxId.isEmpty());

    // 无任何标签引用 → 物理删除允许
    // 注意：getRemoveImpact 会按项目下数据集扫描标签，这里选一个标签中不含 class_id=2 的位置
    // 由于 ds-tax-del-ref 的标签只有 class_id=1，class_id=2 无引用
    bool ok = m_service->removeClass(taxId, 2, true);
    QVERIFY2(ok, qPrintable("无引用时物理删除应成功: " + m_service->lastError()));

    QVariantList after = m_service->getClasses(taxId);
    QCOMPARE(after.size(), 2);
    QCOMPARE(after[0].toString(), QString("solo0"));
    QCOMPARE(after[1].toString(), QString("solo1"));
}

void TestTaxonomyDelete::testRemoveDeprecatedTwiceFails()
{
    QVariantList classes;
    classes << "a" << "b" << "c";
    QString taxId = m_service->createTaxonomy(m_projectId, "TwiceDel", classes);
    QVERIFY(!taxId.isEmpty());

    QVERIFY(m_service->removeClass(taxId, 1, false));
    // 二次删除同一索引应报「已废弃」
    QVERIFY(!m_service->removeClass(taxId, 1, false));
    QCOMPARE(m_service->lastErrorCode(), QString("E_ALREADY_DEPRECATED"));
}

void TestTaxonomyDelete::testInvalidIndexRejected()
{
    QVariantList classes;
    classes << "x" << "y";
    QString taxId = m_service->createTaxonomy(m_projectId, "BadIndex", classes);
    QVERIFY(!taxId.isEmpty());

    QVERIFY(!m_service->removeClass(taxId, -1, false));
    QCOMPARE(m_service->lastErrorCode(), QString("E_INDEX_INVALID"));

    QVERIFY(!m_service->removeClass(taxId, 99, false));
    QCOMPARE(m_service->lastErrorCode(), QString("E_INDEX_INVALID"));
}

QTEST_MAIN(TestTaxonomyDelete)
#include "test_taxonomy_delete.moc"
