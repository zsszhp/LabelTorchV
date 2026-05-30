#include <QTest>
#include <QSqlQuery>
#include <QDir>
#include <QDebug>
#include <QSqlError>
#include "database/Database.h"
#include "database/Schema.h"
#include "DatasetService.h"
#include "ProjectService.h"

class TestDatabase : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testCreateTables();
    void testInsertProject();
    void testSchemaVersion();
    void testImportLabelMeUser();
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

void TestDatabase::cleanupTestCase()
{
    Database::instance().close();
    QFile::remove(m_dbPath);
}

QTEST_MAIN(TestDatabase)
#include "test_database.moc"
