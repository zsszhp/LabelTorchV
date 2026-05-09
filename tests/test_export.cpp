#include <QTest>
#include <QCoreApplication>
#include <QSqlQuery>
#include <QFile>
#include "Database.h"
#include "ExportService.h"
#include "ModelRegistry.h"

class TestExport : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testExportModel();
    void testExportModelInvalidFormat();
    void testGetExportStatus();
    void testListExports();
    void testVerifyExport();
    void testVerifyNonSucceededFails();
    void testUpdateExportStatus();
    void testExportInvalidModelVersion();

private:
    QString m_projectId;
    QString m_datasetId;
    QString m_snapshotId;
    QString m_runId;
    QString m_modelVersionId;
};

void TestExport::initTestCase()
{
    QString dbPath = QCoreApplication::applicationDirPath() + "/test_export_db";
    QFile::remove(dbPath);
    QFile::remove(dbPath + "-journal");
    QFile::remove(dbPath + "-wal");

    Database::instance().open(dbPath);
    Database::instance().initializeSchema();

    auto db = Database::instance().database();

    // Create project
    m_projectId = "proj-export-test";
    QSqlQuery q(db);
    q.prepare("INSERT INTO projects (id, name, root_path) VALUES (?, ?, ?)");
    q.addBindValue(m_projectId);
    q.addBindValue("ExportTestProject");
    q.addBindValue("/tmp/exporttest");
    QVERIFY(q.exec());

    // Create taxonomy
    q.prepare("INSERT INTO taxonomies (id, project_id, name, version, class_definitions_json) VALUES (?, ?, ?, ?, ?)");
    q.addBindValue("tax-export-test");
    q.addBindValue(m_projectId);
    q.addBindValue("Default");
    q.addBindValue(1);
    q.addBindValue("[\"defect\"]");
    QVERIFY(q.exec());

    // Create dataset
    m_datasetId = "ds-export-test";
    q.prepare("INSERT INTO datasets (id, project_id, name, image_root, label_root, format, sample_count, import_status) "
              "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(m_datasetId);
    q.addBindValue(m_projectId);
    q.addBindValue("ExportDataset");
    q.addBindValue("/tmp/img");
    q.addBindValue("/tmp/lbl");
    q.addBindValue("yolo_txt");
    q.addBindValue(5);
    q.addBindValue("completed");
    QVERIFY(q.exec());

    // Create snapshot
    m_snapshotId = "snap-export-test";
    q.prepare("INSERT INTO dataset_snapshots (id, dataset_id, sample_manifest_json, split_manifest_json, taxonomy_version, annotation_revision_boundary) "
              "VALUES (?, ?, ?, ?, ?, ?)");
    q.addBindValue(m_snapshotId);
    q.addBindValue(m_datasetId);
    q.addBindValue("[\"sample-0\"]");
    q.addBindValue("{\"train\":[\"sample-0\"],\"val\":[]}");
    q.addBindValue("tax-export-test:v1");
    q.addBindValue("none");
    QVERIFY(q.exec());

    // Create a succeeded training run
    m_runId = "run-export-test-001";
    q.prepare("INSERT INTO training_runs (id, project_id, snapshot_id, config_snapshot_json, status) "
              "VALUES (?, ?, ?, ?, 'succeeded')");
    q.addBindValue(m_runId);
    q.addBindValue(m_projectId);
    q.addBindValue(m_snapshotId);
    q.addBindValue(R"({"model_family":"yolov8","epochs":50})");
    QVERIFY(q.exec());

    // Register a model version
    ModelRegistry registry;
    QString metricsJson = R"({"mAP50":0.852,"mAP50-95":0.621,"precision":0.89,"recall":0.83,"fitness":0.86})";
    m_modelVersionId = registry.registerModelVersion(
        m_runId, "/weights/best.pt", "/weights/last.pt", metricsJson);
    QVERIFY(!m_modelVersionId.isEmpty());
}

void TestExport::testExportModel()
{
    ExportService service;
    QString optionsJson = R"({"opset_version":13,"dynamic_batch":true,"simplify":true})";

    QString artifactId = service.exportModel(m_modelVersionId, "onnx", optionsJson);
    QVERIFY(!artifactId.isEmpty());

    // Verify the artifact was created with running status
    QVariantMap status = service.getExportStatus(artifactId);
    QVERIFY(!status.isEmpty());
    QCOMPARE(status["modelVersionId"].toString(), m_modelVersionId);
    QCOMPARE(status["format"].toString(), QString("onnx"));
    // Status should be "running" because exportModel transitions pending -> running
    QCOMPARE(status["status"].toString(), QString("running"));
}

void TestExport::testExportModelInvalidFormat()
{
    ExportService service;
    QString artifactId = service.exportModel(m_modelVersionId, "invalid_format", "{}");
    QVERIFY(artifactId.isEmpty());
}

void TestExport::testGetExportStatus()
{
    ExportService service;
    QString artifactId = service.exportModel(m_modelVersionId, "pt", "{}");
    QVERIFY(!artifactId.isEmpty());

    QVariantMap status = service.getExportStatus(artifactId);
    QVERIFY(!status.isEmpty());
    QCOMPARE(status["id"].toString(), artifactId);
    QCOMPARE(status["format"].toString(), QString("pt"));
    QVERIFY(status.contains("status"));
    QVERIFY(status.contains("outputPath"));
    QVERIFY(status.contains("createdAt"));

    // Non-existent artifact returns empty
    QVariantMap notFound = service.getExportStatus("nonexistent-id");
    QVERIFY(notFound.isEmpty());
}

void TestExport::testListExports()
{
    ExportService service;

    // Create multiple exports
    QString id1 = service.exportModel(m_modelVersionId, "onnx", R"({"opset_version":13})");
    QVERIFY(!id1.isEmpty());
    QString id2 = service.exportModel(m_modelVersionId, "pt", "{}");
    QVERIFY(!id2.isEmpty());

    QVariantList exports = service.listExports(m_modelVersionId);
    QVERIFY(exports.size() >= 2);

    // Verify the list contains the expected formats
    bool foundOnnx = false;
    bool foundPt = false;
    for (const QVariant &item : exports) {
        QVariantMap artifact = item.toMap();
        if (artifact["format"].toString() == "onnx") foundOnnx = true;
        if (artifact["format"].toString() == "pt") foundPt = true;
    }
    QVERIFY(foundOnnx);
    QVERIFY(foundPt);

    // Each entry has a status
    for (const QVariant &item : exports) {
        QVariantMap artifact = item.toMap();
        QVERIFY(artifact.contains("status"));
        QVERIFY(artifact.contains("outputPath"));
    }
}

void TestExport::testVerifyExport()
{
    ExportService service;
    QString artifactId = service.exportModel(m_modelVersionId, "onnx", "{}");
    QVERIFY(!artifactId.isEmpty());

    // Cannot verify while running
    QVERIFY(!service.verifyExport(artifactId));

    // Transition to succeeded
    QVERIFY(service.updateExportStatus(artifactId, "succeeded"));

    // Now verify should succeed
    QVERIFY(service.verifyExport(artifactId));

    // Status should be verifying
    QVariantMap status = service.getExportStatus(artifactId);
    QCOMPARE(status["status"].toString(), QString("verifying"));
}

void TestExport::testVerifyNonSucceededFails()
{
    ExportService service;
    QString artifactId = service.exportModel(m_modelVersionId, "pt", "{}");
    QVERIFY(!artifactId.isEmpty());

    // Status is "running" - verify should fail
    QVERIFY(!service.verifyExport(artifactId));

    // Set to pending - verify should fail
    QVERIFY(service.updateExportStatus(artifactId, "pending"));
    QVERIFY(!service.verifyExport(artifactId));

    // Set to failed - verify should fail
    QVERIFY(service.updateExportStatus(artifactId, "failed"));
    QVERIFY(!service.verifyExport(artifactId));

    // Set to verifying - verify should fail (already verifying)
    QVERIFY(service.updateExportStatus(artifactId, "verifying"));
    QVERIFY(!service.verifyExport(artifactId));
}

void TestExport::testUpdateExportStatus()
{
    ExportService service;
    QString artifactId = service.exportModel(m_modelVersionId, "engine", "{}");
    QVERIFY(!artifactId.isEmpty());

    // Update to succeeded
    QVERIFY(service.updateExportStatus(artifactId, "succeeded"));

    QVariantMap status = service.getExportStatus(artifactId);
    QCOMPARE(status["status"].toString(), QString("succeeded"));

    // Update to failed
    QVERIFY(service.updateExportStatus(artifactId, "failed"));

    status = service.getExportStatus(artifactId);
    QCOMPARE(status["status"].toString(), QString("failed"));

    // Update non-existent should fail
    QVERIFY(!service.updateExportStatus("nonexistent-id", "succeeded"));
}

void TestExport::testExportInvalidModelVersion()
{
    ExportService service;
    QString artifactId = service.exportModel("nonexistent-version-id", "onnx", "{}");
    QVERIFY(artifactId.isEmpty());
}

QTEST_MAIN(TestExport)
#include "test_export.moc"
