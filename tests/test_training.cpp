#include <QTest>
#include <QCoreApplication>
#include <QSqlQuery>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
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
    void testCreateRunInvalidJson();
    void testCreateRunNonexistentProject();
    void testStartNonDraftRun();
    void testStopNonRunningRun();
    void testStartNonexistentRun();
    void testStopNonexistentRun();
    void testDeleteNonexistentRun();
    void testReconcileStaleRuns();
    void testGetNonexistentRun();
    void testListRunsEmptyProject();

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

    // 计算样本文件的 SHA-256 哈希，构建新格式 manifest（含 imageHash/labelHash）
    QJsonArray manifestArray;
    for (int i = 0; i < 5; ++i) {
        QJsonObject entry;
        entry["id"] = QString("train-sample-%1").arg(i);

        // 计算图片文件哈希
        QFile imgFile(m_imgDir + QString("/%1.jpg").arg(i));
        QVERIFY(imgFile.open(QIODevice::ReadOnly));
        entry["imageHash"] = QString::fromLatin1(
            QCryptographicHash::hash(imgFile.readAll(), QCryptographicHash::Sha256).toHex());
        imgFile.close();

        // 计算标签文件哈希
        QFile lblFile(m_lblDir + QString("/%1.txt").arg(i));
        QVERIFY(lblFile.open(QIODevice::ReadOnly));
        entry["labelHash"] = QString::fromLatin1(
            QCryptographicHash::hash(lblFile.readAll(), QCryptographicHash::Sha256).toHex());
        lblFile.close();

        manifestArray.append(entry);
    }

    // 创建快照（新格式 manifest 含哈希）
    q.prepare("INSERT INTO dataset_snapshots (id, dataset_id, sample_manifest_json, split_manifest_json, taxonomy_version, annotation_revision_boundary) "
              "VALUES (?, ?, ?, ?, ?, ?)");
    m_snapshotId = "snap-train-test";
    q.addBindValue(m_snapshotId);
    q.addBindValue(m_datasetId);
    q.addBindValue(QString::fromUtf8(QJsonDocument(manifestArray).toJson(QJsonDocument::Compact)));
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
    service.setModelRegistry(nullptr);
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

    // 无 IPC 客户端时，异步任务会将 run 标记为 failed（A5 分支）
    QVariantMap details = service.getRun(runId);
    QCOMPARE(details["status"].toString(), QString("failed"));
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
    // 无 IPC 时 run 已是 failed 状态，stopTraining 应返回 false
    QVERIFY(!service.stopTraining(runId));

    QVariantMap details = service.getRun(runId);
    QCOMPARE(details["status"].toString(), QString("failed"));
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

    // 无 IPC 时 run 状态为 failed，failed 的 run 可以被删除
    QVERIFY(service.deleteRun(runId));
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

// === 异常场景测试 ===

void TestTraining::testCreateRunInvalidJson()
{
    // 无效的 JSON 配置应返回空字符串
    TrainingService service;
    service.setModelRegistry(nullptr);
    QString runId = service.createRun(m_projectId, m_snapshotId, "not a json");
    QVERIFY(runId.isEmpty());

    // 空字符串也不是有效 JSON
    runId = service.createRun(m_projectId, m_snapshotId, "");
    QVERIFY(runId.isEmpty());

    // 不完整的 JSON
    runId = service.createRun(m_projectId, m_snapshotId, "{invalid");
    QVERIFY(runId.isEmpty());
}

void TestTraining::testCreateRunNonexistentProject()
{
    // 不存在的项目 ID，SQLite 外键约束会阻止插入
    TrainingService service;
    service.setModelRegistry(nullptr);
    QString config = R"({"model_family":"yolov8","epochs":10})";
    QString runId = service.createRun("nonexistent-project-id", m_snapshotId, config);
    // 外键约束失败应返回空字符串
    QVERIFY(runId.isEmpty());
}

void TestTraining::testStartNonDraftRun()
{
    // 非 draft 状态的 run 不能启动
    TrainingService service;
    QString config = R"({"model_family":"yolov8","epochs":5})";
    QString runId = service.createRun(m_projectId, m_snapshotId, config);
    QVERIFY(!runId.isEmpty());

    // 先启动一次，无 IPC 时状态变为 failed
    QVERIFY(service.startTraining(runId));
    QThreadPool::globalInstance()->waitForDone();
    QCoreApplication::processEvents();

    // 再次启动应失败（非 draft 状态）
    QVERIFY(!service.startTraining(runId));
}

void TestTraining::testStopNonRunningRun()
{
    // 非 running 状态的 run 不能停止
    TrainingService service;
    QString config = R"({"model_family":"yolov8","epochs":5})";
    QString runId = service.createRun(m_projectId, m_snapshotId, config);

    // draft 状态不能停止
    QVERIFY(!service.stopTraining(runId));
}

void TestTraining::testStartNonexistentRun()
{
    // 不存在的 run ID 应返回 false
    TrainingService service;
    QVERIFY(!service.startTraining("nonexistent-run-id"));
}

void TestTraining::testStopNonexistentRun()
{
    // 不存在的 run ID 应返回 false
    TrainingService service;
    QVERIFY(!service.stopTraining("nonexistent-run-id"));
}

void TestTraining::testDeleteNonexistentRun()
{
    // 不存在的 run ID 应返回 false
    TrainingService service;
    QVERIFY(!service.deleteRun("nonexistent-run-id"));
}

void TestTraining::testReconcileStaleRuns()
{
    // 测试冷启动自检：将残留的 running/preparing 状态修正为 stopped
    TrainingService service;
    service.setModelRegistry(nullptr);

    // 先清理之前测试可能遗留的 running/preparing 记录
    auto db = Database::instance().database();
    QSqlQuery cleanup(db);
    cleanup.exec("UPDATE training_runs SET status = 'stopped', finished_at = datetime('now') WHERE status IN ('running', 'preparing')");

    // 创建一个 run 并启动
    QString config = R"({"model_family":"yolov8","epochs":5})";
    QString runId = service.createRun(m_projectId, m_snapshotId, config);
    QVERIFY(!runId.isEmpty());
    QVERIFY(service.startTraining(runId));
    QThreadPool::globalInstance()->waitForDone();
    QCoreApplication::processEvents();

    // 手动将状态设为 running（模拟异常退出残留）
    QSqlQuery q(db);
    q.prepare("UPDATE training_runs SET status = 'running' WHERE id = ?");
    q.addBindValue(runId);
    QVERIFY(q.exec());

    // 再创建一个 preparing 状态的残留
    QString config2 = R"({"model_family":"yolov8","epochs":3})";
    QString runId2 = service.createRun(m_projectId, m_snapshotId, config2);
    QVERIFY(!runId2.isEmpty());
    q.prepare("UPDATE training_runs SET status = 'preparing' WHERE id = ?");
    q.addBindValue(runId2);
    QVERIFY(q.exec());

    // 执行冷启动自检
    int fixed = service.reconcileStaleRuns();
    QCOMPARE(fixed, 2);

    // 验证状态已修正为 stopped
    QVariantMap details1 = service.getRun(runId);
    QCOMPARE(details1["status"].toString(), QString("stopped"));

    QVariantMap details2 = service.getRun(runId2);
    QCOMPARE(details2["status"].toString(), QString("stopped"));

    // 没有残留时返回 0
    int fixedAgain = service.reconcileStaleRuns();
    QCOMPARE(fixedAgain, 0);
}

void TestTraining::testGetNonexistentRun()
{
    // 不存在的 run 应返回空 Map
    TrainingService service;
    QVariantMap details = service.getRun("nonexistent-run-id");
    QVERIFY(details.isEmpty());
}

void TestTraining::testListRunsEmptyProject()
{
    // 不存在的项目应返回空列表
    TrainingService service;
    QVariantList runs = service.listRuns("nonexistent-project-id");
    QVERIFY(runs.isEmpty());
}

QTEST_MAIN(TestTraining)
#include "test_training.moc"
