// P1-C 主工作流闭环测试：
// P1-14 辅助标注回写/增量训练入口、主动学习队列落库
// P1-16/17/18 评估结果透传（PR/阈值/Go-NoGo/逐类）
// P1-19 训练失败诊断
// P1-20 导出交付目录
#include <QTest>
#include <QCoreApplication>
#include <QSqlQuery>
#include <QSqlError>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryDir>

#include "Database.h"
#include "AssistedLabelService.h"
#include "ActiveLearningService.h"
#include "TrainingService.h"
#include "TestingService.h"
#include "ExportService.h"
#include "labelio/YoloTxtReader.h"
#include "geometry/AxisAlignedBox.h"

class TestP1Workflow : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // P1-14
    void testCommitConfirmedLabelsWritesYoloTxtAndRevision();
    void testCommitConfirmedLabelsSkipsInvalid();
    void testRetrainFromBatchRequiresServices();
    void testActiveLearningQueuePersists();

    // P1-19
    void testTrainingFailureInfoStoredAndReadable();

    // P1-16/17/18
    void testTestingPassthroughCurvesAndDecisions();

    // P1-20
    void testExportDeliveryPackage();

private:
    QString m_projectId;
    QString m_datasetId;
    QString m_snapshotId;
    QString m_runId;
    QString m_modelVersionId;
    QString m_sampleId;
    QString m_labelPath;
    QString m_imageDir;
    QString m_labelDir;
    /// 临时目录须贯穿全部用例，否则 initTestCase 返回后文件被清理
    QTemporaryDir m_tmp;

    void injectCandidates(const QString &batchId, const QJsonArray &candidates);
    QString createBatch(const QString &batchId);
};

void TestP1Workflow::initTestCase()
{
    QString dbPath = QCoreApplication::applicationDirPath() + "/test_p1_workflow_db";
    QFile::remove(dbPath);
    QFile::remove(dbPath + "-journal");
    QFile::remove(dbPath + "-wal");

    Database::instance().open(dbPath);
    QVERIFY(Database::instance().initializeSchema());

    // 校验 active_learning_items 表已随 Schema 创建（P1-14）
    {
        QSqlQuery q(Database::instance().database());
        QVERIFY(q.exec("SELECT id FROM active_learning_items LIMIT 0"));
    }

    QVERIFY(m_tmp.isValid());
    m_imageDir = m_tmp.path() + "/images";
    m_labelDir = m_tmp.path() + "/labels";
    QDir().mkpath(m_imageDir);
    QDir().mkpath(m_labelDir);

    auto db = Database::instance().database();

    m_projectId = "proj-p1-test";
    QSqlQuery q(db);
    q.prepare("INSERT INTO projects (id, name, root_path, task_type) VALUES (?, ?, ?, 'detect')");
    q.addBindValue(m_projectId);
    q.addBindValue("P1WorkflowProject");
    q.addBindValue(m_tmp.path());
    QVERIFY(q.exec());

    q.prepare("INSERT INTO taxonomies (id, project_id, name, version, class_definitions_json) VALUES (?, ?, ?, ?, ?)");
    q.addBindValue("tax-p1-test");
    q.addBindValue(m_projectId);
    q.addBindValue("Default");
    q.addBindValue(1);
    q.addBindValue("[\"scratch\",\"dent\"]");
    QVERIFY(q.exec());

    m_datasetId = "ds-p1-test";
    q.prepare("INSERT INTO datasets (id, project_id, name, image_root, label_root, format, sample_count, import_status) "
              "VALUES (?, ?, ?, ?, ?, 'yolo_txt', 1, 'completed')");
    q.addBindValue(m_datasetId);
    q.addBindValue(m_projectId);
    q.addBindValue("P1Dataset");
    q.addBindValue(m_imageDir);
    q.addBindValue(m_labelDir);
    QVERIFY(q.exec());

    // 样本 + 标签文件
    m_sampleId = "sample-p1-0";
    m_labelPath = m_labelDir + "/sample-p1-0.txt";
    {
        QFile img(m_imageDir + "/sample-p1-0.jpg");
        QVERIFY(img.open(QIODevice::WriteOnly));
        img.write("fake-image");
        img.close();

        QFile lbl(m_labelPath);
        QVERIFY(lbl.open(QIODevice::WriteOnly | QIODevice::Text));
        lbl.write("0 0.5 0.5 0.2 0.2\n");
        lbl.close();
    }
    q.prepare("INSERT INTO dataset_samples (id, dataset_id, image_path, label_path, width, height, split) "
              "VALUES (?, ?, ?, ?, 640, 480, 'train')");
    q.addBindValue(m_sampleId);
    q.addBindValue(m_datasetId);
    q.addBindValue(m_imageDir + "/sample-p1-0.jpg");
    q.addBindValue(m_labelPath);
    QVERIFY(q.exec());

    m_snapshotId = "snap-p1-test";
    q.prepare("INSERT INTO dataset_snapshots (id, dataset_id, sample_manifest_json, split_manifest_json, taxonomy_version) "
              "VALUES (?, ?, ?, ?, ?)");
    q.addBindValue(m_snapshotId);
    q.addBindValue(m_datasetId);
    q.addBindValue("[\"sample-p1-0\"]");
    q.addBindValue("{\"train\":[\"sample-p1-0\"],\"val\":[]}");
    q.addBindValue("tax-p1-test:v1");
    QVERIFY(q.exec());

    m_runId = "run-p1-test";
    q.prepare("INSERT INTO training_runs (id, project_id, snapshot_id, config_snapshot_json, status) "
              "VALUES (?, ?, ?, ?, 'draft')");
    q.addBindValue(m_runId);
    q.addBindValue(m_projectId);
    q.addBindValue(m_snapshotId);
    q.addBindValue(R"({"model_family":"yolov8","epochs":10,"batch":4})");
    QVERIFY(q.exec());

    m_modelVersionId = "mv-p1-test";
    q.prepare("INSERT INTO model_versions (id, run_id, best_weight_path, last_weight_path, metrics_snapshot_json, source, project_id) "
              "VALUES (?, ?, ?, ?, ?, 'trained', ?)");
    q.addBindValue(m_modelVersionId);
    q.addBindValue(m_runId);
    q.addBindValue(m_tmp.path() + "/best.pt");
    q.addBindValue(m_tmp.path() + "/last.pt");
    q.addBindValue(R"({"mAP50":0.90,"mAP50-95":0.72,"precision":0.9,"recall":0.85})");
    q.addBindValue(m_projectId);
    QVERIFY(q.exec());
}

QString TestP1Workflow::createBatch(const QString &batchId)
{
    auto db = Database::instance().database();
    QSqlQuery q(db);
    q.prepare("INSERT INTO assisted_label_batches "
              "(id, model_version_id, dataset_id, target_sample_scope, conf_threshold, iou_threshold, candidate_snapshot_json) "
              "VALUES (?, ?, ?, 'all', 0.25, 0.45, '{}')");
    q.addBindValue(batchId);
    q.addBindValue(m_modelVersionId);
    q.addBindValue(m_datasetId);
    if (!q.exec()) return {};
    return batchId;
}

void TestP1Workflow::injectCandidates(const QString &batchId, const QJsonArray &candidates)
{
    QJsonObject snapshotObj;
    snapshotObj["status"] = "completed";
    snapshotObj["candidates"] = candidates;
    QString jsonStr = QString::fromUtf8(QJsonDocument(snapshotObj).toJson(QJsonDocument::Compact));

    auto db = Database::instance().database();
    QSqlQuery q(db);
    q.prepare("UPDATE assisted_label_batches SET candidate_snapshot_json = ? WHERE id = ?");
    q.addBindValue(jsonStr);
    q.addBindValue(batchId);
    QVERIFY(q.exec());
}

void TestP1Workflow::testCommitConfirmedLabelsWritesYoloTxtAndRevision()
{
    AssistedLabelService service;
    QString batchId = createBatch("batch-p1-commit");
    QVERIFY(!batchId.isEmpty());

    // 两个确认框 + 一个 pending（不应写入）
    QJsonArray candidates;
    QJsonObject c1;
    c1["className"] = "scratch";
    c1["classIndex"] = 0;
    c1["cx"] = 0.2; c1["cy"] = 0.2; c1["w"] = 0.1; c1["h"] = 0.1;
    c1["confidence"] = 0.9;
    c1["state"] = "confirmed";
    c1["sampleId"] = m_sampleId;
    candidates.append(c1);

    QJsonObject c2;
    c2["className"] = "dent";
    c2["classIndex"] = 1;
    c2["cx"] = 0.8; c2["cy"] = 0.8; c2["w"] = 0.15; c2["h"] = 0.15;
    c2["confidence"] = 0.8;
    c2["state"] = "edited";
    c2["sampleId"] = m_sampleId;
    candidates.append(c2);

    QJsonObject c3;
    c3["className"] = "dent";
    c3["classIndex"] = 1;
    c3["cx"] = 0.3; c3["cy"] = 0.3; c3["w"] = 0.1; c3["h"] = 0.1;
    c3["confidence"] = 0.5;
    c3["state"] = "pending";
    c3["sampleId"] = m_sampleId;
    candidates.append(c3);

    injectCandidates(batchId, candidates);

    QVariantMap result = service.commitConfirmedLabels(batchId);
    QVERIFY2(result["success"].toBool(), qPrintable(result["error"].toMap()["message"].toString()));
    QCOMPARE(result["samplesWritten"].toInt(), 1);
    QCOMPARE(result["revisionsCreated"].toInt(), 1);
    QVERIFY(result["skipped"].toInt() >= 0);

    // 标签文件应为原子写入后的完整标注（原有 1 + 新增 2）
    QVector<AxisAlignedBox> boxes = YoloTxtReader::read(m_labelPath);
    QCOMPARE(boxes.size(), 3);

    bool hasScratch = false, hasDent = false;
    for (const AxisAlignedBox &b : boxes) {
        if (b.classIndex == 0) hasScratch = true;
        if (b.classIndex == 1) hasDent = true;
    }
    QVERIFY(hasScratch && hasDent);

    // annotation_revisions 应有记录，source_type=assisted_confirm
    auto db = Database::instance().database();
    QSqlQuery q(db);
    q.prepare("SELECT source_type, before_snapshot_json, after_snapshot_json "
              "FROM annotation_revisions WHERE sample_id = ? ORDER BY created_at DESC LIMIT 1");
    q.addBindValue(m_sampleId);
    QVERIFY(q.exec());
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toString(), QString("assisted_confirm"));
    QVERIFY(!q.value(1).toString().isEmpty());
    QVERIFY(!q.value(2).toString().isEmpty());
}

void TestP1Workflow::testCommitConfirmedLabelsSkipsInvalid()
{
    AssistedLabelService service;
    QString batchId = createBatch("batch-p1-skip");
    QVERIFY(!batchId.isEmpty());

    // 缺少 sampleId 的确认框 → 计入 skipped
    QJsonArray candidates;
    QJsonObject bad;
    bad["className"] = "scratch";
    bad["classIndex"] = 0;
    bad["cx"] = 0.5; bad["cy"] = 0.5; bad["w"] = 0.1; bad["h"] = 0.1;
    bad["confidence"] = 0.9;
    bad["state"] = "confirmed";
    // 无 sampleId
    candidates.append(bad);

    // 非法几何
    QJsonObject badGeom;
    badGeom["className"] = "scratch";
    badGeom["classIndex"] = 0;
    badGeom["cx"] = 0.5; badGeom["cy"] = 0.5; badGeom["w"] = 0.0; badGeom["h"] = 0.0;
    badGeom["confidence"] = 0.9;
    badGeom["state"] = "confirmed";
    badGeom["sampleId"] = m_sampleId;
    candidates.append(badGeom);

    injectCandidates(batchId, candidates);

    QVariantMap result = service.commitConfirmedLabels(batchId);
    // 全部跳过时 boxesBySample 为空，success=true（无事可做）
    QVERIFY(result["success"].toBool());
    QCOMPARE(result["samplesWritten"].toInt(), 0);
    QVERIFY(result["skipped"].toInt() >= 2);
}

void TestP1Workflow::testRetrainFromBatchRequiresServices()
{
    AssistedLabelService service;
    // 未注入 SnapshotService/TrainingService 时应结构化失败
    QString batchId = createBatch("batch-p1-retrain");
    QVERIFY(!batchId.isEmpty());

    QJsonArray candidates;
    QJsonObject c1;
    c1["className"] = "scratch";
    c1["classIndex"] = 0;
    c1["cx"] = 0.25; c1["cy"] = 0.25; c1["w"] = 0.1; c1["h"] = 0.1;
    c1["confidence"] = 0.95;
    c1["state"] = "confirmed";
    c1["sampleId"] = m_sampleId;
    candidates.append(c1);
    injectCandidates(batchId, candidates);

    QVariantMap result = service.retrainFromBatch(batchId, 0.8, "random", "{}");
    QVERIFY(!result["success"].toBool());
    QCOMPARE(result["status"].toString(), QString("snapshot_failed"));
    QVariantMap err = result["error"].toMap();
    QCOMPARE(err["code"].toString(), QString("E_SERVICE_NOT_READY"));
    // 回写这一步应已成功
    QVERIFY(result["commit"].toMap()["success"].toBool());
}

void TestP1Workflow::testActiveLearningQueuePersists()
{
    ActiveLearningService service;

    QJsonObject sample;
    sample["path"] = "/tmp/al/sample-a.jpg";
    sample["sampleId"] = m_sampleId;
    sample["datasetId"] = m_datasetId;
    sample["confidence"] = 0.22;
    sample["reason"] = "low_confidence";
    sample["priority"] = 2;
    sample["classIndex"] = 0;
    sample["className"] = "scratch";
    QJsonArray boxes;
    QJsonObject box;
    box["class_id"] = 0;
    box["confidence"] = 0.22;
    box["xyxy"] = QJsonArray{10, 20, 30, 40};
    boxes.append(box);
    sample["boxes"] = boxes;

    service.addSampleToQueue("low-confidence", sample);

    QJsonObject sample2 = sample;
    sample2["path"] = "/tmp/al/sample-b.jpg";
    sample2["reason"] = "false_negative";
    sample2["priority"] = 3;
    service.addSampleToQueue("false-negative", sample2);

    // 内存队列应有内容
    QCOMPARE(service.getQueueSamples("low-confidence").size(), 1);
    QCOMPARE(service.getQueueSamples("false-negative").size(), 1);

    // 数据库应有记录（重启不丢的根基）
    {
        auto db = Database::instance().database();
        QSqlQuery q(db);
        QVERIFY(q.exec("SELECT COUNT(*) FROM active_learning_items WHERE status = 'queued'"));
        QVERIFY(q.next());
        QVERIFY(q.value(0).toInt() >= 2);
    }

    // 模拟重启：新实例直接从库加载
    ActiveLearningService reloaded;
    QCOMPARE(reloaded.getQueueSamples("low-confidence").size(), 1);
    QCOMPARE(reloaded.getQueueSamples("false-negative").size(), 1);
    QVariantMap stats = reloaded.getAllQueueStats();
    QCOMPARE(stats["total"].toInt(), 2);

    // 移除后库中应标记 discarded
    reloaded.removeSampleFromQueue("low-confidence", "/tmp/al/sample-a.jpg");
    QCOMPARE(reloaded.getQueueSamples("low-confidence").size(), 0);
    {
        auto db = Database::instance().database();
        QSqlQuery q(db);
        q.prepare("SELECT status FROM active_learning_items WHERE sample_path = ?");
        q.addBindValue("/tmp/al/sample-a.jpg");
        QVERIFY(q.exec());
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString(), QString("discarded"));
    }

    // 清空队列
    reloaded.clearQueue("false-negative");
    QCOMPARE(reloaded.getQueueSamples("false-negative").size(), 0);
}

void TestP1Workflow::testTrainingFailureInfoStoredAndReadable()
{
    TrainingService service;

    // 模拟日志流
    QVariantMap logEvent;
    logEvent["event_type"] = "task.log";
    logEvent["task_id"] = m_runId;
    logEvent["payload"] = QVariantMap{{"message", "Epoch 1/10 - loss: 1.2345"}};
    service.handleTrainingEvent(logEvent);

    // 模拟失败事件（后端返回 log_tail + diagnosis）
    QVariantMap diagnosis;
    diagnosis["code"] = "OOM";
    diagnosis["message"] = "显存不足（OOM）导致训练失败";
    diagnosis["suggestions"] = QStringList{"建议将 batch 降至 2", "建议关闭其他 GPU 进程"};

    QVariantMap failPayload;
    failPayload["error"] = "CUDA out of memory. Tried to allocate 2.00 GiB";
    failPayload["log_tail"] = QVariantList{"Epoch 1/10 - loss: 1.2345", "CUDA out of memory"};
    failPayload["diagnosis"] = diagnosis;

    QVariantMap failEvent;
    failEvent["event_type"] = "task.failed";
    failEvent["task_id"] = m_runId;
    failEvent["payload"] = failPayload;
    service.handleTrainingEvent(failEvent);

    QVariantMap info = service.getFailureInfo(m_runId);
    QVERIFY(!info.isEmpty());
    QVERIFY(info["error"].toString().contains("out of memory"));
    QCOMPARE(info["diagnosisCode"].toString(), QString("OOM"));
    QVERIFY(info["diagnosisMessage"].toString().contains("显存"));
    QVERIFY(info["suggestions"].toList().size() >= 2);
    QCOMPARE(info["logTail"].toList().size(), 2);

    // getRun 应一并带出 failureInfoJson
    QVariantMap run = service.getRun(m_runId);
    QVERIFY(!run["failureInfoJson"].toString().isEmpty());
    QCOMPARE(run["status"].toString(), QString("failed"));
}

void TestP1Workflow::testTestingPassthroughCurvesAndDecisions()
{
    auto db = Database::instance().database();

    // 构造带完整 metrics_json 的测试任务
    QString taskId = "test-p1-passthrough";
    QJsonObject metrics;
    metrics["mAP50"] = 0.92;
    metrics["mAP50-95"] = 0.74;
    metrics["precision"] = 0.91;
    metrics["recall"] = 0.88;
    metrics["f1"] = 0.89;

    QJsonArray f1Curve;
    for (int i = 0; i < 5; ++i) {
        QJsonObject pt;
        pt["confidence"] = i * 0.2;
        pt["f1"] = 0.5 + i * 0.1;
        f1Curve.append(pt);
    }
    metrics["f1_curve"] = f1Curve;

    QJsonObject rec;
    rec["recommended_conf"] = 0.4;
    rec["method"] = "max_f1";
    rec["f1_at_threshold"] = 0.88;
    rec["precision_at_threshold"] = 0.9;
    rec["recall_at_threshold"] = 0.86;
    rec["expected_miss_rate"] = 0.14;
    rec["expected_false_alarm_rate"] = 0.1;
    metrics["threshold_recommendation"] = rec;

    QJsonObject goNoGo;
    goNoGo["decision"] = "go";
    goNoGo["gate_metric"] = "mAP50";
    goNoGo["gate_threshold"] = 0.85;
    goNoGo["gate_value"] = 0.92;
    goNoGo["gate_passed"] = true;
    goNoGo["regression_vs_baseline"] = false;
    goNoGo["baseline_value"] = 0.90;
    goNoGo["current_value"] = 0.92;
    goNoGo["reasons"] = QJsonArray{"结论：GO（可部署）"};
    metrics["go_no_go"] = goNoGo;

    QJsonArray perClass;
    QJsonObject cls1;
    cls1["classIndex"] = 1;
    cls1["className"] = "dent";
    cls1["precision"] = 0.8;
    cls1["recall"] = 0.6;
    cls1["f1"] = 0.69;
    cls1["ap50"] = 0.65;
    cls1["ap"] = 0.6;
    cls1["support"] = 20;
    cls1["fn_count"] = 8;
    cls1["fp_count"] = 3;
    perClass.append(cls1);
    QJsonObject cls0;
    cls0["classIndex"] = 0;
    cls0["className"] = "scratch";
    cls0["precision"] = 0.9;
    cls0["recall"] = 0.7;
    cls0["f1"] = 0.79;
    cls0["ap50"] = 0.75;
    cls0["ap"] = 0.7;
    cls0["support"] = 10;
    cls0["fn_count"] = 3;
    cls0["fp_count"] = 1;
    perClass.append(cls0);
    metrics["per_class"] = perClass;

    QJsonArray prCurve;
    for (int i = 0; i < 5; ++i) {
        QJsonObject pt;
        pt["recall"] = i * 0.25;
        pt["precision"] = 1.0 - i * 0.15;
        pt["confidence"] = 0.5;
        pt["f1"] = 0.7;
        prCurve.append(pt);
    }

    QSqlQuery q(db);
    q.prepare("INSERT INTO testing_runs (id, project_id, model_version_id, snapshot_id, config_json, status, metrics_json, pr_curve_json) "
              "VALUES (?, ?, ?, ?, '{}', 'succeeded', ?, ?)");
    q.addBindValue(taskId);
    q.addBindValue(m_projectId);
    q.addBindValue(m_modelVersionId);
    q.addBindValue(m_snapshotId);
    q.addBindValue(QString::fromUtf8(QJsonDocument(metrics).toJson(QJsonDocument::Compact)));
    q.addBindValue(QString::fromUtf8(QJsonDocument(prCurve).toJson(QJsonDocument::Compact)));
    QVERIFY(q.exec());

    TestingService testing;

    // P1-16：PR 曲线透传，字段名稳定
    QVariantList pr = testing.getPRCurveData(taskId);
    QCOMPARE(pr.size(), 5);
    QVariantMap firstPt = pr.first().toMap();
    QVERIFY(firstPt.contains("recall"));
    QVERIFY(firstPt.contains("precision"));

    // P1-17：阈值推荐 + Go/No-Go
    QVariantMap threshold = testing.getThresholdRecommendation(taskId);
    QCOMPARE(threshold["recommended_conf"].toDouble(), 0.4);
    QCOMPARE(threshold["method"].toString(), QString("max_f1"));
    QCOMPARE(threshold["expected_miss_rate"].toDouble(), 0.14);

    QVariantMap go = testing.getGoNoGo(taskId);
    QCOMPARE(go["decision"].toString(), QString("go"));
    QCOMPARE(go["gate_metric"].toString(), QString("mAP50"));
    QVERIFY(go["gate_passed"].toBool());
    QCOMPARE(go["regression_vs_baseline"].toBool(), false);

    // P1-18：逐类指标按漏检降序
    QVariantList perClassOut = testing.getPerClassMetrics(taskId);
    QCOMPARE(perClassOut.size(), 2);
    QCOMPARE(perClassOut[0].toMap()["className"].toString(), QString("dent"));
    QCOMPARE(perClassOut[0].toMap()["fn_count"].toInt(), 8);

    // 接入 FP/FN 队列（active_learning_items）
    {
        QSqlQuery insert(db);
        insert.prepare("INSERT INTO active_learning_items "
                       "(id, queue_type, sample_path, reason, confidence, class_index, class_name, status) "
                       "VALUES ('al-fp-1', 'false-positive', '/tmp/fp1.jpg', 'false_positive', 0.6, 1, 'dent', 'queued')");
        QVERIFY(insert.exec());
        insert.prepare("INSERT INTO active_learning_items "
                       "(id, queue_type, sample_path, reason, confidence, class_index, class_name, status) "
                       "VALUES ('al-fn-1', 'false-negative', '/tmp/fn1.jpg', 'false_negative', 0.1, 1, 'dent', 'queued')");
        QVERIFY(insert.exec());
    }

    QVariantList errorQueue = testing.getPerClassErrorQueue(taskId);
    QVERIFY(!errorQueue.isEmpty());
    QVariantMap dentEntry = errorQueue.first().toMap();
    QCOMPARE(dentEntry["className"].toString(), QString("dent"));
    QCOMPARE(dentEntry["fnCount"].toInt(), 1);
    QCOMPARE(dentEntry["fpCount"].toInt(), 1);
    QCOMPARE(dentEntry["fnSamples"].toList().size(), 1);
    QCOMPARE(dentEntry["fpSamples"].toList().size(), 1);
}

void TestP1Workflow::testExportDeliveryPackage()
{
    auto db = Database::instance().database();

    // 使用独立模型版本，避免被其他用例的 testing_runs 干扰 thresholds/eval 摘要
    const QString deliveryMvId = "mv-p1-delivery";
    {
        QSqlQuery q(db);
        q.prepare("INSERT INTO model_versions (id, run_id, best_weight_path, metrics_snapshot_json, source, project_id) "
                  "VALUES (?, ?, ?, ?, 'trained', ?)");
        q.addBindValue(deliveryMvId);
        q.addBindValue(m_runId);
        q.addBindValue(m_tmp.path() + "/best.pt");
        q.addBindValue(R"({"mAP50":0.91})");
        q.addBindValue(m_projectId);
        QVERIFY(q.exec());
    }

    // 准备导出产物：先造一个真实文件作为导出输出
    QString exportsDir = m_tmp.path() + "/exports_delivery";
    QVERIFY(QDir().mkpath(exportsDir));
    QString modelPath = exportsDir + "/export_abcd1234.onnx";
    {
        QFile f(modelPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("fake-onnx-bytes");
        f.close();
    }

    // 准备一次评估记录，供 thresholds/eval_summary 提取
    QString taskId = "test-p1-delivery";
    QJsonObject metrics;
    metrics["mAP50"] = 0.91;
    QJsonObject rec;
    rec["recommended_conf"] = 0.35;
    rec["method"] = "max_f1";
    metrics["threshold_recommendation"] = rec;
    QJsonObject goNoGo;
    goNoGo["decision"] = "go";
    metrics["go_no_go"] = goNoGo;
    QJsonArray perClass;
    QJsonObject cls;
    cls["classIndex"] = 0;
    cls["className"] = "scratch";
    cls["fn_count"] = 1;
    perClass.append(cls);
    metrics["per_class"] = perClass;

    {
        QSqlQuery q(db);
        q.prepare("INSERT INTO testing_runs (id, project_id, model_version_id, snapshot_id, config_json, status, metrics_json) "
                  "VALUES (?, ?, ?, ?, '{}', 'succeeded', ?)");
        q.addBindValue(taskId);
        q.addBindValue(m_projectId);
        q.addBindValue(deliveryMvId);
        q.addBindValue(m_snapshotId);
        q.addBindValue(QString::fromUtf8(QJsonDocument(metrics).toJson(QJsonDocument::Compact)));
        QVERIFY(q.exec());
    }

    QString artifactId = "artifact-p1-delivery";
    {
        QSqlQuery q(db);
        q.prepare("INSERT INTO export_artifacts (id, model_version_id, format, options_snapshot_json, output_path, validation_result, status) "
                  "VALUES (?, ?, 'onnx', '{}', ?, '', 'running')");
        q.addBindValue(artifactId);
        q.addBindValue(deliveryMvId);
        q.addBindValue(modelPath);
        QVERIFY(q.exec());
    }

    ExportService exportService;
    QString deliveryDir = exportService.generateDeliveryPackage(artifactId);
    QVERIFY2(!deliveryDir.isEmpty(), "delivery package dir should be created");
    QVERIFY(QDir(deliveryDir).exists());

    // 交付目录必备文件
    const QStringList expectedFiles = {
        "model.onnx", "classes.yaml", "thresholds.json",
        "eval_summary.json", "warmup.py", "infer_sample.py", "manifest.json"
    };
    for (const QString &name : expectedFiles) {
        QString path = deliveryDir + "/" + name;
        QVERIFY2(QFile::exists(path), qPrintable("missing delivery file: " + name));
    }

    // classes.yaml 应包含类别名
    {
        QFile f(deliveryDir + "/classes.yaml");
        QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
        QString content = QString::fromUtf8(f.readAll());
        QVERIFY(content.contains("scratch"));
        QVERIFY(content.contains("dent"));
    }

    // thresholds.json 应含推荐阈值
    {
        QFile f(deliveryDir + "/thresholds.json");
        QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
        QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
        QCOMPARE(obj["recommended_conf"].toDouble(), 0.35);
    }

    // eval_summary.json 应含评估摘要
    {
        QFile f(deliveryDir + "/eval_summary.json");
        QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
        QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
        QVERIFY(obj.contains("metrics"));
        QCOMPARE(obj["modelVersionId"].toString(), deliveryMvId);
    }

    // getDeliveryDir 应返回同一路径
    QCOMPARE(exportService.getDeliveryDir(artifactId), deliveryDir);
}

QTEST_MAIN(TestP1Workflow)
#include "test_p1_workflow.moc"
