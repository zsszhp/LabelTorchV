#include <QTest>
#include <QSqlQuery>
#include <QDir>
#include <QDebug>
#include <QSqlError>
#include "database/Database.h"
#include "database/Schema.h"
#include "DatasetService.h"
#include "ProjectService.h"
#include "TaxonomyService.h"

class TestDatabase : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testCreateTables();
    void testInsertProject();
    void testSchemaVersion();
    void testImportLabelMeUser();
    void testProjectSerializationAndMigration();
    void testDuplicateProjectCreation();
    void testDeleteDataset();
    void cleanupTestCase();

private:
    QString m_dbPath;
};

void TestDatabase::initTestCase()
{
    m_dbPath = QDir::tempPath() + "/labeltorch_test.db";
    QFile::remove(m_dbPath);
    QVERIFY(Database::instance().open(m_dbPath));
}

void TestDatabase::testCreateTables()
{
    QVERIFY(Database::instance().initializeSchema());
    QVERIFY(Database::instance().isOpen());
}

void TestDatabase::testInsertProject()
{
    QSqlQuery query(Database::instance().database());
    query.prepare("INSERT INTO projects (id, name, root_path) VALUES (?, ?, ?)");
    query.addBindValue("test-id-1");
    query.addBindValue("测试项目");
    query.addBindValue("/tmp/test_project");
    QVERIFY(query.exec());
}

void TestDatabase::testSchemaVersion()
{
    QSqlQuery query(Database::instance().database());
    QVERIFY(query.exec("SELECT version FROM schema_version ORDER BY version DESC LIMIT 1"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);
}

void TestDatabase::testImportLabelMeUser()
{
    // 1. Initialize a test project
    QSqlQuery query(Database::instance().database());
    query.exec("DELETE FROM projects WHERE id = 'test-proj-lm'");
    query.prepare("INSERT INTO projects (id, name, root_path) VALUES (?, ?, ?)");
    query.addBindValue("test-proj-lm");
    query.addBindValue("LabelMeTestProj");
    query.addBindValue(QDir::tempPath() + "/lm_proj_root");
    QVERIFY(query.exec());

    // 2. Scan directories
    QString imageDir = "D:/z/work/superstar/robot/img/biaozhu/ExportImage";
    QString labelDir = "D:/z/work/superstar/robot/img/biaozhu/ExportLabel";

    if (!QDir(imageDir).exists() || !QDir(labelDir).exists()) {
        qDebug() << "Test folders do not exist. Skipping user import test.";
        return;
    }

    DatasetService dsService;
    QVariantMap scanResult = dsService.scanSeparate(imageDir, labelDir);
    qDebug() << "=== SCAN RESULT ===";
    qDebug() << "isValid:" << scanResult["isValid"].toBool();
    qDebug() << "imageCount:" << scanResult["imageCount"].toInt();
    qDebug() << "labelCount:" << scanResult["labelCount"].toInt();
    qDebug() << "detectedFormat:" << scanResult["detectedFormat"].toString();
    qDebug() << "classIds:" << scanResult["classIds"].toList();
    if (scanResult.contains("error")) {
        qDebug() << "error:" << scanResult["error"].toString();
    }

    // 3. Import dataset
    QString dsId = dsService.importDatasetSeparate(
        "test-proj-lm",
        "LabelMeUserDataset",
        imageDir,
        labelDir
    );

    qDebug() << "=== IMPORT RESULT ===";
    qDebug() << "Imported dataset ID:" << dsId;

    if (dsId.isEmpty()) {
        qDebug() << "Last DB error:" << Database::instance().database().lastError().text();
    } else {
        QSqlQuery sampleQuery(Database::instance().database());
        sampleQuery.prepare("SELECT count(*) FROM dataset_samples WHERE dataset_id = ?");
        sampleQuery.addBindValue(dsId);
        if (sampleQuery.exec() && sampleQuery.next()) {
            qDebug() << "Samples inserted count:" << sampleQuery.value(0).toInt();
        }
        
        QSqlQuery schemaQuery(Database::instance().database());
        schemaQuery.prepare("SELECT raw_class_names_json, raw_class_order_json, source_format FROM imported_label_schemas WHERE dataset_id = ?");
        schemaQuery.addBindValue(dsId);
        if (schemaQuery.exec() && schemaQuery.next()) {
            qDebug() << "Schema raw_class_names:" << schemaQuery.value(0).toString();
            qDebug() << "Schema raw_class_order:" << schemaQuery.value(1).toString();
            qDebug() << "Schema source_format:" << schemaQuery.value(2).toString();
        }
    }
}

void TestDatabase::testProjectSerializationAndMigration()
{
    ProjectService projService;
    TaxonomyService taxService;
    projService.setTaxonomyService(&taxService);

    QString rootPath = QDir::tempPath() + "/serialized_project_test";
    QDir(rootPath).removeRecursively(); // Ensure clean start

    QString projId = projService.createProject("测试序列化项目", rootPath);
    QVERIFY(!projId.isEmpty());

    // 验证懒加载目录结构：初始应该只有 project.json 文件
    QFileInfo jsonInfo(rootPath + "/project.json");
    QVERIFY(jsonInfo.exists());

    QDir dir(rootPath);
    QStringList files = dir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries);
    QCOMPARE(files.size(), 1); 
    QCOMPARE(files[0], QString("project.json"));

    // 修改任务类型，触发元数据自动保存
    QVERIFY(projService.setTaskType(projId, "obb"));

    // 验证 project.json 中的 task_type 已更新为 obb
    QFile file(rootPath + "/project.json");
    QVERIFY(file.open(QIODevice::ReadOnly));
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    QVERIFY(doc.isObject());
    QCOMPARE(doc.object()["task_type"].toString(), QString("obb"));

    // 从数据库中删除项目前，先清理外键关联的类别体系记录以防止约束冲突
    QSqlQuery deleteRefQuery(Database::instance().database());
    deleteRefQuery.prepare("DELETE FROM taxonomies WHERE project_id = ?");
    deleteRefQuery.addBindValue(projId);
    QVERIFY(deleteRefQuery.exec());

    // 从数据库中删除项目，模拟迁移或数据库清空场景
    QVERIFY(projService.deleteProject(projId));

    // 验证项目记录已在数据库中清除
    QSqlQuery query(Database::instance().database());
    query.prepare("SELECT id FROM projects WHERE id = ?");
    query.addBindValue(projId);
    QVERIFY(query.exec());
    QVERIFY(!query.next());

    // 通过项目目录重新导入，恢复项目状态
    QString importedId = projService.importProject(rootPath);
    QCOMPARE(importedId, projId);

    // 验证项目数据已被成功高保真还原回 SQLite
    QSqlQuery query2(Database::instance().database());
    query2.prepare("SELECT task_type FROM projects WHERE id = ?");
    query2.addBindValue(projId);
    QVERIFY(query2.exec());
    QVERIFY(query2.next());
    QCOMPARE(query2.value(0).toString(), QString("obb"));

    // 清理临时文件
    QDir(rootPath).removeRecursively();
}

void TestDatabase::testDuplicateProjectCreation()
{
    ProjectService projService;
    TaxonomyService taxService;
    projService.setTaxonomyService(&taxService);

    QString rootPath = QDir::tempPath() + "/duplicate_project_test";
    QDir(rootPath).removeRecursively(); // Ensure clean start

    // 1. 创建项目
    QString projId1 = projService.createProject("项目1", rootPath);
    QVERIFY(!projId1.isEmpty());

    // 2. 再次尝试用相同路径创建项目，期望能够成功返回已存在的项目ID而不会因为 UNIQUE 约束失败
    QString projId2 = projService.createProject("项目2", rootPath);
    QCOMPARE(projId1, projId2);

    // 3. 从数据库删除项目记录，保留物理上的 project.json
    // 清除外键关联
    QSqlQuery deleteRefQuery(Database::instance().database());
    deleteRefQuery.prepare("DELETE FROM taxonomies WHERE project_id = ?");
    deleteRefQuery.addBindValue(projId1);
    QVERIFY(deleteRefQuery.exec());

    QVERIFY(projService.deleteProject(projId1));

    // 此时数据库中无此项目，但物理上有 project.json。
    // 再次调用 createProject 应触发自动 import 并返回正确的 ID 恢复项目
    QString projId3 = projService.createProject("项目3", rootPath);
    QCOMPARE(projId3, projId1);

    // 4. 清理临时文件
    QDir(rootPath).removeRecursively();
}

void TestDatabase::testDeleteDataset()
{
    // 1. Create a project
    QSqlQuery query(Database::instance().database());
    query.exec("DELETE FROM projects WHERE id = 'test-proj-delete'");
    query.prepare("INSERT INTO projects (id, name, root_path) VALUES (?, ?, ?)");
    query.addBindValue("test-proj-delete");
    query.addBindValue("DeleteTestProj");
    query.addBindValue("/tmp/delete_proj");
    QVERIFY(query.exec());

    // 2. Create a dataset
    QString dsId = "test-ds-delete";
    query.prepare("INSERT INTO datasets (id, project_id, name, image_root, label_root, format, sample_count, import_status) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(dsId);
    query.addBindValue("test-proj-delete");
    query.addBindValue("DeleteTestDS");
    query.addBindValue("/tmp/delete_ds/images");
    query.addBindValue("/tmp/delete_ds/labels");
    query.addBindValue("yolo_txt");
    query.addBindValue(1);
    query.addBindValue("completed");
    QVERIFY(query.exec());

    // 3. Create a sample
    query.prepare("INSERT INTO dataset_samples (id, dataset_id, image_path, label_path, validation_status) "
                  "VALUES (?, ?, ?, ?, ?)");
    query.addBindValue("test-sample-delete");
    query.addBindValue(dsId);
    query.addBindValue("/tmp/delete_ds/images/1.jpg");
    query.addBindValue("/tmp/delete_ds/labels/1.txt");
    query.addBindValue("valid");
    QVERIFY(query.exec());

    // 4. Create an imported label schema
    query.prepare("INSERT INTO imported_label_schemas (id, dataset_id, raw_class_names_json, raw_class_order_json, source_format) "
                  "VALUES (?, ?, ?, ?, ?)");
    query.addBindValue("test-schema-delete");
    query.addBindValue(dsId);
    query.addBindValue("[\"class0\"]");
    query.addBindValue("[0]");
    query.addBindValue("yolo_txt");
    QVERIFY(query.exec());

    // 5. Create a snapshot referencing the dataset
    QString snapId = "test-snap-delete";
    query.prepare("INSERT INTO dataset_snapshots (id, dataset_id, sample_manifest_json) "
                  "VALUES (?, ?, ?)");
    query.addBindValue(snapId);
    query.addBindValue(dsId);
    query.addBindValue("{}");
    QVERIFY(query.exec());

    // 6. Create a training run referencing the snapshot
    QString runId = "test-run-delete";
    query.prepare("INSERT INTO training_runs (id, project_id, snapshot_id, config_snapshot_json) "
                  "VALUES (?, ?, ?, ?)");
    query.addBindValue(runId);
    query.addBindValue("test-proj-delete");
    query.addBindValue(snapId);
    query.addBindValue("{}");
    QVERIFY(query.exec());

    // 7. Create a model version referencing the training run
    QString modelId = "test-model-delete";
    query.prepare("INSERT INTO model_versions (id, run_id) "
                  "VALUES (?, ?)");
    query.addBindValue(modelId);
    query.addBindValue(runId);
    QVERIFY(query.exec());

    // 8. Create an export artifact referencing the model version
    query.prepare("INSERT INTO export_artifacts (id, model_version_id, format, output_path) "
                  "VALUES (?, ?, ?, ?)");
    query.addBindValue("test-export-delete");
    query.addBindValue(modelId);
    query.addBindValue("onnx");
    query.addBindValue("/tmp/export.onnx");
    QVERIFY(query.exec());

    // 9. Call DatasetService::deleteDataset
    DatasetService dsService;
    bool success = dsService.deleteDataset(dsId);
    QVERIFY(success);

    // 10. Verify that all records are deleted
    query.prepare("SELECT count(*) FROM datasets WHERE id = ?");
    query.addBindValue(dsId);
    QVERIFY(query.exec());
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 0);

    query.prepare("SELECT count(*) FROM dataset_snapshots WHERE id = ?");
    query.addBindValue(snapId);
    QVERIFY(query.exec());
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 0);

    query.prepare("SELECT count(*) FROM training_runs WHERE id = ?");
    query.addBindValue(runId);
    QVERIFY(query.exec());
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 0);

    query.prepare("SELECT count(*) FROM model_versions WHERE id = ?");
    query.addBindValue(modelId);
    QVERIFY(query.exec());
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 0);
}

void TestDatabase::cleanupTestCase()
{
    Database::instance().close();
    QFile::remove(m_dbPath);
}

QTEST_MAIN(TestDatabase)
#include "test_database.moc"
