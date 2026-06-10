#include <QTest>
#include <QCoreApplication>
#include <QSqlQuery>
#include <QFile>
#include "Database.h"
#include "SnapshotService.h"

class TestSnapshot : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testCreateSnapshot();
    void testListSnapshots();
    void testGetSnapshot();
    void testSplitRatio();
    void testDeleteSnapshot();
    void testDeleteSnapshotInUse();
    void testImmutable();

    // 异常场景测试
    void testCreateSnapshotEmptyDataset();
    void testCreateSnapshotNonexistentDataset();
    void testCreateSnapshotEdgeRatios();
    void testGetSnapshotNonexistent();
    void testListSnapshotsNonexistentDataset();
    void testDeleteSnapshotNonexistent();
    void testGetSampleManifestNonexistent();
    void testGetSplitManifestNonexistent();

private:
    QString m_projectId;
    QString m_datasetId;
    QString m_taxonomyId;
};

void TestSnapshot::initTestCase()
{
    QString dbPath = QCoreApplication::applicationDirPath() + "/test_snapshot_db";
    // Clean up from previous runs
    QFile::remove(dbPath);
    QFile::remove(dbPath + "-journal");
    QFile::remove(dbPath + "-wal");

    Database::instance().open(dbPath);
    Database::instance().initializeSchema();

    auto db = Database::instance().database();

    // Create a project
    m_projectId = "proj-snap-test";
    QSqlQuery q(db);
    q.prepare("INSERT INTO projects (id, name, root_path) VALUES (?, ?, ?)");
    q.addBindValue(m_projectId);
    q.addBindValue("SnapshotTestProject");
    q.addBindValue("/tmp/snaptest");
    QVERIFY(q.exec());

    // Create a taxonomy for the project (using auto-increment ID to avoid conflicts)
    m_taxonomyId = "tax-snap-test";
    q.prepare("INSERT INTO taxonomies (id, project_id, name, version, class_definitions_json) VALUES (?, ?, ?, ?, ?)");
    q.addBindValue(m_taxonomyId);
    q.addBindValue(m_projectId);
    q.addBindValue("Default");
    q.addBindValue(1);
    q.addBindValue("[\"defect\",\"scratch\"]");
    QVERIFY(q.exec());

    // Create a dataset
    m_datasetId = "ds-snap-test";
    q.prepare("INSERT INTO datasets (id, project_id, name, image_root, label_root, format, sample_count, import_status) "
              "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(m_datasetId);
    q.addBindValue(m_projectId);
    q.addBindValue("TestDataset");
    q.addBindValue("/tmp/images");
    q.addBindValue("/tmp/labels");
    q.addBindValue("yolo_txt");
    q.addBindValue(10);
    q.addBindValue("completed");
    QVERIFY(q.exec());

    // Insert 10 sample records
    for (int i = 0; i < 10; ++i) {
        q.prepare("INSERT INTO dataset_samples (id, dataset_id, image_path, label_path, validation_status) VALUES (?, ?, ?, ?, ?)");
        q.addBindValue(QString("sample-%1").arg(i));
        q.addBindValue(m_datasetId);
        q.addBindValue(QString("/tmp/images/%1.jpg").arg(i));
        q.addBindValue(QString("/tmp/labels/%1.txt").arg(i));
        q.addBindValue("valid");
        QVERIFY(q.exec());
    }
}

void TestSnapshot::testCreateSnapshot()
{
    SnapshotService service;
    QString snapshotId = service.createSnapshot(m_datasetId, 0.8, "random");
    QVERIFY(!snapshotId.isEmpty());
}

void TestSnapshot::testListSnapshots()
{
    SnapshotService service;
    QVariantList snapshots = service.listSnapshots(m_datasetId);
    QVERIFY(snapshots.size() >= 1);

    QVariantMap first = snapshots[0].toMap();
    QVERIFY(first.contains("id"));
    QVERIFY(first.contains("sampleCount"));
    QCOMPARE(first["sampleCount"].toInt(), 10);
}

void TestSnapshot::testGetSnapshot()
{
    SnapshotService service;
    QVariantList snapshots = service.listSnapshots(m_datasetId);
    QVERIFY(!snapshots.isEmpty());

    QString snapshotId = snapshots[0].toMap()["id"].toString();
    QVariantMap details = service.getSnapshot(snapshotId);

    QCOMPARE(details["sampleCount"].toInt(), 10);
    QVERIFY(details["trainCount"].toInt() > 0);
    QVERIFY(details["valCount"].toInt() > 0);
    QCOMPARE(details["trainCount"].toInt() + details["valCount"].toInt(), 10);
}

void TestSnapshot::testSplitRatio()
{
    SnapshotService service;
    // 50/50 split
    QString snapId = service.createSnapshot(m_datasetId, 0.5, "sequential");
    QVERIFY(!snapId.isEmpty());

    QVariantMap details = service.getSnapshot(snapId);
    QCOMPARE(details["trainCount"].toInt(), 5);
    QCOMPARE(details["valCount"].toInt(), 5);
}

void TestSnapshot::testDeleteSnapshot()
{
    SnapshotService service;
    QString snapId = service.createSnapshot(m_datasetId, 0.8, "random");
    QVERIFY(!snapId.isEmpty());

    QVERIFY(service.deleteSnapshot(snapId));

    QVariantMap details = service.getSnapshot(snapId);
    QVERIFY(details.isEmpty());
}

void TestSnapshot::testDeleteSnapshotInUse()
{
    SnapshotService service;
    QString snapId = service.createSnapshot(m_datasetId, 0.8, "random");
    QVERIFY(!snapId.isEmpty());

    // Create a training run that references this snapshot
    auto db = Database::instance().database();
    QSqlQuery q(db);
    q.prepare("INSERT INTO training_runs (id, project_id, snapshot_id, config_snapshot_json, status) VALUES (?, ?, ?, ?, ?)");
    q.addBindValue("run-block-delete");
    q.addBindValue(m_projectId);
    q.addBindValue(snapId);
    q.addBindValue("{}");
    q.addBindValue("draft");
    QVERIFY(q.exec());

    // Should fail to delete because it's in use
    QVERIFY(!service.deleteSnapshot(snapId));

    // Cleanup
    q.prepare("DELETE FROM training_runs WHERE id = ?");
    q.addBindValue("run-block-delete");
    q.exec();
}

void TestSnapshot::testImmutable()
{
    SnapshotService service;
    QString snapId = service.createSnapshot(m_datasetId, 0.8, "random");
    QVERIFY(!snapId.isEmpty());

    QVERIFY(service.isImmutable(snapId));
    QVERIFY(!service.isImmutable("nonexistent-id"));
}

// === 异常场景测试 ===

void TestSnapshot::testCreateSnapshotEmptyDataset()
{
    // 创建一个没有有效样本的数据集
    auto db = Database::instance().database();
    QString emptyDsId = "ds-empty-snap-test";
    QSqlQuery q(db);
    q.prepare("INSERT INTO datasets (id, project_id, name, image_root, label_root, format, sample_count, import_status) "
              "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(emptyDsId);
    q.addBindValue(m_projectId);
    q.addBindValue("EmptyDataset");
    q.addBindValue("/tmp/empty/img");
    q.addBindValue("/tmp/empty/lbl");
    q.addBindValue("yolo_txt");
    q.addBindValue(0);
    q.addBindValue("completed");
    QVERIFY(q.exec());

    // 空数据集应返回空字符串
    SnapshotService service;
    QString snapId = service.createSnapshot(emptyDsId, 0.8, "random");
    QVERIFY(snapId.isEmpty());
}

void TestSnapshot::testCreateSnapshotNonexistentDataset()
{
    // 不存在的数据集应返回空字符串
    SnapshotService service;
    QString snapId = service.createSnapshot("nonexistent-dataset-id", 0.8, "random");
    QVERIFY(snapId.isEmpty());
}

void TestSnapshot::testCreateSnapshotEdgeRatios()
{
    SnapshotService service;

    // trainRatio = 1.0：所有样本在训练集，验证集为空
    QString snapId1 = service.createSnapshot(m_datasetId, 1.0, "sequential");
    QVERIFY(!snapId1.isEmpty());
    QVariantMap details1 = service.getSnapshot(snapId1);
    QCOMPARE(details1["trainCount"].toInt(), 10);
    QCOMPARE(details1["valCount"].toInt(), 0);

    // trainRatio = 0.0：至少1个样本在训练集（qMax(1, ...)）
    QString snapId2 = service.createSnapshot(m_datasetId, 0.0, "sequential");
    QVERIFY(!snapId2.isEmpty());
    QVariantMap details2 = service.getSnapshot(snapId2);
    QCOMPARE(details2["trainCount"].toInt(), 1); // qMax(1, 0) = 1
    QCOMPARE(details2["valCount"].toInt(), 9);
}

void TestSnapshot::testGetSnapshotNonexistent()
{
    SnapshotService service;
    QVariantMap details = service.getSnapshot("nonexistent-snapshot-id");
    QVERIFY(details.isEmpty());
}

void TestSnapshot::testListSnapshotsNonexistentDataset()
{
    SnapshotService service;
    QVariantList snapshots = service.listSnapshots("nonexistent-dataset-id");
    QVERIFY(snapshots.isEmpty());
}

void TestSnapshot::testDeleteSnapshotNonexistent()
{
    SnapshotService service;
    // 不存在的快照应返回 false
    QVERIFY(!service.deleteSnapshot("nonexistent-snapshot-id"));
}

void TestSnapshot::testGetSampleManifestNonexistent()
{
    SnapshotService service;
    QVariantList manifest = service.getSampleManifest("nonexistent-snapshot-id");
    QVERIFY(manifest.isEmpty());
}

void TestSnapshot::testGetSplitManifestNonexistent()
{
    SnapshotService service;
    QVariantMap split = service.getSplitManifest("nonexistent-snapshot-id");
    QVERIFY(split.isEmpty());
}

QTEST_MAIN(TestSnapshot)
#include "test_snapshot.moc"
