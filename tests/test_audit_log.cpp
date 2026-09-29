// P2-12 审计落地与指标历史双路兼容测试：
// 1. AuditLog 统一写入 task_events 并可查询
// 2. ProjectService 删除项目 / DatasetService 删除数据集 / TrainingService 启停 写审计
// 3. MetricService.getMetricHistory 优先读 run_metrics，空时回退 task_events.epoch_complete
#include <QTest>
#include <QCoreApplication>
#include <QSqlQuery>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QJsonDocument>
#include <QJsonObject>

#include "Database.h"
#include "utils/AuditLog.h"
#include "ProjectService.h"
#include "DatasetService.h"
#include "TrainingService.h"
#include "MetricService.h"

class TestAuditLog : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testAuditLogRecordAndList();
    void testAuditLogRejectsEmptyArgs();
    void testDeleteProjectWritesAudit();
    void testDeleteDatasetWritesAudit();
    void testTrainingStartStopWritesAudit();
    void testMetricHistoryPrefersRunMetrics();
    void testMetricHistoryFallsBackToTaskEvents();

private:
    QTemporaryDir m_tmp;
    QString m_projectId;
    QString m_datasetId;
    QString m_snapshotId;

    int countEvents(const QString &taskType, const QString &taskId, const QString &eventType);
};

void TestAuditLog::initTestCase()
{
    QString dbPath = QCoreApplication::applicationDirPath() + "/test_audit_log_db";
    QFile::remove(dbPath);
    QFile::remove(dbPath + "-journal");
    QFile::remove(dbPath + "-wal");

    Database::instance().open(dbPath);
    QVERIFY(Database::instance().initializeSchema());
    QVERIFY(m_tmp.isValid());
}

int TestAuditLog::countEvents(const QString &taskType, const QString &taskId, const QString &eventType)
{
    QSqlQuery q(Database::instance().database());
    q.prepare("SELECT COUNT(*) FROM task_events WHERE task_type = ? AND task_id = ? AND event_type = ?");
    q.addBindValue(taskType);
    q.addBindValue(taskId);
    q.addBindValue(eventType);
    if (q.exec() && q.next()) return q.value(0).toInt();
    return -1;
}

void TestAuditLog::testAuditLogRecordAndList()
{
    QVariantMap payload;
    payload["name"] = "demo";
    payload["count"] = 3;

    QVERIFY(AuditLog::record("project", "proj-audit-1", "created", payload));
    QVERIFY(AuditLog::record("project", "proj-audit-1", "deleted"));

    QVariantList events = AuditLog::listEvents("project", "proj-audit-1");
    QCOMPARE(events.size(), 2);

    // 倒序：最新在前
    QVariantMap first = events[0].toMap();
    QCOMPARE(first["eventType"].toString(), QString("deleted"));
    QVariantMap second = events[1].toMap();
    QCOMPARE(second["eventType"].toString(), QString("created"));
    QCOMPARE(second["payload"].toMap()["name"].toString(), QString("demo"));
    QCOMPARE(second["payload"].toMap()["count"].toInt(), 3);
}

void TestAuditLog::testAuditLogRejectsEmptyArgs()
{
    QVERIFY(!AuditLog::record("", "x", "created"));
    QVERIFY(!AuditLog::record("project", "", "created"));
    QVERIFY(!AuditLog::record("project", "x", ""));
}

void TestAuditLog::testDeleteProjectWritesAudit()
{
    auto db = Database::instance().database();
    m_projectId = "proj-audit-del";
    m_datasetId = "ds-audit-del";

    QSqlQuery q(db);
    q.prepare("INSERT INTO projects (id, name, root_path, task_type) VALUES (?, ?, ?, 'detect')");
    q.addBindValue(m_projectId);
    q.addBindValue("审计项目");
    q.addBindValue(m_tmp.path() + "/audit_proj");
    QVERIFY(q.exec());

    q.prepare("INSERT INTO datasets (id, project_id, name, image_root, label_root, format, sample_count, import_status) "
              "VALUES (?, ?, ?, '', '', 'yolo_txt', 0, 'completed')");
    q.addBindValue(m_datasetId);
    q.addBindValue(m_projectId);
    q.addBindValue("审计数据集");
    QVERIFY(q.exec());

    ProjectService service;
    QVERIFY(service.deleteProject(m_projectId));

    QCOMPARE(countEvents("project", m_projectId, "deleted"), 1);

    // payload 含删除前捕获的名称与级联摘要
    QVariantList events = AuditLog::listEvents("project", m_projectId);
    QCOMPARE(events.size(), 1);
    QVariantMap payload = events[0].toMap()["payload"].toMap();
    QCOMPARE(payload["projectName"].toString(), QString("审计项目"));
    QVERIFY(payload.contains("cascade"));
    QCOMPARE(payload["cascade"].toMap()["datasetCount"].toInt(), 1);
}

void TestAuditLog::testDeleteDatasetWritesAudit()
{
    auto db = Database::instance().database();
    QString projectId = "proj-audit-ds";
    QString datasetId = "ds-audit-del2";

    QSqlQuery q(db);
    q.prepare("INSERT INTO projects (id, name, root_path) VALUES (?, ?, ?)");
    q.addBindValue(projectId);
    q.addBindValue("数据集审计项目");
    q.addBindValue(m_tmp.path() + "/audit_ds_proj");
    QVERIFY(q.exec());

    q.prepare("INSERT INTO datasets (id, project_id, name, image_root, label_root, format, sample_count, import_status) "
              "VALUES (?, ?, ?, '', '', 'yolo_txt', 0, 'completed')");
    q.addBindValue(datasetId);
    q.addBindValue(projectId);
    q.addBindValue("待删数据集");
    QVERIFY(q.exec());

    DatasetService service;
    QVERIFY(service.deleteDataset(datasetId));

    QCOMPARE(countEvents("dataset", datasetId, "deleted"), 1);
    QVariantList events = AuditLog::listEvents("dataset", datasetId);
    QCOMPARE(events[0].toMap()["payload"].toMap()["datasetName"].toString(), QString("待删数据集"));
}

void TestAuditLog::testTrainingStartStopWritesAudit()
{
    auto db = Database::instance().database();
    QString projectId = "proj-audit-train";
    QString datasetId = "ds-audit-train";
    QString snapshotId = "snap-audit-train";

    QSqlQuery q(db);
    q.prepare("INSERT INTO projects (id, name, root_path) VALUES (?, ?, ?)");
    q.addBindValue(projectId);
    q.addBindValue("训练审计项目");
    q.addBindValue(m_tmp.path() + "/audit_train_proj");
    QVERIFY(q.exec());

    q.prepare("INSERT INTO datasets (id, project_id, name, image_root, label_root, format, sample_count, import_status) "
              "VALUES (?, ?, ?, '', '', 'yolo_txt', 0, 'completed')");
    q.addBindValue(datasetId);
    q.addBindValue(projectId);
    q.addBindValue("训练审计数据集");
    QVERIFY(q.exec());

    q.prepare("INSERT INTO dataset_snapshots (id, dataset_id, sample_manifest_json) VALUES (?, ?, '[]')");
    q.addBindValue(snapshotId);
    q.addBindValue(datasetId);
    QVERIFY(q.exec());

    TrainingService service;
    QString runId = service.createRun(projectId, snapshotId, R"({"epochs":1})");
    QVERIFY(!runId.isEmpty());

    // 无 IPC 客户端时启动会走向 failed，但审计事件仍应先落库
    service.startTraining(runId);
    QCOMPARE(countEvents("training", runId, "train_start"), 1);

    // 等待 startTraining 的并发快照准备收尾，避免用例返回后访问已析构服务
    QThreadPool::globalInstance()->waitForDone();
    QCoreApplication::processEvents();

    // 状态可能已变为 preparing/failed；仅 running/preparing 可停，这里只验证 stop 路径审计
    QSqlQuery upd(db);
    upd.prepare("UPDATE training_runs SET status = 'running' WHERE id = ?");
    upd.addBindValue(runId);
    QVERIFY(upd.exec());

    // 无 IPC 时 stopTraining 直接标记 stopped 并返回 true
    QVERIFY(service.stopTraining(runId));
    QCOMPARE(countEvents("training", runId, "train_stop"), 1);
}

void TestAuditLog::testMetricHistoryPrefersRunMetrics()
{
    auto db = Database::instance().database();
    QString runId = "run-metric-audit";

    QSqlQuery q(db);
    q.prepare("INSERT INTO projects (id, name, root_path) VALUES (?, ?, ?)");
    q.addBindValue("proj-metric-audit");
    q.addBindValue("指标项目");
    q.addBindValue(m_tmp.path() + "/audit_metric_proj");
    QVERIFY(q.exec());

    q.prepare("INSERT INTO datasets (id, project_id, name, image_root, label_root, format, sample_count, import_status) "
              "VALUES ('ds-metric-audit', 'proj-metric-audit', 'd', '', '', 'yolo_txt', 0, 'completed')");
    QVERIFY(q.exec());

    q.prepare("INSERT INTO dataset_snapshots (id, dataset_id, sample_manifest_json) VALUES ('snap-metric-audit', 'ds-metric-audit', '[]')");
    QVERIFY(q.exec());

    q.prepare("INSERT INTO training_runs (id, project_id, snapshot_id, config_snapshot_json, status) "
              "VALUES (?, 'proj-metric-audit', 'snap-metric-audit', '{}', 'running')");
    q.addBindValue(runId);
    QVERIFY(q.exec());

    // 双路干扰项：task_events 里也有 epoch_complete，应被 run_metrics 优先
    QVariantMap legacy;
    legacy["epoch"] = 99;
    legacy["mAP50"] = 0.11;
    QVERIFY(AuditLog::record("training", runId, "epoch_complete", legacy));

    MetricService metricService;
    QVERIFY(metricService.storeEpochMetrics(runId, 1, {{"loss", 0.5}, {"mAP50", 0.8}}));
    QVERIFY(metricService.storeEpochMetrics(runId, 2, {{"loss", 0.3}, {"mAP50", 0.9}}));

    QVariantList history = metricService.getMetricHistory(runId);
    QCOMPARE(history.size(), 2);

    QVariantMap e1 = history[0].toMap();
    QCOMPARE(e1["epoch"].toInt(), 1);
    QCOMPARE(e1["loss"].toDouble(), 0.5);
    QCOMPARE(e1["mAP50"].toDouble(), 0.8);

    QVariantMap e2 = history[1].toMap();
    QCOMPARE(e2["epoch"].toInt(), 2);
    QCOMPARE(e2["mAP50"].toDouble(), 0.9);
}

void TestAuditLog::testMetricHistoryFallsBackToTaskEvents()
{
    QString runId = "run-metric-fallback";

    auto db = Database::instance().database();
    QSqlQuery q(db);
    q.prepare("INSERT INTO training_runs (id, project_id, snapshot_id, config_snapshot_json, status) "
              "VALUES (?, 'proj-metric-audit', 'snap-metric-audit', '{}', 'succeeded')");
    q.addBindValue(runId);
    QVERIFY(q.exec());

    // run_metrics 无数据，仅 task_events.epoch_complete
    QVariantMap payload;
    payload["epoch"] = 1;
    payload["mAP50"] = 0.77;
    QVERIFY(AuditLog::record("training", runId, "epoch_complete", payload));

    MetricService metricService;
    QVariantList history = metricService.getMetricHistory(runId);
    QCOMPARE(history.size(), 1);
    QCOMPARE(history[0].toMap()["mAP50"].toDouble(), 0.77);
}

QTEST_MAIN(TestAuditLog)
#include "test_audit_log.moc"
