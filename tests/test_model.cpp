#include <QTest>
#include <QCoreApplication>
#include <QSqlQuery>
#include <QFile>
#include "Database.h"
#include "ModelRegistry.h"
#include "MetricService.h"
#include "ModelVersionModel.h"

class TestModel : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testRegisterModelVersion();
    void testListModelVersions();
    void testGetModelVersion();
    void testDeleteModelVersion();
    void testDeleteVersionWithExportsFails();
    void testSetParentVersion();
    void testSetAndRemoveTag();
    void testMetricServiceGetMetrics();
    void testMetricServiceCompareVersions();
    void testModelVersionModel();

private:
    QString m_projectId;
    QString m_datasetId;
    QString m_snapshotId;
    QString m_runId;
};

void TestModel::initTestCase()
{
    QString dbPath = QCoreApplication::applicationDirPath() + "/test_model_db";
    QFile::remove(dbPath);
    QFile::remove(dbPath + "-journal");
    QFile::remove(dbPath + "-wal");

    Database::instance().open(dbPath);
    Database::instance().initializeSchema();

    auto db = Database::instance().database();

    // Create project
    m_projectId = "proj-model-test";
    QSqlQuery q(db);
    q.prepare("INSERT INTO projects (id, name, root_path) VALUES (?, ?, ?)");
    q.addBindValue(m_projectId);
    q.addBindValue("ModelTestProject");
    q.addBindValue("/tmp/modeltest");
    QVERIFY(q.exec());

    // Create taxonomy
    q.prepare("INSERT INTO taxonomies (id, project_id, name, version, class_definitions_json) VALUES (?, ?, ?, ?, ?)");
    q.addBindValue("tax-model-test");
    q.addBindValue(m_projectId);
    q.addBindValue("Default");
    q.addBindValue(1);
    q.addBindValue("[\"defect\"]");
    QVERIFY(q.exec());

    // Create dataset
    m_datasetId = "ds-model-test";
    q.prepare("INSERT INTO datasets (id, project_id, name, image_root, label_root, format, sample_count, import_status) "
              "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(m_datasetId);
    q.addBindValue(m_projectId);
    q.addBindValue("ModelDataset");
    q.addBindValue("/tmp/img");
    q.addBindValue("/tmp/lbl");
    q.addBindValue("yolo_txt");
    q.addBindValue(5);
    q.addBindValue("completed");
    QVERIFY(q.exec());

    // Create snapshot
    m_snapshotId = "snap-model-test";
    q.prepare("INSERT INTO dataset_snapshots (id, dataset_id, sample_manifest_json, split_manifest_json, taxonomy_version, annotation_revision_boundary) "
              "VALUES (?, ?, ?, ?, ?, ?)");
    q.addBindValue(m_snapshotId);
    q.addBindValue(m_datasetId);
    q.addBindValue("[\"sample-0\"]");
    q.addBindValue("{\"train\":[\"sample-0\"],\"val\":[]}");
    q.addBindValue("tax-model-test:v1");
    q.addBindValue("none");
    QVERIFY(q.exec());

    // Create a succeeded training run
    m_runId = "run-model-test-001";
    q.prepare("INSERT INTO training_runs (id, project_id, snapshot_id, config_snapshot_json, status) "
              "VALUES (?, ?, ?, ?, 'succeeded')");
    q.addBindValue(m_runId);
    q.addBindValue(m_projectId);
    q.addBindValue(m_snapshotId);
    q.addBindValue(R"({"model_family":"yolov8","epochs":50})");
    QVERIFY(q.exec());
}

void TestModel::testRegisterModelVersion()
{
    ModelRegistry registry;
    QString metricsJson = R"({"mAP50":0.852,"mAP50-95":0.621,"precision":0.89,"recall":0.83,"fitness":0.86})";
    QString versionId = registry.registerModelVersion(
        m_runId, "/weights/best.pt", "/weights/last.pt", metricsJson);

    QVERIFY(!versionId.isEmpty());
}

void TestModel::testListModelVersions()
{
    ModelRegistry registry;
    QVariantList versions = registry.listModelVersions(m_projectId);
    QVERIFY(versions.size() >= 1);

    QVariantMap first = versions[0].toMap();
    QCOMPARE(first["runId"].toString(), m_runId);
    QCOMPARE(first["bestWeightPath"].toString(), QString("/weights/best.pt"));
    QCOMPARE(first["lastWeightPath"].toString(), QString("/weights/last.pt"));
}

void TestModel::testGetModelVersion()
{
    ModelRegistry registry;
    QVariantList versions = registry.listModelVersions(m_projectId);
    QVERIFY(!versions.isEmpty());

    QString versionId = versions[0].toMap()["id"].toString();
    QVariantMap details = registry.getModelVersion(versionId);
    QVERIFY(!details.isEmpty());
    QCOMPARE(details["runId"].toString(), m_runId);
    QCOMPARE(details["bestWeightPath"].toString(), QString("/weights/best.pt"));
    QVERIFY(details["metricsJson"].toString().contains("mAP50"));
}

void TestModel::testDeleteModelVersion()
{
    ModelRegistry registry;
    // Register a new version to delete
    QString versionId = registry.registerModelVersion(
        m_runId, "/w/best.pt", "/w/last.pt", "{}");
    QVERIFY(!versionId.isEmpty());

    // Should be deletable (no exports reference it)
    QVERIFY(registry.deleteModelVersion(versionId));

    // Verify it's gone
    QVariantMap details = registry.getModelVersion(versionId);
    QVERIFY(details.isEmpty());
}

void TestModel::testDeleteVersionWithExportsFails()
{
    ModelRegistry registry;
    QString versionId = registry.registerModelVersion(
        m_runId, "/w2/best.pt", "/w2/last.pt", "{}");
    QVERIFY(!versionId.isEmpty());

    // Insert an export artifact referencing this version
    auto db = Database::instance().database();
    QSqlQuery q(db);
    q.prepare("INSERT INTO export_artifacts (id, model_version_id, format, output_path) VALUES (?, ?, ?, ?)");
    q.addBindValue("export-test-001");
    q.addBindValue(versionId);
    q.addBindValue("onnx");
    q.addBindValue("/exports/model.onnx");
    QVERIFY(q.exec());

    // Delete should fail
    QVERIFY(!registry.deleteModelVersion(versionId));

    // Version should still exist
    QVariantMap details = registry.getModelVersion(versionId);
    QVERIFY(!details.isEmpty());
}

void TestModel::testSetParentVersion()
{
    ModelRegistry registry;
    QString parentVersionId = registry.registerModelVersion(
        m_runId, "/wp/best.pt", "/wp/last.pt", "{}");
    QVERIFY(!parentVersionId.isEmpty());

    QString childVersionId = registry.registerModelVersion(
        m_runId, "/wc/best.pt", "/wc/last.pt", "{}");
    QVERIFY(!childVersionId.isEmpty());

    // Set parent
    QVERIFY(registry.setParentVersion(childVersionId, parentVersionId));

    // Verify parent is set
    QVariantMap childDetails = registry.getModelVersion(childVersionId);
    QCOMPARE(childDetails["parentVersionId"].toString(), parentVersionId);

    // Cannot set self as parent
    QVERIFY(!registry.setParentVersion(childVersionId, childVersionId));
}

void TestModel::testSetAndRemoveTag()
{
    ModelRegistry registry;
    QString versionId = registry.registerModelVersion(
        m_runId, "/wt/best.pt", "/wt/last.pt", R"({"mAP50":0.9})");
    QVERIFY(!versionId.isEmpty());

    // Add tags
    QVERIFY(registry.setTag(versionId, "baseline"));
    QVERIFY(registry.setTag(versionId, "best-so-far"));

    // Verify tags in metrics
    QVariantMap details = registry.getModelVersion(versionId);
    QString metricsJson = details["metricsJson"].toString();
    QVERIFY(metricsJson.contains("tags"));
    QVERIFY(metricsJson.contains("baseline"));
    QVERIFY(metricsJson.contains("best-so-far"));
    // Original metric should still be there
    QVERIFY(metricsJson.contains("mAP50"));

    // Remove a tag
    QVERIFY(registry.removeTag(versionId, "baseline"));

    // Verify removal
    details = registry.getModelVersion(versionId);
    metricsJson = details["metricsJson"].toString();
    QVERIFY(!metricsJson.contains("baseline"));
    QVERIFY(metricsJson.contains("best-so-far"));

    // Duplicate tag should not cause error
    QVERIFY(registry.setTag(versionId, "best-so-far"));

    // Remove non-existent tag is ok
    QVERIFY(registry.removeTag(versionId, "nonexistent"));
}

void TestModel::testMetricServiceGetMetrics()
{
    ModelRegistry registry;
    MetricService metricService;

    QString versionId = registry.registerModelVersion(
        m_runId, "/wm/best.pt", "/wm/last.pt",
        R"({"mAP50":0.872,"mAP50-95":0.543,"precision":0.91,"recall":0.85,"fitness":0.88})");
    QVERIFY(!versionId.isEmpty());

    QVariantMap metrics = metricService.getMetrics(versionId);
    QVERIFY(!metrics.isEmpty());
    QCOMPARE(metrics["mAP50"].toDouble(), 0.872);
    QCOMPARE(metrics["mAP50-95"].toDouble(), 0.543);
    QCOMPARE(metrics["precision"].toDouble(), 0.91);
    QCOMPARE(metrics["recall"].toDouble(), 0.85);
    QCOMPARE(metrics["fitness"].toDouble(), 0.88);
    QVERIFY(metrics.contains("raw"));
}

void TestModel::testMetricServiceCompareVersions()
{
    ModelRegistry registry;
    MetricService metricService;

    QString versionId1 = registry.registerModelVersion(
        m_runId, "/wc1/best.pt", "/wc1/last.pt",
        R"({"mAP50":0.80,"mAP50-95":0.50,"precision":0.85,"recall":0.78,"fitness":0.82})");
    QString versionId2 = registry.registerModelVersion(
        m_runId, "/wc2/best.pt", "/wc2/last.pt",
        R"({"mAP50":0.90,"mAP50-95":0.65,"precision":0.92,"recall":0.88,"fitness":0.91})");

    QVariantMap comparison = metricService.compareVersions(versionId1, versionId2);
    QVERIFY(!comparison.isEmpty());
    QCOMPARE(comparison["version1"].toString(), versionId1);
    QCOMPARE(comparison["version2"].toString(), versionId2);

    QVariantMap comp = comparison["comparison"].toMap();
    QVariantMap map50Comp = comp["mAP50"].toMap();
    QCOMPARE(map50Comp["v1"].toDouble(), 0.80);
    QCOMPARE(map50Comp["v2"].toDouble(), 0.90);
    QCOMPARE(map50Comp["diff"].toDouble(), 0.10);
    QVERIFY(map50Comp["improved"].toBool());
}

void TestModel::testModelVersionModel()
{
    ModelVersionModel model;
    QCOMPARE(model.count(), 0);

    model.setProjectId(m_projectId);
    QVERIFY(model.count() >= 1);

    // Check data access
    QModelIndex idx = model.index(0, 0);
    QString versionId = model.data(idx, ModelVersionModel::IdRole).toString();
    QVERIFY(!versionId.isEmpty());

    QString runId = model.data(idx, ModelVersionModel::RunIdRole).toString();
    QCOMPARE(runId, m_runId);

    QString bestWeight = model.data(idx, ModelVersionModel::BestWeightRole).toString();
    QVERIFY(!bestWeight.isEmpty());

    // Invalid index returns empty
    QModelIndex invalidIdx = model.index(-1, 0);
    QVERIFY(model.data(invalidIdx, ModelVersionModel::IdRole).isNull());

    // Role names
    QHash<int, QByteArray> roles = model.roleNames();
    QVERIFY(roles.contains(ModelVersionModel::IdRole));
    QVERIFY(roles.contains(ModelVersionModel::RunIdRole));
    QVERIFY(roles.contains(ModelVersionModel::MetricsRole));
    QVERIFY(roles.contains(ModelVersionModel::ParentVersionRole));
    QVERIFY(roles.contains(ModelVersionModel::CreatedAtRole));
}

QTEST_MAIN(TestModel)
#include "test_model.moc"
