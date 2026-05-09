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

QTEST_MAIN(TestSnapshot)
#include "test_snapshot.moc"
