#include <QTest>
#include <QCoreApplication>
#include <QSqlQuery>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "Database.h"
#include "InferenceService.h"
#include "AssistedLabelService.h"
#include "ModelRegistry.h"

class TestInference : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testRunInference();
    void testGetBatchStatus();
    void testListBatches();
    void testCancelBatch();
    void testConfirmCandidate();
    void testRejectCandidate();
    void testConfirmAllAboveThreshold();
    void testRejectAllBelowThreshold();
    void testGetBatchStats();

private:
    QString m_projectId;
    QString m_datasetId;
    QString m_snapshotId;
    QString m_runId;
    QString m_modelVersionId;

    /**
     * @brief Inject test candidates into a batch's candidate_snapshot_json.
     */
    void injectCandidates(const QString &batchId, const QJsonArray &candidates, const QString &status = "completed");
};

void TestInference::initTestCase()
{
    QString dbPath = QCoreApplication::applicationDirPath() + "/test_inference_db";
    QFile::remove(dbPath);
    QFile::remove(dbPath + "-journal");
    QFile::remove(dbPath + "-wal");

    Database::instance().open(dbPath);
    Database::instance().initializeSchema();

    auto db = Database::instance().database();

    // Create project
    m_projectId = "proj-inference-test";
    QSqlQuery q(db);
    q.prepare("INSERT INTO projects (id, name, root_path) VALUES (?, ?, ?)");
    q.addBindValue(m_projectId);
    q.addBindValue("InferenceTestProject");
    q.addBindValue("/tmp/infertest");
    QVERIFY(q.exec());

    // Create taxonomy
    q.prepare("INSERT INTO taxonomies (id, project_id, name, version, class_definitions_json) VALUES (?, ?, ?, ?, ?)");
    q.addBindValue("tax-inference-test");
    q.addBindValue(m_projectId);
    q.addBindValue("Default");
    q.addBindValue(1);
    q.addBindValue("[\"scratch\",\"dent\",\"stain\"]");
    QVERIFY(q.exec());

    // Create dataset
    m_datasetId = "ds-inference-test";
    q.prepare("INSERT INTO datasets (id, project_id, name, image_root, label_root, format, sample_count, import_status) "
              "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(m_datasetId);
    q.addBindValue(m_projectId);
    q.addBindValue("InferenceDataset");
    q.addBindValue("/tmp/img");
    q.addBindValue("/tmp/lbl");
    q.addBindValue("yolo_txt");
    q.addBindValue(10);
    q.addBindValue("completed");
    QVERIFY(q.exec());

    // Create snapshot
    m_snapshotId = "snap-inference-test";
    q.prepare("INSERT INTO dataset_snapshots (id, dataset_id, sample_manifest_json, split_manifest_json, taxonomy_version, annotation_revision_boundary) "
              "VALUES (?, ?, ?, ?, ?, ?)");
    q.addBindValue(m_snapshotId);
    q.addBindValue(m_datasetId);
    q.addBindValue("[\"sample-0\",\"sample-1\"]");
    q.addBindValue("{\"train\":[\"sample-0\"],\"val\":[\"sample-1\"]}");
    q.addBindValue("tax-inference-test:v1");
    q.addBindValue("none");
    QVERIFY(q.exec());

    // Create a succeeded training run
    m_runId = "run-inference-test-001";
    q.prepare("INSERT INTO training_runs (id, project_id, snapshot_id, config_snapshot_json, status) "
              "VALUES (?, ?, ?, ?, 'succeeded')");
    q.addBindValue(m_runId);
    q.addBindValue(m_projectId);
    q.addBindValue(m_snapshotId);
    q.addBindValue(R"({"model_family":"yolov8","epochs":50})");
    QVERIFY(q.exec());

    // Register a model version
    ModelRegistry registry;
    m_modelVersionId = registry.registerModelVersion(
        m_runId, "/weights/best.pt", "/weights/last.pt",
        R"({"mAP50":0.92,"mAP50-95":0.71,"precision":0.94,"recall":0.89})");
    QVERIFY(!m_modelVersionId.isEmpty());
}

void TestInference::injectCandidates(const QString &batchId, const QJsonArray &candidates, const QString &status)
{
    auto db = Database::instance().database();
    QJsonObject snapshotObj;
    snapshotObj["status"] = status;
    snapshotObj["candidates"] = candidates;
    QString jsonStr = QString::fromUtf8(QJsonDocument(snapshotObj).toJson(QJsonDocument::Compact));

    QSqlQuery q(db);
    q.prepare("UPDATE assisted_label_batches SET candidate_snapshot_json = ? WHERE id = ?");
    q.addBindValue(jsonStr);
    q.addBindValue(batchId);
    QVERIFY(q.exec());
}

void TestInference::testRunInference()
{
    InferenceService service;
    QString batchId = service.runInference(
        m_modelVersionId, m_datasetId, "all", 0.25, 0.45);
    QVERIFY(!batchId.isEmpty());

    // Verify the batch exists in database
    auto db = Database::instance().database();
    QSqlQuery q(db);
    q.prepare("SELECT model_version_id, dataset_id, target_sample_scope, conf_threshold, iou_threshold "
              "FROM assisted_label_batches WHERE id = ?");
    q.addBindValue(batchId);
    QVERIFY(q.exec());
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toString(), m_modelVersionId);
    QCOMPARE(q.value(1).toString(), m_datasetId);
    QCOMPARE(q.value(2).toString(), QString("all"));
    QCOMPARE(q.value(3).toDouble(), 0.25);
    QCOMPARE(q.value(4).toDouble(), 0.45);
}

void TestInference::testGetBatchStatus()
{
    InferenceService service;
    QString batchId = service.runInference(
        m_modelVersionId, m_datasetId, "unlabeled", 0.3, 0.5);
    QVERIFY(!batchId.isEmpty());

    QVariantMap status = service.getBatchStatus(batchId);
    QVERIFY(!status.isEmpty());
    QCOMPARE(status["id"].toString(), batchId);
    QCOMPARE(status["modelVersionId"].toString(), m_modelVersionId);
    QCOMPARE(status["datasetId"].toString(), m_datasetId);
    QCOMPARE(status["targetSampleScope"].toString(), QString("unlabeled"));
    QCOMPARE(status["confThreshold"].toDouble(), 0.3);
    QCOMPARE(status["iouThreshold"].toDouble(), 0.5);
    QCOMPARE(status["status"].toString(), QString("pending"));

    // Non-existent batch returns empty
    QVariantMap notFound = service.getBatchStatus("nonexistent-id");
    QVERIFY(notFound.isEmpty());
}

void TestInference::testListBatches()
{
    InferenceService service;

    // Create multiple batches
    QString batchId1 = service.runInference(
        m_modelVersionId, m_datasetId, "all", 0.25, 0.45);
    QString batchId2 = service.runInference(
        m_modelVersionId, m_datasetId, "failed", 0.5, 0.7);
    QVERIFY(!batchId1.isEmpty());
    QVERIFY(!batchId2.isEmpty());

    QVariantList batches = service.listBatches(m_datasetId);
    QVERIFY(batches.size() >= 2);

    // Each batch should have required fields
    for (const auto &batch : batches) {
        QVariantMap b = batch.toMap();
        QVERIFY(b.contains("id"));
        QVERIFY(b.contains("modelVersionId"));
        QVERIFY(b.contains("datasetId"));
        QVERIFY(b.contains("status"));
        QVERIFY(b.contains("confThreshold"));
    }
}

void TestInference::testCancelBatch()
{
    InferenceService service;
    QString batchId = service.runInference(
        m_modelVersionId, m_datasetId, "all", 0.25, 0.45);
    QVERIFY(!batchId.isEmpty());

    // Cancel the batch
    QVERIFY(service.cancelBatch(batchId));

    // Verify status changed
    QVariantMap status = service.getBatchStatus(batchId);
    QCOMPARE(status["status"].toString(), QString("cancelled"));

    // Cannot cancel again
    QVERIFY(!service.cancelBatch(batchId));

    // Non-existent batch returns false
    QVERIFY(!service.cancelBatch("nonexistent-id"));
}

void TestInference::testConfirmCandidate()
{
    InferenceService infService;
    AssistedLabelService labelService;

    // Create a batch and inject candidates
    QString batchId = infService.runInference(
        m_modelVersionId, m_datasetId, "all", 0.25, 0.45);
    QVERIFY(!batchId.isEmpty());

    QJsonArray candidates;
    {
        QJsonObject c1;
        c1["className"] = "scratch"; c1["classIndex"] = 0;
        c1["cx"] = 0.5; c1["cy"] = 0.5; c1["w"] = 0.2; c1["h"] = 0.1;
        c1["confidence"] = 0.92; c1["state"] = "pending";
        candidates.append(c1);

        QJsonObject c2;
        c2["className"] = "dent"; c2["classIndex"] = 1;
        c2["cx"] = 0.3; c2["cy"] = 0.7; c2["w"] = 0.15; c2["h"] = 0.08;
        c2["confidence"] = 0.78; c2["state"] = "pending";
        candidates.append(c2);

        QJsonObject c3;
        c3["className"] = "stain"; c3["classIndex"] = 2;
        c3["cx"] = 0.8; c3["cy"] = 0.2; c3["w"] = 0.1; c3["h"] = 0.1;
        c3["confidence"] = 0.45; c3["state"] = "pending";
        candidates.append(c3);
    }
    injectCandidates(batchId, candidates);

    // Confirm candidate at index 1
    QVERIFY(labelService.confirmCandidate(batchId, 1));

    // Verify the candidate state
    QVariantList result = labelService.getCandidates(batchId);
    QCOMPARE(result.size(), 3);
    QCOMPARE(result[0].toMap()["state"].toString(), QString("pending"));
    QCOMPARE(result[1].toMap()["state"].toString(), QString("confirmed"));
    QCOMPARE(result[2].toMap()["state"].toString(), QString("pending"));

    // Invalid index
    QVERIFY(!labelService.confirmCandidate(batchId, -1));
    QVERIFY(!labelService.confirmCandidate(batchId, 99));
}

void TestInference::testRejectCandidate()
{
    InferenceService infService;
    AssistedLabelService labelService;

    QString batchId = infService.runInference(
        m_modelVersionId, m_datasetId, "all", 0.25, 0.45);
    QVERIFY(!batchId.isEmpty());

    QJsonArray candidates;
    {
        QJsonObject c1;
        c1["className"] = "scratch"; c1["classIndex"] = 0;
        c1["cx"] = 0.5; c1["cy"] = 0.5; c1["w"] = 0.2; c1["h"] = 0.1;
        c1["confidence"] = 0.92; c1["state"] = "pending";
        candidates.append(c1);

        QJsonObject c2;
        c2["className"] = "dent"; c2["classIndex"] = 1;
        c2["cx"] = 0.3; c2["cy"] = 0.7; c2["w"] = 0.15; c2["h"] = 0.08;
        c2["confidence"] = 0.45; c2["state"] = "pending";
        candidates.append(c2);
    }
    injectCandidates(batchId, candidates);

    // Reject candidate at index 1
    QVERIFY(labelService.rejectCandidate(batchId, 1));

    QVariantList result = labelService.getCandidates(batchId);
    QCOMPARE(result.size(), 2);
    QCOMPARE(result[0].toMap()["state"].toString(), QString("pending"));
    QCOMPARE(result[1].toMap()["state"].toString(), QString("rejected"));
}

void TestInference::testConfirmAllAboveThreshold()
{
    InferenceService infService;
    AssistedLabelService labelService;

    QString batchId = infService.runInference(
        m_modelVersionId, m_datasetId, "all", 0.25, 0.45);
    QVERIFY(!batchId.isEmpty());

    QJsonArray candidates;
    {
        QJsonObject c1;
        c1["className"] = "scratch"; c1["classIndex"] = 0;
        c1["cx"] = 0.5; c1["cy"] = 0.5; c1["w"] = 0.2; c1["h"] = 0.1;
        c1["confidence"] = 0.95; c1["state"] = "pending";
        candidates.append(c1);

        QJsonObject c2;
        c2["className"] = "dent"; c2["classIndex"] = 1;
        c2["cx"] = 0.3; c2["cy"] = 0.7; c2["w"] = 0.15; c2["h"] = 0.08;
        c2["confidence"] = 0.80; c2["state"] = "pending";
        candidates.append(c2);

        QJsonObject c3;
        c3["className"] = "stain"; c3["classIndex"] = 2;
        c3["cx"] = 0.8; c3["cy"] = 0.2; c3["w"] = 0.1; c3["h"] = 0.1;
        c3["confidence"] = 0.40; c3["state"] = "pending";
        candidates.append(c3);

        QJsonObject c4;
        c4["className"] = "scratch"; c4["classIndex"] = 0;
        c4["cx"] = 0.6; c4["cy"] = 0.4; c4["w"] = 0.12; c4["h"] = 0.06;
        c4["confidence"] = 0.55; c4["state"] = "pending";
        candidates.append(c4);
    }
    injectCandidates(batchId, candidates);

    // Confirm all above 0.6 threshold - should confirm 2 (0.95 and 0.80)
    int confirmed = labelService.confirmAllAboveThreshold(batchId, 0.6);
    QCOMPARE(confirmed, 2);

    QVariantList result = labelService.getCandidates(batchId);
    QCOMPARE(result[0].toMap()["state"].toString(), QString("confirmed"));
    QCOMPARE(result[1].toMap()["state"].toString(), QString("confirmed"));
    QCOMPARE(result[2].toMap()["state"].toString(), QString("pending"));
    QCOMPARE(result[3].toMap()["state"].toString(), QString("pending"));
}

void TestInference::testRejectAllBelowThreshold()
{
    InferenceService infService;
    AssistedLabelService labelService;

    QString batchId = infService.runInference(
        m_modelVersionId, m_datasetId, "all", 0.25, 0.45);
    QVERIFY(!batchId.isEmpty());

    QJsonArray candidates;
    {
        QJsonObject c1;
        c1["className"] = "scratch"; c1["classIndex"] = 0;
        c1["cx"] = 0.5; c1["cy"] = 0.5; c1["w"] = 0.2; c1["h"] = 0.1;
        c1["confidence"] = 0.95; c1["state"] = "pending";
        candidates.append(c1);

        QJsonObject c2;
        c2["className"] = "dent"; c2["classIndex"] = 1;
        c2["cx"] = 0.3; c2["cy"] = 0.7; c2["w"] = 0.15; c2["h"] = 0.08;
        c2["confidence"] = 0.30; c2["state"] = "pending";
        candidates.append(c2);

        QJsonObject c3;
        c3["className"] = "stain"; c3["classIndex"] = 2;
        c3["cx"] = 0.8; c3["cy"] = 0.2; c3["w"] = 0.1; c3["h"] = 0.1;
        c3["confidence"] = 0.20; c3["state"] = "pending";
        candidates.append(c3);

        QJsonObject c4;
        c4["className"] = "scratch"; c4["classIndex"] = 0;
        c4["cx"] = 0.6; c4["cy"] = 0.4; c4["w"] = 0.12; c4["h"] = 0.06;
        c4["confidence"] = 0.75; c4["state"] = "pending";
        candidates.append(c4);
    }
    injectCandidates(batchId, candidates);

    // Reject all below 0.5 - should reject 2 (0.30 and 0.20)
    int rejected = labelService.rejectAllBelowThreshold(batchId, 0.5);
    QCOMPARE(rejected, 2);

    QVariantList result = labelService.getCandidates(batchId);
    QCOMPARE(result[0].toMap()["state"].toString(), QString("pending"));
    QCOMPARE(result[1].toMap()["state"].toString(), QString("rejected"));
    QCOMPARE(result[2].toMap()["state"].toString(), QString("rejected"));
    QCOMPARE(result[3].toMap()["state"].toString(), QString("pending"));
}

void TestInference::testGetBatchStats()
{
    InferenceService infService;
    AssistedLabelService labelService;

    QString batchId = infService.runInference(
        m_modelVersionId, m_datasetId, "all", 0.25, 0.45);
    QVERIFY(!batchId.isEmpty());

    QJsonArray candidates;
    {
        QJsonObject c1;
        c1["className"] = "scratch"; c1["classIndex"] = 0;
        c1["cx"] = 0.5; c1["cy"] = 0.5; c1["w"] = 0.2; c1["h"] = 0.1;
        c1["confidence"] = 0.95; c1["state"] = "confirmed";
        candidates.append(c1);

        QJsonObject c2;
        c2["className"] = "dent"; c2["classIndex"] = 1;
        c2["cx"] = 0.3; c2["cy"] = 0.7; c2["w"] = 0.15; c2["h"] = 0.08;
        c2["confidence"] = 0.30; c2["state"] = "rejected";
        candidates.append(c2);

        QJsonObject c3;
        c3["className"] = "stain"; c3["classIndex"] = 2;
        c3["cx"] = 0.8; c3["cy"] = 0.2; c3["w"] = 0.1; c3["h"] = 0.1;
        c3["confidence"] = 0.60; c3["state"] = "pending";
        candidates.append(c3);

        QJsonObject c4;
        c4["className"] = "scratch"; c4["classIndex"] = 0;
        c4["cx"] = 0.6; c4["cy"] = 0.4; c4["w"] = 0.12; c4["h"] = 0.06;
        c4["confidence"] = 0.70; c4["state"] = "edited";
        candidates.append(c4);

        QJsonObject c5;
        c5["className"] = "dent"; c5["classIndex"] = 1;
        c5["cx"] = 0.4; c5["cy"] = 0.3; c5["w"] = 0.1; c5["h"] = 0.05;
        c5["confidence"] = 0.50; c5["state"] = "pending";
        candidates.append(c5);
    }
    injectCandidates(batchId, candidates);

    QVariantMap stats = labelService.getBatchStats(batchId);
    QCOMPARE(stats["total"].toInt(), 5);
    QCOMPARE(stats["confirmed"].toInt(), 1);
    QCOMPARE(stats["rejected"].toInt(), 1);
    QCOMPARE(stats["pending"].toInt(), 2);
    QCOMPARE(stats["edited"].toInt(), 1);

    // Non-existent batch returns zero stats
    QVariantMap emptyStats = labelService.getBatchStats("nonexistent-id");
    QCOMPARE(emptyStats["total"].toInt(), 0);
}

QTEST_MAIN(TestInference)
#include "test_inference.moc"
