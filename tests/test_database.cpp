#include <QTest>
#include <QSqlQuery>
#include "database/Database.h"
#include "database/Schema.h"

class TestDatabase : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testCreateTables();
    void testInsertProject();
    void testSchemaVersion();
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

void TestDatabase::cleanupTestCase()
{
    Database::instance().close();
    QFile::remove(m_dbPath);
}

QTEST_MAIN(TestDatabase)
#include "test_database.moc"
