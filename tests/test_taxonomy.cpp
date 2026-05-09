#include <QTest>
#include <QCoreApplication>
#include <QSqlQuery>
#include "Database.h"
#include "TaxonomyService.h"
#include "ProjectService.h"

class TestTaxonomy : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testCreateTaxonomy();
    void testAddClass();
    void testRemoveClass();
    void testRenameClass();
    void testReorderClasses();
    void testTaxonomyVersion();
    void testDeleteTaxonomy();
    void cleanupTestCase();

private:
    TaxonomyService *m_taxonomyService = nullptr;
    ProjectService *m_projectService = nullptr;
    QString m_projectId;
    QString m_customTaxonomyId;  // ID of the taxonomy created in testCreateTaxonomy
};

void TestTaxonomy::initTestCase()
{
    Database::instance().open(":memory:");
    Database::instance().initializeSchema();
    m_taxonomyService = new TaxonomyService(this);
    m_projectService = new ProjectService(this);
    m_projectService->setTaxonomyService(m_taxonomyService);

    // Create a test project via ProjectService (which auto-creates default taxonomy)
    m_projectId = m_projectService->createProject("TestProject", "/tmp/test");
    QVERIFY(!m_projectId.isEmpty());
}

void TestTaxonomy::testCreateTaxonomy()
{
    QVariantList classes;
    classes << "defect_a" << "defect_b" << "defect_c";
    m_customTaxonomyId = m_taxonomyService->createTaxonomy(m_projectId, "Test Taxonomy", classes);
    QVERIFY(!m_customTaxonomyId.isEmpty());

    QVariantMap t = m_taxonomyService->getTaxonomy(m_customTaxonomyId);
    QCOMPARE(t["name"].toString(), QString("Test Taxonomy"));
    QCOMPARE(t["version"].toInt(), 1);
}

void TestTaxonomy::testAddClass()
{
    QVERIFY(m_taxonomyService->addClass(m_customTaxonomyId, "defect_d"));

    QVariantList classes = m_taxonomyService->getClasses(m_customTaxonomyId);
    QCOMPARE(classes.size(), 4);
    QCOMPARE(classes[3].toString(), QString("defect_d"));
}

void TestTaxonomy::testRemoveClass()
{
    QVERIFY(m_taxonomyService->removeClass(m_customTaxonomyId, 0));  // Remove defect_a

    QVariantList classes = m_taxonomyService->getClasses(m_customTaxonomyId);
    QCOMPARE(classes.size(), 3);
    QCOMPARE(classes[0].toString(), QString("defect_b"));

    // Invalid index
    QVERIFY(!m_taxonomyService->removeClass(m_customTaxonomyId, -1));
    QVERIFY(!m_taxonomyService->removeClass(m_customTaxonomyId, 99));
}

void TestTaxonomy::testRenameClass()
{
    QVERIFY(m_taxonomyService->renameClass(m_customTaxonomyId, 0, "scratch"));

    QVariantList classes = m_taxonomyService->getClasses(m_customTaxonomyId);
    QCOMPARE(classes[0].toString(), QString("scratch"));

    // Invalid index
    QVERIFY(!m_taxonomyService->renameClass(m_customTaxonomyId, -1, "bad"));
    QVERIFY(!m_taxonomyService->renameClass(m_customTaxonomyId, 99, "bad"));
}

void TestTaxonomy::testReorderClasses()
{
    QVariantList newOrder;
    newOrder << "defect_d" << "scratch" << "defect_c";
    QVERIFY(m_taxonomyService->reorderClasses(m_customTaxonomyId, newOrder));

    QVariantList classes = m_taxonomyService->getClasses(m_customTaxonomyId);
    QCOMPARE(classes.size(), 3);
    QCOMPARE(classes[0].toString(), QString("defect_d"));
    QCOMPARE(classes[1].toString(), QString("scratch"));
    QCOMPARE(classes[2].toString(), QString("defect_c"));
}

void TestTaxonomy::testTaxonomyVersion()
{
    // Version should have been incremented by the add/remove/rename/reorder operations
    int version = m_taxonomyService->getTaxonomyVersion(m_customTaxonomyId);
    QVERIFY(version > 1);  // Started at 1, should be higher after modifications
}

void TestTaxonomy::testDeleteTaxonomy()
{
    QString id = m_taxonomyService->createTaxonomy(m_projectId, "To Delete", {});
    QVERIFY(!id.isEmpty());
    QVERIFY(m_taxonomyService->deleteTaxonomy(id));

    QVariantMap t = m_taxonomyService->getTaxonomy(id);
    QVERIFY(t.isEmpty());
}

void TestTaxonomy::cleanupTestCase()
{
    delete m_projectService;
    delete m_taxonomyService;
    Database::instance().close();
}

QTEST_MAIN(TestTaxonomy)
#include "test_taxonomy.moc"
