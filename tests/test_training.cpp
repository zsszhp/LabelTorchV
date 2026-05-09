#include <QTest>
#include <QCoreApplication>
#include <QSqlQuery>
#include <QFile>
#include "Database.h"
#include "TrainingService.h"

class TestTraining : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testCreateRun();
    void testListRuns();
    void testGetRun();
    void testStartTraining();
    void testStopTraining();
    void testDeleteRun();
    void testDeleteRunningRunFails();
    void testUpdateRunStatus();

private:
    QString m_projectId;
    QString m_datasetId;
    QString m_snapshotId;
};

void TestTraining::initTestCase()
{
    QString dbPath = QCoreApplication::applicationDirPath() + "/test_training_db";
    QFile::remove(dbPath);
    QFile::remove(dbPath + "-journal");
    QFile::remove(dbPath + "-wal");

    Database::instance().open(dbPath);
    Database::instance().initializeSchema();

    auto db = Database::instance().database();

    // Create project
    m_projectId = "proj-train-test";
    QSqlQuery q(db);
    q.prepare("INSERT INTO projects (id, name, root_path) VALUES (?, ?, ?)");
    q.addBindValue(m_projectId);
    q.addBindValue("TrainTestProject");
    q.addBindValue("/tmp/traintest");
    QVERIFY(q.exec());

    // Create taxonomy
    q.prepare("INSERT INTO taxonomies (id, project_id, name, version, class_definitions_json) VALUES (?, ?, ?, ?, ?)");
    q.addBindValue("tax-train-test");
    q.addBindValue(m_projectId);
    q.addBindValue("Default");
    q.addBindValue(1);
    q.addBindValue("[\"defect\"]");
    QVERIFY(q.exec());

    // Create dataset
    m_datasetId = "ds-train-test";
    q.prepare("INSERT INTO datasets (id, project_id, name, image_root, label_root, format, sample_count, import_status) "
              "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(m_datasetId);
    q.addBindValue(m_projectId);
    q.addBindValue("TrainDataset");
    q.addBindValue("/tmp/img");
    q.addBindValue("/tmp/lbl");
    q.addBindValue("yolo_txt");
    q.addBindValue(5);
    q.addBindValue("completed");
    QVERIFY(q.exec());

    // Insert sample records
    for (int i = 0; i < 5; ++i) {
        q.prepare("INSERT INTO dataset_samples (id, dataset_id, image_path, label_path, validation_status) VALUES (?, ?, ?, ?, ?)");
        q.addBindValue(QString("train-sample-%1").arg(i));
        q.addBindValue(m_datasetId);
        q.addBindValue(QString("/tmp/img/%1.jpg").arg(i));
        q.addBindValue(QString("/tmp/lbl/%1.txt").arg(i));
        q.addBindValue("valid");
        QVERIFY(q.exec());
    }

    // Create snapshot
    q.prepare("INSERT INTO dataset_snapshots (id, dataset_id, sample_manifest_json, split_manifest_json, taxonomy_version, annotation_revision_boundary) "
              "VALUES (?, ?, ?, ?, ?, ?)");
    m_snapshotId = "snap-train-test";
    q.addBindValue(m_snapshotId);
    q.addBindValue(m_datasetId);
    q.addBindValue("[\"train-sample-0\",\"train-sample-1\",\"train-sample-2\",\"train-sample-3\",\"train-sample-4\"]");
    q.addBindValue("{\"train\":[\"train-sample-0\",\"train-sample-1\",\"train-sample-2\",\"train-sample-3\"],\"val\":[\"train-sample-4\"]}");
    q.addBindValue("tax-train-test:v1");
    q.addBindValue("none");
    QVERIFY(q.exec());
}

void TestTraining::testCreateRun()
{
    TrainingService service;
    QString config = R"({"model_family":"yolov8","epochs":10,"batch":16})";
    QString runId = service.createRun(m_projectId, m_snapshotId, config);
    QVERIFY(!runId.isEmpty());
}

void TestTraining::testListRuns()
{
    TrainingService service;
    QVariantList runs = service.listRuns(m_projectId);
    QVERIFY(runs.size() >= 1);

    QVariantMap first = runs[0].toMap();
    QCOMPARE(first["status"].toString(), QString("draft"));
}

void TestTraining::testGetRun()
{
    TrainingService service;
    QVariantList runs = service.listRuns(m_projectId);
    QVERIFY(!runs.isEmpty());

    QString runId = runs[0].toMap()["id"].toString();
    QVariantMap details = service.getRun(runId);
    QVERIFY(!details.isEmpty());
    QCOMPARE(details["status"].toString(), QString("draft"));
    QVERIFY(details["configJson"].toString().contains("yolov8"));
}

void TestTraining::testStartTraining()
{
    TrainingService service;
    QString config = R"({"model_family":"yolov8","epochs":5})";
    QString runId = service.createRun(m_projectId, m_snapshotId, config);
    QVERIFY(!runId.isEmpty());

    QVERIFY(service.startTraining(runId));

    QVariantMap details = service.getRun(runId);
    QCOMPARE(details["status"].toString(), QString("running"));
    QVERIFY(!details["startedAt"].toString().isEmpty());
}

void TestTraining::testStopTraining()
{
    TrainingService service;
    QString config = R"({"model_family":"yolov8","epochs":5})";
    QString runId = service.createRun(m_projectId, m_snapshotId, config);
    QVERIFY(service.startTraining(runId));
    QVERIFY(service.stopTraining(runId));

    QVariantMap details = service.getRun(runId);
    QCOMPARE(details["status"].toString(), QString("cancelled"));
    QVERIFY(!details["finishedAt"].toString().isEmpty());
}

void TestTraining::testDeleteRun()
{
    TrainingService service;
    QString config = R"({"model_family":"yolov8"})";
    QString runId = service.createRun(m_projectId, m_snapshotId, config);
    QVERIFY(!runId.isEmpty());

    // Draft can be deleted
    QVERIFY(service.deleteRun(runId));

    QVariantMap details = service.getRun(runId);
    QVERIFY(details.isEmpty());
}

void TestTraining::testDeleteRunningRunFails()
{
    TrainingService service;
    QString config = R"({"model_family":"yolov8"})";
    QString runId = service.createRun(m_projectId, m_snapshotId, config);
    QVERIFY(service.startTraining(runId));

    // Running run cannot be deleted
    QVERIFY(!service.deleteRun(runId));
}

void TestTraining::testUpdateRunStatus()
{
    TrainingService service;
    QString config = R"({"model_family":"yolov8"})";
    QString runId = service.createRun(m_projectId, m_snapshotId, config);
    QVERIFY(service.startTraining(runId));

    // Mark as succeeded
    QVERIFY(service.updateRunStatus(runId, "succeeded"));

    QVariantMap details = service.getRun(runId);
    QCOMPARE(details["status"].toString(), QString("succeeded"));
    QVERIFY(!details["finishedAt"].toString().isEmpty());
}

QTEST_MAIN(TestTraining)
#include "test_training.moc"
