/**
 * @file test_project_cascade.cpp
 * @brief P0-5 项目删除级联测试
 *
 * 期望行为：deleteProject 后，所有子表中引用该项目的记录必须被一并删除，
 * 不得留下孤儿记录（taxonomies / datasets / dataset_samples / dataset_snapshots /
 * training_runs / model_versions / export_artifacts / run_metrics 等）。
 */
#include <QTest>
#include <QCoreApplication>
#include <QSqlQuery>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>

#include "Database.h"
#include "ProjectService.h"
#include "TaxonomyService.h"

class TestProjectCascade : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void testDeleteProjectRemovesChildRows();
    void testDeleteNonexistentProjectReturnsFalse();
    void testDeleteProjectTwiceIsSafe();

private:
    /// 统计某张表中与指定项目关联的行数
    int countRows(const QString &sql, const QString &projectId);
    /// 构造完整项目 + 全部子表记录，返回 projectId
    QString buildProjectWithChildren(const QString &tag);

    QTemporaryDir m_tmpDir;
    ProjectService *m_projectService = nullptr;
    TaxonomyService *m_taxonomyService = nullptr;
};

void TestProjectCascade::initTestCase()
{
    QVERIFY2(m_tmpDir.isValid(), "无法创建临时目录");

    QString dbPath = m_tmpDir.path() + "/test_project_cascade.db";
    QFile::remove(dbPath);
    QFile::remove(dbPath + "-journal");
    QFile::remove(dbPath + "-wal");
    Database::instance().open(dbPath);
    Database::instance().initializeSchema();

    m_taxonomyService = new TaxonomyService(this);
    m_projectService = new ProjectService(this);
    m_projectService->setTaxonomyService(m_taxonomyService);
}

int TestProjectCascade::countRows(const QString &sql, const QString &projectId)
{
    QSqlQuery q(Database::instance().database());
    q.prepare(sql);
    q.addBindValue(projectId);
    if (!q.exec() || !q.next()) return -1;
    return q.value(0).toInt();
}

QString TestProjectCascade::buildProjectWithChildren(const QString &tag)
{
    QString rootPath = m_tmpDir.path() + "/proj_" + tag;
    QDir().mkpath(rootPath);

    QString projectId = m_projectService->createProject("Cascade_" + tag, rootPath, "detect");
    if (projectId.isEmpty()) return {};

    auto db = Database::instance().database();
    QSqlQuery q(db);

    // 数据集
    QString dsId = "ds-" + tag;
    q.prepare("INSERT INTO datasets (id, project_id, name, image_root, label_root, format, "
              "sample_count, import_status) VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(dsId);
    q.addBindValue(projectId);
    q.addBindValue("DS_" + tag);
    q.addBindValue(rootPath + "/images");
    q.addBindValue(rootPath + "/labels");
    q.addBindValue("yolo_txt");
    q.addBindValue(1);
    q.addBindValue("completed");
    if (!q.exec()) return {};

    // 样本
    q.prepare("INSERT INTO dataset_samples (id, dataset_id, image_path, label_path, validation_status) "
              "VALUES (?, ?, ?, ?, ?)");
    q.addBindValue("sample-" + tag);
    q.addBindValue(dsId);
    q.addBindValue(rootPath + "/images/a.jpg");
    q.addBindValue(rootPath + "/labels/a.txt");
    q.addBindValue("valid");
    if (!q.exec()) return {};

    // 导入标签 schema
    q.prepare("INSERT INTO imported_label_schemas (id, dataset_id, raw_class_names_json, "
              "raw_class_order_json, source_format) VALUES (?, ?, ?, ?, ?)");
    q.addBindValue("schema-" + tag);
    q.addBindValue(dsId);
    q.addBindValue("[\"defect\"]");
    q.addBindValue("[0]");
    q.addBindValue("yolo_txt");
    if (!q.exec()) return {};

    // 快照
    QString snapId = "snap-" + tag;
    q.prepare("INSERT INTO dataset_snapshots (id, dataset_id, sample_manifest_json, "
              "split_manifest_json, taxonomy_version, annotation_revision_boundary) "
              "VALUES (?, ?, ?, ?, ?, ?)");
    q.addBindValue(snapId);
    q.addBindValue(dsId);
    q.addBindValue("[\"sample-" + tag + "\"]");
    q.addBindValue("{\"train\":[\"sample-" + tag + "\"],\"val\":[]}");
    q.addBindValue("none:v1");
    q.addBindValue("none");
    if (!q.exec()) return {};

    // 训练运行
    QString runId = "run-" + tag;
    q.prepare("INSERT INTO training_runs (id, project_id, snapshot_id, config_snapshot_json, status) "
              "VALUES (?, ?, ?, ?, 'succeeded')");
    q.addBindValue(runId);
    q.addBindValue(projectId);
    q.addBindValue(snapId);
    q.addBindValue("{}");
    if (!q.exec()) return {};

    // 训练指标
    q.prepare("INSERT INTO run_metrics (id, run_id, epoch, metric_name, metric_value) "
              "VALUES (?, ?, ?, ?, ?)");
    q.addBindValue("metric-" + tag);
    q.addBindValue(runId);
    q.addBindValue(1);
    q.addBindValue("mAP50");
    q.addBindValue(0.9);
    if (!q.exec()) return {};

    // 模型版本
    QString mvId = "mv-" + tag;
    q.prepare("INSERT INTO model_versions (id, run_id, best_weight_path, last_weight_path, "
              "metrics_snapshot_json, project_id) VALUES (?, ?, ?, ?, ?, ?)");
    q.addBindValue(mvId);
    q.addBindValue(runId);
    q.addBindValue(rootPath + "/best.pt");
    q.addBindValue(rootPath + "/last.pt");
    q.addBindValue("{}");
    q.addBindValue(projectId);
    if (!q.exec()) return {};

    // 导出产物
    q.prepare("INSERT INTO export_artifacts (id, model_version_id, format, options_snapshot_json, "
              "output_path, validation_result, status) VALUES (?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue("exp-" + tag);
    q.addBindValue(mvId);
    q.addBindValue("onnx");
    q.addBindValue("{}");
    q.addBindValue(rootPath + "/model.onnx");
    q.addBindValue("{}");
    q.addBindValue("succeeded");
    if (!q.exec()) return {};

    // 标注修订
    q.prepare("INSERT INTO annotation_revisions (id, dataset_id, sample_id, source_type, "
              "before_snapshot_json, after_snapshot_json) VALUES (?, ?, ?, ?, ?, ?)");
    q.addBindValue("rev-" + tag);
    q.addBindValue(dsId);
    q.addBindValue("sample-" + tag);
    q.addBindValue("manual");
    q.addBindValue("[]");
    q.addBindValue("[]");
    if (!q.exec()) return {};

    return projectId;
}

// === P0-5 用例 ===

void TestProjectCascade::testDeleteProjectRemovesChildRows()
{
    QString projectId = buildProjectWithChildren("cascade1");
    QVERIFY2(!projectId.isEmpty(), "构造带子表记录的项目应成功");

    // 删除前确认子表有记录
    QVERIFY(countRows("SELECT COUNT(*) FROM datasets WHERE project_id = ?", projectId) == 1);
    QVERIFY(countRows("SELECT COUNT(*) FROM training_runs WHERE project_id = ?", projectId) == 1);

    // 执行删除
    QVERIFY2(m_projectService->deleteProject(projectId), "deleteProject 应成功");

    // projects 表自身
    QCOMPARE(countRows("SELECT COUNT(*) FROM projects WHERE id = ?", projectId), 0);

    // 各子表均不得残留孤儿记录
    QCOMPARE(countRows("SELECT COUNT(*) FROM taxonomies WHERE project_id = ?", projectId), 0);
    QCOMPARE(countRows("SELECT COUNT(*) FROM datasets WHERE project_id = ?", projectId), 0);
    QCOMPARE(countRows(
        "SELECT COUNT(*) FROM dataset_samples WHERE dataset_id IN "
        "(SELECT id FROM datasets WHERE project_id = ?)", projectId), 0);
    QCOMPARE(countRows(
        "SELECT COUNT(*) FROM dataset_snapshots WHERE dataset_id IN "
        "(SELECT id FROM datasets WHERE project_id = ?)", projectId), 0);
    QCOMPARE(countRows(
        "SELECT COUNT(*) FROM imported_label_schemas WHERE dataset_id IN "
        "(SELECT id FROM datasets WHERE project_id = ?)", projectId), 0);
    QCOMPARE(countRows(
        "SELECT COUNT(*) FROM annotation_revisions WHERE dataset_id IN "
        "(SELECT id FROM datasets WHERE project_id = ?)", projectId), 0);
    QCOMPARE(countRows("SELECT COUNT(*) FROM training_runs WHERE project_id = ?", projectId), 0);
    QCOMPARE(countRows(
        "SELECT COUNT(*) FROM run_metrics WHERE run_id IN "
        "(SELECT id FROM training_runs WHERE project_id = ?)", projectId), 0);
    QCOMPARE(countRows("SELECT COUNT(*) FROM model_versions WHERE project_id = ?", projectId), 0);
    QCOMPARE(countRows(
        "SELECT COUNT(*) FROM model_versions WHERE run_id IN "
        "(SELECT id FROM training_runs WHERE project_id = ?)", projectId), 0);
    QCOMPARE(countRows(
        "SELECT COUNT(*) FROM export_artifacts WHERE model_version_id IN "
        "(SELECT id FROM model_versions WHERE project_id = ?)", projectId), 0);
}

void TestProjectCascade::testDeleteNonexistentProjectReturnsFalse()
{
    QVERIFY(!m_projectService->deleteProject("proj-does-not-exist-xyz"));
}

void TestProjectCascade::testDeleteProjectTwiceIsSafe()
{
    QString projectId = buildProjectWithChildren("cascade2");
    QVERIFY(!projectId.isEmpty());

    QVERIFY(m_projectService->deleteProject(projectId));
    // 二次删除不应崩溃；返回值允许 false（记录已不存在）
    m_projectService->deleteProject(projectId);
    QCOMPARE(countRows("SELECT COUNT(*) FROM projects WHERE id = ?", projectId), 0);
}

QTEST_MAIN(TestProjectCascade)
#include "test_project_cascade.moc"
