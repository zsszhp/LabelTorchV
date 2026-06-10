#include <QTest>
#include <QCoreApplication>
#include <QSqlQuery>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include "Database.h"
#include "ImportScanner.h"
#include "DatasetService.h"
#include "SnapshotService.h"

/**
 * @brief 分类模型训练端到端测试
 *
 * 覆盖范围：
 * 1. ImportScanner 分类格式探测
 * 2. DatasetService 分类数据集导入
 * 3. SnapshotService 分类快照物理目录生成
 * 4. data.yaml 格式验证
 * 5. 边界情况和错误路径
 */
class TestClassify : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // === ImportScanner 分类格式探测 ===
    void testDetectClassifyLayout();
    void testDetectClassifyLayoutMinTwoClasses();
    void testDetectClassifyLayoutExcludeDirs();
    void testDetectClassifyLayoutNoImages();
    void testDetectClassifyLayoutSingleClass();
    void testDetectClassifyLayoutNestedImages();

    // === DatasetService 分类数据集导入 ===
    void testImportClassifyFolderDataset();
    void testImportClassifyFolderLabelPathStoresClassName();
    void testImportClassifyFolderSchemaStored();
    void testImportClassifyFolderEmptyDir();
    void testImportClassifyFolderNonExistDir();
    void testImportDatasetV2ClassifyFolderDispatch();

    // === SnapshotService 分类快照 ===
    void testPrepareClassifySnapshotDir();
    void testClassifySnapshotDataYaml();
    void testClassifySnapshotNoLabelsDir();
    void testClassifySnapshotClassSubDirs();

    // === 向后兼容 ===
    void testDetectYoloLayoutNotAffected();
    void testDetectAnomalibLayoutNotAffected();
    void testDetectFlatLayoutNotAffected();

private:
    QTemporaryDir m_tmpDir;
    QString m_classifyDir;   // ImageNet 风格分类目录
    QString m_yoloDir;       // YOLO 格式目录
    QString m_anomalibDir;   // Anomalib 格式目录
    ImportScanner *m_scanner = nullptr;
    DatasetService *m_datasetService = nullptr;
    SnapshotService *m_snapshotService = nullptr;
    QString m_projectId;
    QString m_datasetId;
    QString m_taxonomyId;
};

void TestClassify::initTestCase()
{
    QVERIFY2(m_tmpDir.isValid(), "无法创建临时目录");

    // 创建 ImageNet 风格分类目录结构
    m_classifyDir = m_tmpDir.path() + "/classify_dataset";
    QDir().mkpath(m_classifyDir + "/cat");
    QDir().mkpath(m_classifyDir + "/dog");
    QDir().mkpath(m_classifyDir + "/bird");

    // 每个类别放3张图片
    for (const auto &cls : QStringList{"cat", "dog", "bird"}) {
        for (int i = 0; i < 3; ++i) {
            QFile f(m_classifyDir + "/" + cls + QString("/%1_%2.jpg").arg(cls).arg(i));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("fake_jpeg_data");
            f.close();
        }
    }

    // 创建 YOLO 格式目录（用于向后兼容测试）
    m_yoloDir = m_tmpDir.path() + "/yolo_dataset";
    QDir().mkpath(m_yoloDir + "/images");
    QDir().mkpath(m_yoloDir + "/labels");
    QFile yoloImg(m_yoloDir + "/images/img001.jpg");
    QVERIFY(yoloImg.open(QIODevice::WriteOnly));
    yoloImg.write("fake");
    yoloImg.close();
    QFile yoloLbl(m_yoloDir + "/labels/img001.txt");
    QVERIFY(yoloLbl.open(QIODevice::WriteOnly));
    yoloLbl.write("0 0.5 0.5 0.1 0.1");
    yoloLbl.close();

    // 创建 Anomalib 格式目录（用于向后兼容测试）
    m_anomalibDir = m_tmpDir.path() + "/anomalib_dataset";
    QDir().mkpath(m_anomalibDir + "/train/good");
    QDir().mkpath(m_anomalibDir + "/test/good");
    QDir().mkpath(m_anomalibDir + "/test/defective");
    QFile goodImg(m_anomalibDir + "/train/good/good001.jpg");
    QVERIFY(goodImg.open(QIODevice::WriteOnly));
    goodImg.write("fake");
    goodImg.close();

    // 初始化数据库
    QString dbPath = QCoreApplication::applicationDirPath() + "/test_classify_db";
    QFile::remove(dbPath);
    QFile::remove(dbPath + "-journal");
    QFile::remove(dbPath + "-wal");

    Database::instance().open(dbPath);
    Database::instance().initializeSchema();

    // 创建分类项目
    m_projectId = "proj-classify-test";
    auto db = Database::instance().database();
    QSqlQuery q(db);
    q.prepare("INSERT INTO projects (id, name, root_path, task_type) VALUES (?, ?, ?, ?)");
    q.addBindValue(m_projectId);
    q.addBindValue("ClassifyTestProject");
    q.addBindValue(m_tmpDir.path());
    q.addBindValue("classify");
    QVERIFY(q.exec());

    // 创建 taxonomy
    m_taxonomyId = "tax-classify-test";
    q.prepare("INSERT INTO taxonomies (id, project_id, name, version, class_definitions_json) VALUES (?, ?, ?, ?, ?)");
    q.addBindValue(m_taxonomyId);
    q.addBindValue(m_projectId);
    q.addBindValue("Default");
    q.addBindValue(1);
    q.addBindValue("[\"cat\",\"dog\",\"bird\"]");
    QVERIFY(q.exec());

    m_scanner = new ImportScanner(this);
    m_datasetService = new DatasetService(this);
    m_snapshotService = new SnapshotService(this);
}

void TestClassify::cleanupTestCase()
{
    delete m_scanner;
    delete m_datasetService;
    delete m_snapshotService;
}

// ============================================================
// ImportScanner 分类格式探测
// ============================================================

void TestClassify::testDetectClassifyLayout()
{
    // 正常的 ImageNet 风格目录应被识别为 classify_folder
    QVariantMap result = m_scanner->scanFolder(m_classifyDir);
    QVERIFY(result["isValid"].toBool());
    QCOMPARE(result["detectedFormat"].toString(), QString("classify_folder"));
    QCOMPARE(result["imageCount"].toInt(), 9); // 3类 x 3张
    QCOMPARE(result["labelCount"].toInt(), 0);
    QCOMPARE(result["unmatchedImagesCount"].toInt(), 0);
}

void TestClassify::testDetectClassifyLayoutMinTwoClasses()
{
    // 至少需要2个类别目录才识别为分类布局
    QTemporaryDir tmpDir;
    QVERIFY2(tmpDir.isValid(), "无法创建临时目录");
    QDir().mkpath(tmpDir.path() + "/cat");
    // 只有一个类别
    QFile f(tmpDir.path() + "/cat/img001.jpg");
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("fake");
    f.close();

    QVariantMap result = m_scanner->scanFolder(tmpDir.path());
    // 单类别不应识别为 classify_folder
    QVERIFY(result["detectedFormat"].toString() != "classify_folder");
}

void TestClassify::testDetectClassifyLayoutExcludeDirs()
{
    // images/labels/train/val 等目录不应被识别为类别目录
    QTemporaryDir tmpDir;
    QVERIFY2(tmpDir.isValid(), "无法创建临时目录");
    QDir().mkpath(tmpDir.path() + "/images");
    QDir().mkpath(tmpDir.path() + "/labels");
    QDir().mkpath(tmpDir.path() + "/train");
    QDir().mkpath(tmpDir.path() + "/val");

    // 在 images 下放图片，应识别为 YOLO 格式而非分类格式
    QFile f(tmpDir.path() + "/images/img001.jpg");
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("fake");
    f.close();
    QFile lf(tmpDir.path() + "/labels/img001.txt");
    QVERIFY(lf.open(QIODevice::WriteOnly));
    lf.write("0 0.5 0.5 0.1 0.1");
    lf.close();

    QVariantMap result = m_scanner->scanFolder(tmpDir.path());
    QVERIFY(result["detectedFormat"].toString() != "classify_folder");
}

void TestClassify::testDetectClassifyLayoutNoImages()
{
    // 目录下没有图片文件
    QTemporaryDir tmpDir;
    QVERIFY2(tmpDir.isValid(), "无法创建临时目录");
    QDir().mkpath(tmpDir.path() + "/cat");
    QDir().mkpath(tmpDir.path() + "/dog");
    // 空目录，无图片

    QVariantMap result = m_scanner->scanFolder(tmpDir.path());
    QVERIFY(!result["isValid"].toBool());
}

void TestClassify::testDetectClassifyLayoutSingleClass()
{
    // 单类别目录不应识别为分类布局
    QTemporaryDir tmpDir;
    QVERIFY2(tmpDir.isValid(), "无法创建临时目录");
    QDir().mkpath(tmpDir.path() + "/only_class");
    for (int i = 0; i < 5; ++i) {
        QFile f(tmpDir.path() + QString("/only_class/img%1.jpg").arg(i));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("fake");
        f.close();
    }

    QVariantMap result = m_scanner->scanFolder(tmpDir.path());
    QVERIFY(result["detectedFormat"].toString() != "classify_folder");
}

void TestClassify::testDetectClassifyLayoutNestedImages()
{
    // 类别子目录内的子目录中也有图片（递归扫描）
    QTemporaryDir tmpDir;
    QVERIFY2(tmpDir.isValid(), "无法创建临时目录");
    QDir().mkpath(tmpDir.path() + "/cat/subdir");
    QDir().mkpath(tmpDir.path() + "/dog");
    QFile f1(tmpDir.path() + "/cat/subdir/img001.jpg");
    QVERIFY(f1.open(QIODevice::WriteOnly));
    f1.write("fake");
    f1.close();
    QFile f2(tmpDir.path() + "/dog/img002.jpg");
    QVERIFY(f2.open(QIODevice::WriteOnly));
    f2.write("fake");
    f2.close();

    // detectClassifyLayout 只检查直接子目录的图片（非递归），
    // cat/subdir 下的图片不会被 collectImageFiles(dir, false) 发现
    QVariantMap result = m_scanner->scanFolder(tmpDir.path());
    // cat 目录下直接没有图片，只有子目录有，所以 cat 不会被识别为类别
    // 只有 dog 有直接图片，单类别不满足条件
    QVERIFY(result["detectedFormat"].toString() != "classify_folder");
}

// ============================================================
// DatasetService 分类数据集导入
// ============================================================

void TestClassify::testImportClassifyFolderDataset()
{
    m_datasetId = m_datasetService->importDatasetV2(
        m_projectId,
        "TestClassifyDataset",
        m_classifyDir,
        "classify_folder"
    );
    QVERIFY(!m_datasetId.isEmpty());
}

void TestClassify::testImportClassifyFolderLabelPathStoresClassName()
{
    // 验证分类样本的 label_path 字段存储的是类别名
    auto db = Database::instance().database();
    QSqlQuery q(db);
    q.prepare("SELECT DISTINCT label_path FROM dataset_samples WHERE dataset_id = ?");
    q.addBindValue(m_datasetId);
    QVERIFY(q.exec());

    QStringList classNames;
    while (q.next()) {
        QString labelPath = q.value(0).toString();
        if (!labelPath.isEmpty()) {
            classNames.append(labelPath);
        }
    }
    // 应该有3个类别：cat, dog, bird
    QCOMPARE(classNames.size(), 3);
    QVERIFY(classNames.contains("cat"));
    QVERIFY(classNames.contains("dog"));
    QVERIFY(classNames.contains("bird"));
}

void TestClassify::testImportClassifyFolderSchemaStored()
{
    // 验证 imported_label_schemas 中存储了类别信息
    auto db = Database::instance().database();
    QSqlQuery q(db);
    q.prepare("SELECT source_format, raw_class_names_json FROM imported_label_schemas WHERE dataset_id = ?");
    q.addBindValue(m_datasetId);
    QVERIFY(q.exec());
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toString(), QString("classify_folder"));
    QVERIFY(!q.value(1).toString().isEmpty());
}

void TestClassify::testImportClassifyFolderEmptyDir()
{
    // 空目录导入应失败
    QTemporaryDir tmpDir;
    QVERIFY2(tmpDir.isValid(), "无法创建临时目录");
    QDir().mkpath(tmpDir.path() + "/empty_dataset");

    QString result = m_datasetService->importDatasetV2(
        m_projectId,
        "EmptyClassify",
        tmpDir.path() + "/empty_dataset",
        "classify_folder"
    );
    QVERIFY(result.isEmpty()); // 导入失败返回空字符串
}

void TestClassify::testImportClassifyFolderNonExistDir()
{
    // 不存在的目录导入应失败
    QString result = m_datasetService->importDatasetV2(
        m_projectId,
        "NonExistClassify",
        "/non/exist/path",
        "classify_folder"
    );
    QVERIFY(result.isEmpty());
}

void TestClassify::testImportDatasetV2ClassifyFolderDispatch()
{
    // 验证 importDatasetV2 正确分发 classify_folder 格式
    // 创建一个新的分类目录
    QTemporaryDir tmpDir;
    QVERIFY2(tmpDir.isValid(), "无法创建临时目录");
    QDir().mkpath(tmpDir.path() + "/apple");
    QDir().mkpath(tmpDir.path() + "/banana");
    QFile f1(tmpDir.path() + "/apple/a001.jpg");
    QVERIFY(f1.open(QIODevice::WriteOnly));
    f1.write("fake");
    f1.close();
    QFile f2(tmpDir.path() + "/banana/b001.jpg");
    QVERIFY(f2.open(QIODevice::WriteOnly));
    f2.write("fake");
    f2.close();

    // 先扫描获取格式
    QVariantMap scanResult = m_scanner->scanFolder(tmpDir.path());
    QCOMPARE(scanResult["detectedFormat"].toString(), QString("classify_folder"));

    // 再导入
    QString dsId = m_datasetService->importDatasetV2(
        m_projectId,
        "FruitDataset",
        tmpDir.path(),
        "classify_folder"
    );
    QVERIFY(!dsId.isEmpty());

    // 验证数据集格式字段
    auto db = Database::instance().database();
    QSqlQuery q(db);
    q.prepare("SELECT format, sample_count, import_status FROM datasets WHERE id = ?");
    q.addBindValue(dsId);
    QVERIFY(q.exec());
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toString(), QString("classify_folder"));
    QCOMPARE(q.value(1).toInt(), 2);
    QCOMPARE(q.value(2).toString(), QString("completed"));
}

// ============================================================
// SnapshotService 分类快照
// ============================================================

void TestClassify::testPrepareClassifySnapshotDir()
{
    // 先对分类数据集做 train/val 划分
    auto db = Database::instance().database();
    QSqlQuery q(db);
    q.prepare("SELECT id FROM dataset_samples WHERE dataset_id = ? LIMIT 1");
    q.addBindValue(m_datasetId);
    QVERIFY(q.exec());
    QVERIFY(q.next());

    // 创建快照
    QString snapshotId = m_snapshotService->createSnapshot(m_datasetId, 0.3, "random");
    QVERIFY(!snapshotId.isEmpty());

    // 准备物理目录
    QString yamlPath = m_snapshotService->prepareSnapshotPhysicalDir(snapshotId);
    QVERIFY(!yamlPath.isEmpty());
}

void TestClassify::testClassifySnapshotDataYaml()
{
    // 验证分类快照的 data.yaml 格式正确
    // 找到最新的快照
    auto db = Database::instance().database();
    QSqlQuery q(db);
    q.prepare("SELECT id FROM dataset_snapshots WHERE dataset_id = ? ORDER BY created_at DESC LIMIT 1");
    q.addBindValue(m_datasetId);
    QVERIFY(q.exec());
    QVERIFY(q.next());
    QString snapshotId = q.value(0).toString();

    // 读取 data.yaml
    QString projectRoot = m_tmpDir.path();
    QString yamlPath = projectRoot + "/cache/snapshots/" + snapshotId + "/data.yaml";
    QFile yamlFile(yamlPath);
    QVERIFY(yamlFile.exists());
    QVERIFY(yamlFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QString content = yamlFile.readAll();
    yamlFile.close();

    // 验证 data.yaml 内容
    QVERIFY(content.contains("path:"));
    QVERIFY(content.contains("train: train"));
    QVERIFY(content.contains("val: val"));
    // 分类 data.yaml 不应包含 nc 和 names
    QVERIFY(!content.contains("nc:"));
    QVERIFY(!content.contains("names:"));
}

void TestClassify::testClassifySnapshotNoLabelsDir()
{
    // 验证分类快照目录中没有 labels/ 子目录
    auto db = Database::instance().database();
    QSqlQuery q(db);
    q.prepare("SELECT id FROM dataset_snapshots WHERE dataset_id = ? ORDER BY created_at DESC LIMIT 1");
    q.addBindValue(m_datasetId);
    QVERIFY(q.exec());
    QVERIFY(q.next());
    QString snapshotId = q.value(0).toString();

    QString projectRoot = m_tmpDir.path();
    QString snapshotDir = projectRoot + "/cache/snapshots/" + snapshotId;

    // 不应有 labels 目录
    QVERIFY(!QDir(snapshotDir + "/labels").exists());
    QVERIFY(!QDir(snapshotDir + "/labels/train").exists());
}

void TestClassify::testClassifySnapshotClassSubDirs()
{
    // 验证分类快照目录下有 train/cat/, train/dog/, val/cat/ 等子目录
    auto db = Database::instance().database();
    QSqlQuery q(db);
    q.prepare("SELECT id FROM dataset_snapshots WHERE dataset_id = ? ORDER BY created_at DESC LIMIT 1");
    q.addBindValue(m_datasetId);
    QVERIFY(q.exec());
    QVERIFY(q.next());
    QString snapshotId = q.value(0).toString();

    QString projectRoot = m_tmpDir.path();
    QString snapshotDir = projectRoot + "/cache/snapshots/" + snapshotId;

    // 应有 train 和 val 目录
    QVERIFY(QDir(snapshotDir + "/train").exists());
    QVERIFY(QDir(snapshotDir + "/val").exists());

    // train 下应有类别子目录
    QDir trainDir(snapshotDir + "/train");
    QStringList classSubDirs = trainDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QVERIFY(classSubDirs.size() >= 1); // 至少有一个类别子目录

    // val 下也应有类别子目录
    QDir valDir(snapshotDir + "/val");
    QStringList valSubDirs = valDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QVERIFY(valSubDirs.size() >= 1);
}

// ============================================================
// 向后兼容测试
// ============================================================

void TestClassify::testDetectYoloLayoutNotAffected()
{
    // YOLO 格式目录仍应被正确识别
    QVariantMap result = m_scanner->scanFolder(m_yoloDir);
    QVERIFY(result["isValid"].toBool());
    QCOMPARE(result["detectedFormat"].toString(), QString("yolo_txt"));
}

void TestClassify::testDetectAnomalibLayoutNotAffected()
{
    // Anomalib 格式目录仍应被正确识别
    QVariantMap result = m_scanner->scanFolder(m_anomalibDir);
    QVERIFY(result["isValid"].toBool());
    QCOMPARE(result["detectedFormat"].toString(), QString("anomaly_unsupervised"));
}

void TestClassify::testDetectFlatLayoutNotAffected()
{
    // 扁平布局仍应被正确识别
    QTemporaryDir tmpDir;
    QVERIFY2(tmpDir.isValid(), "无法创建临时目录");
    // 创建扁平布局：图片和标签在同一目录
    QFile imgFile(tmpDir.path() + "/img001.jpg");
    QVERIFY(imgFile.open(QIODevice::WriteOnly));
    imgFile.write("fake");
    imgFile.close();
    QFile lblFile(tmpDir.path() + "/img001.txt");
    QVERIFY(lblFile.open(QIODevice::WriteOnly));
    lblFile.write("0 0.5 0.5 0.1 0.1");
    lblFile.close();

    QVariantMap result = m_scanner->scanFolder(tmpDir.path());
    QVERIFY(result["isValid"].toBool());
    // 扁平布局应识别为 yolo_txt
    QCOMPARE(result["detectedFormat"].toString(), QString("yolo_txt"));
}

QTEST_MAIN(TestClassify)
#include "test_classify.moc"
