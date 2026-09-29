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
    void testPhysicalRemoveClass();
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
    // 新语义：废弃 + 保留 id 空位。类列表长度不变，后续 class_id 不前移，
    // 否则已有 YOLO 标签中的 class_id 会整体错位。
    QVERIFY(m_taxonomyService->removeClass(m_customTaxonomyId, 0));  // 废弃 defect_a

    QVariantList classes = m_taxonomyService->getClasses(m_customTaxonomyId);
    // 删除后 size 不变：4 个 class_id 槽位全部保留
    QCOMPARE(classes.size(), 4);
    // 被删索引为空（已废弃）
    QVERIFY(classes[0].toString().isEmpty());
    // 后续索引类名不变（class_id 不前移）
    QCOMPARE(classes[1].toString(), QString("defect_b"));
    QCOMPARE(classes[2].toString(), QString("defect_c"));
    QCOMPARE(classes[3].toString(), QString("defect_d"));

    // 重复废弃同一索引应失败
    QVERIFY(!m_taxonomyService->removeClass(m_customTaxonomyId, 0));
    QCOMPARE(m_taxonomyService->lastErrorCode(), QString("E_ALREADY_DEPRECATED"));

    // Invalid index
    QVERIFY(!m_taxonomyService->removeClass(m_customTaxonomyId, -1));
    QVERIFY(!m_taxonomyService->removeClass(m_customTaxonomyId, 99));
}

void TestTaxonomy::testRenameClass()
{
    // 重命名存活类别（索引 1 = defect_b），废弃位不受影响
    QVERIFY(m_taxonomyService->renameClass(m_customTaxonomyId, 1, "scratch"));

    QVariantList classes = m_taxonomyService->getClasses(m_customTaxonomyId);
    QCOMPARE(classes[1].toString(), QString("scratch"));
    QVERIFY(classes[0].toString().isEmpty());  // 废弃位仍为空

    // 对废弃位重命名等价于复活该 class_id
    QVERIFY(m_taxonomyService->renameClass(m_customTaxonomyId, 0, "revived"));
    classes = m_taxonomyService->getClasses(m_customTaxonomyId);
    QCOMPARE(classes[0].toString(), QString("revived"));

    // Invalid index
    QVERIFY(!m_taxonomyService->renameClass(m_customTaxonomyId, -1, "bad"));
    QVERIFY(!m_taxonomyService->renameClass(m_customTaxonomyId, 99, "bad"));
}

void TestTaxonomy::testReorderClasses()
{
    QVariantList newOrder;
    newOrder << "defect_d" << "scratch" << "revived" << "defect_c";
    QVERIFY(m_taxonomyService->reorderClasses(m_customTaxonomyId, newOrder));

    QVariantList classes = m_taxonomyService->getClasses(m_customTaxonomyId);
    QCOMPARE(classes.size(), 4);
    QCOMPARE(classes[0].toString(), QString("defect_d"));
    QCOMPARE(classes[1].toString(), QString("scratch"));
    QCOMPARE(classes[2].toString(), QString("revived"));
    QCOMPARE(classes[3].toString(), QString("defect_c"));
}

void TestTaxonomy::testPhysicalRemoveClass()
{
    // 物理删除仅在无引用时允许：无引用则索引前移、长度缩短
    QVariantList classes;
    classes << "pa" << "pb" << "pc";
    QString taxId = m_taxonomyService->createTaxonomy(m_projectId, "Physical Test", classes);
    QVERIFY(!taxId.isEmpty());

    QVERIFY(m_taxonomyService->removeClass(taxId, 1, true));  // forcePhysical
    QVariantList after = m_taxonomyService->getClasses(taxId);
    QCOMPARE(after.size(), 2);
    QCOMPARE(after[0].toString(), QString("pa"));
    QCOMPARE(after[1].toString(), QString("pc"));

    // 有快照引用时物理删除被拒，只能标记废弃
    QVariantList classes2;
    classes2 << "ra" << "rb";
    QString taxId2 = m_taxonomyService->createTaxonomy(m_projectId, "In Use Test", classes2);
    QVERIFY(!taxId2.isEmpty());

    QSqlQuery q(Database::instance().database());
    q.prepare("INSERT INTO datasets (id, project_id, name, image_root, label_root, format, sample_count, import_status) "
              "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue("ds-tax-phys-test");
    q.addBindValue(m_projectId);
    q.addBindValue("PhysTestDs");
    q.addBindValue("/tmp/tax-img");
    q.addBindValue("/tmp/tax-lbl");
    q.addBindValue("yolo_txt");
    q.addBindValue(0);
    q.addBindValue("completed");
    QVERIFY(q.exec());

    q.prepare("INSERT INTO dataset_snapshots (id, dataset_id, sample_manifest_json, taxonomy_version) "
              "VALUES (?, ?, ?, ?)");
    q.addBindValue("snap-tax-phys-test");
    q.addBindValue("ds-tax-phys-test");
    q.addBindValue("[]");
    q.addBindValue(taxId2 + QStringLiteral(":1"));
    QVERIFY(q.exec());

    // 有引用：物理删除被拒
    QVERIFY(!m_taxonomyService->removeClass(taxId2, 0, true));
    QCOMPARE(m_taxonomyService->lastErrorCode(), QString("E_IN_USE"));

    // 标记废弃仍然允许（不改变 class_id 布局）
    QVERIFY(m_taxonomyService->removeClass(taxId2, 0, false));
    QVariantList after2 = m_taxonomyService->getClasses(taxId2);
    QCOMPARE(after2.size(), 2);
    QVERIFY(after2[0].toString().isEmpty());
    QCOMPARE(after2[1].toString(), QString("rb"));
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
