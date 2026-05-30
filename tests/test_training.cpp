#include <QTest>
#include <QCoreApplication>
#include <QSqlQuery>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <QThreadPool>
#include "Database.h"
#include "TrainingService.h"

class TestTraining : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
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
    QTemporaryDir m_tmpDir;
    QString m_imgDir;
    QString m_lblDir;
};

void TestTraining::initTestCase()
{
    QVERIFY2(m_tmpDir.isValid(), "无法创建临时目录");

    // 创建测试图片和标签目录
    m_imgDir = m_tmpDir.path() + "/img";
    m_lblDir = m_tmpDir.path() + "/lbl";
    QDir().mkpath(m_imgDir);
    QDir().mkpath(m_lblDir);

    // 创建5张测试图片和标签文件
    for (int i = 0; i < 5; ++i) {
        // 创建1x1像素的伪JPEG文件（足够通过QFile::exists检查）
        QFile imgFile(m_imgDir + QString("/%1.jpg").arg(i));
        QVERIFY(imgFile.open(QIODevice::WriteOnly));
        imgFile.write("fake");
        imgFile.close();

        QFile lblFile(m_lblDir + QString("/%1.txt").arg(i));
        QVERIFY(lblFile.open(QIODevice::WriteOnly));
        lblFile.write("0 0.5 0.5 0.1 0.1");
        lblFile.close();
    }

    QString dbPath = QCoreApplication::applicationDirPath() + "/test_training_db";
    QFile::remove(dbPath);
    QFile::remove(dbPath + "-journal");
    QFile::remove(dbPath + "-wal");

    Database::instance().open(dbPath);
    Database::instance().initializeSchema();

    auto db = Database::instance().database();

    // 创建项目
    m_projectId = "proj-train-test";
    QSqlQuery q(db);
    q.prepare("INSERT INTO projects (id, name, root_path) VALUES (?, ?, ?)");
    q.addBindValue(m_projectId);
    q.addBindValue("TrainTestProject");
    q.addBindValue(m_tmpDir.path());
    QVERIFY(q.exec());

    // 创建类别体系
    q.prepare("INSERT INTO taxonomies (id, project_id, name, version, class_definitions_json) VALUES (?, ?, ?, ?, ?)");
    q.addBindValue("tax-train-test");
    q.addBindValue(m_projectId);
    q.addBindValue("Default");
    q.addBindValue(1);
    q.addBindValue("[\"defect\"]");
    QVERIFY(q.exec());

    // 创建数据集
    m_datasetId = "ds-train-test";
    q.prepare("INSERT INTO datasets (id, project_id, name, image_root, label_root, format, sample_count, import_status) "
              "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(m_datasetId);
    q.addBindValue(m_projectId);
    q.addBindValue("TrainDataset");
    q.addBindValue(m_imgDir);
    q.addBindValue(m_lblDir);
    q.addBindValue("yolo_txt");
    q.addBindValue(5);
    q.addBindValue("completed");
    QVERIFY(q.exec());

    // 插入样本记录
    for (int i = 0; i < 5; ++i) {
        q.prepare("INSERT INTO dataset_samples (id, dataset_id, image_path, label_path, validation_status) VALUES (?, ?, ?, ?, ?)");
        q.addBindValue(QString("train-sample-%1").arg(i));
        q.addBindValue(m_datasetId);
        q.addBindValue(m_imgDir + QString("/%1.jpg").arg(i));
        q.addBindValue(m_lblDir + QString("/%1.txt").arg(i));
        q.addBindValue("valid");
        QVERIFY(q.exec());
    }

    // 创建快照
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

void TestTraining::cleanupTestCase()
{
    Database::instance().close();
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
    QThreadPool::globalInstance()->waitForDone();
    QCoreApplication::processEvents();

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
    QThreadPool::globalInstance()->waitForDone();
    QCoreApplication::processEvents();
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
    QThreadPool::globalInstance()->waitForDone();
    QCoreApplication::processEvents();

    QVERIFY(!service.deleteRun(runId));
}

void TestTraining::testUpdateRunStatus()
{
    TrainingService service;
    QString config = R"({"model_family":"yolov8"})";
    QString runId = service.createRun(m_projectId, m_snapshotId, config);
    QVERIFY(service.startTraining(runId));
    QThreadPool::globalInstance()->waitForDone();
    QCoreApplication::processEvents();

    QVERIFY(service.updateRunStatus(runId, "succeeded"));

    QVariantMap details = service.getRun(runId);
    QCOMPARE(details["status"].toString(), QString("succeeded"));
    QVERIFY(!details["finishedAt"].toString().isEmpty());
}

QTEST_MAIN(TestTraining)
#include "test_training.moc"
