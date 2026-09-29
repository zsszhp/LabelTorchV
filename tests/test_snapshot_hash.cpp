/**
 * @file test_snapshot_hash.cpp
 * @brief P0-1 快照哈希冻结测试
 *
 * 期望行为：
 * 1. createSnapshot 生成的 sample_manifest_json 含每个样本的 imageHash/labelHash（SHA-256）
 * 2. 源文件改动后 prepareSnapshotPhysicalDir 默认拒绝（E_SOURCE_DRIFT），不得悄悄使用漂移数据
 * 3. 显式要求物化冻结副本时（materializeFrozenCopy=true）允许以当前数据重新冻结
 * 4. 旧格式（纯 ID 数组、无哈希）manifest 应被拒绝，防止无法校验的快照进入训练
 */
#include <QTest>
#include <QCoreApplication>
#include <QSqlQuery>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QCryptographicHash>

#include "Database.h"
#include "SnapshotService.h"

namespace {

/// 计算文件 SHA-256（十六进制），与 SnapshotService::computeFileSha256 保持一致
QString sha256Of(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&f)) return {};
    return QString::fromLatin1(hash.result().toHex());
}

/// 写入文本文件（用于标签），返回是否成功
bool writeTextFile(const QString &path, const QByteArray &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(content);
    f.close();
    return true;
}

/// 写入二进制文件（用于图片占位），返回是否成功
bool writeBinFile(const QString &path, const QByteArray &content)
{
    return writeTextFile(path, content);
}

} // namespace

class TestSnapshotHash : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // P0-1 核心用例
    void testManifestContainsContentHashes();
    void testPrepareRejectsSourceDrift();
    void testPrepareAllowsMaterializeFrozenCopy();
    void testPrepareUnchangedSourceSucceeds();
    void testLegacyManifestWithoutHashRejected();
    void testMissingSourceFileRejected();

private:
    /// 从数据库读取快照的 sample_manifest_json 原文
    QString manifestJsonOf(const QString &snapshotId);
    /// 在临时目录内创建带图片+标签的样本，并插入 dataset_samples，返回 sampleId
    QString createRealSample(const QString &name, const QByteArray &imgContent,
                             const QByteArray &lblContent);
    /// 创建独立数据集 + 单样本，避免用例间样本篡改互相污染；返回 {datasetId, sampleId}
    QPair<QString, QString> createIsolatedDataset(const QString &tag,
                                                  const QByteArray &imgContent,
                                                  const QByteArray &lblContent);

    QTemporaryDir m_tmpDir;
    QString m_projectId;
    QString m_datasetId;
    QString m_taxonomyId;
    QString m_projectRoot;
    SnapshotService *m_service = nullptr;
};

void TestSnapshotHash::initTestCase()
{
    QVERIFY2(m_tmpDir.isValid(), "无法创建临时目录");
    m_projectRoot = m_tmpDir.path() + "/proj";
    QDir().mkpath(m_projectRoot);

    // 使用独立数据库文件，避免污染其它测试
    QString dbPath = m_tmpDir.path() + "/test_snapshot_hash.db";
    QFile::remove(dbPath);
    QFile::remove(dbPath + "-journal");
    QFile::remove(dbPath + "-wal");
    Database::instance().open(dbPath);
    Database::instance().initializeSchema();

    auto db = Database::instance().database();
    QSqlQuery q(db);

    m_projectId = "proj-snap-hash";
    q.prepare("INSERT INTO projects (id, name, root_path, task_type) VALUES (?, ?, ?, ?)");
    q.addBindValue(m_projectId);
    q.addBindValue("SnapHashProject");
    q.addBindValue(m_projectRoot);
    q.addBindValue("detect");
    QVERIFY(q.exec());

    m_taxonomyId = "tax-snap-hash";
    q.prepare("INSERT INTO taxonomies (id, project_id, name, version, class_definitions_json) "
              "VALUES (?, ?, ?, ?, ?)");
    q.addBindValue(m_taxonomyId);
    q.addBindValue(m_projectId);
    q.addBindValue("Default");
    q.addBindValue(1);
    q.addBindValue("[\"defect\",\"scratch\"]");
    QVERIFY(q.exec());

    m_datasetId = "ds-snap-hash";
    q.prepare("INSERT INTO datasets (id, project_id, name, image_root, label_root, format, "
              "sample_count, import_status) VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(m_datasetId);
    q.addBindValue(m_projectId);
    q.addBindValue("SnapHashDataset");
    q.addBindValue(m_projectRoot + "/images");
    q.addBindValue(m_projectRoot + "/labels");
    q.addBindValue("yolo_txt");
    q.addBindValue(0);
    q.addBindValue("completed");
    QVERIFY(q.exec());

    m_service = new SnapshotService(this);
}

void TestSnapshotHash::cleanupTestCase()
{
    delete m_service;
    m_service = nullptr;
}

QString TestSnapshotHash::manifestJsonOf(const QString &snapshotId)
{
    QSqlQuery q(Database::instance().database());
    q.prepare("SELECT sample_manifest_json FROM dataset_snapshots WHERE id = ?");
    q.addBindValue(snapshotId);
    if (!q.exec() || !q.next()) return {};
    return q.value(0).toString();
}

QString TestSnapshotHash::createRealSample(const QString &name, const QByteArray &imgContent,
                                           const QByteArray &lblContent)
{
    QString imgPath = m_projectRoot + "/images/" + name + ".jpg";
    QString lblPath = m_projectRoot + "/labels/" + name + ".txt";
    if (!writeBinFile(imgPath, imgContent)) return {};
    if (!writeTextFile(lblPath, lblContent)) return {};

    QString sampleId = "sample-" + name;
    QSqlQuery q(Database::instance().database());
    q.prepare("INSERT INTO dataset_samples (id, dataset_id, image_path, label_path, validation_status) "
              "VALUES (?, ?, ?, ?, ?)");
    q.addBindValue(sampleId);
    q.addBindValue(m_datasetId);
    q.addBindValue(imgPath);
    q.addBindValue(lblPath);
    q.addBindValue("valid");
    if (!q.exec()) return {};
    return sampleId;
}

QPair<QString, QString> TestSnapshotHash::createIsolatedDataset(const QString &tag,
                                                                const QByteArray &imgContent,
                                                                const QByteArray &lblContent)
{
    // 独立数据集 + 独立样本，保证快照 manifest 只覆盖本用例的数据
    QString dsId = "ds-iso-" + tag;
    QSqlQuery q(Database::instance().database());
    q.prepare("INSERT INTO datasets (id, project_id, name, image_root, label_root, format, "
              "sample_count, import_status) VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(dsId);
    q.addBindValue(m_projectId);
    q.addBindValue("Iso_" + tag);
    q.addBindValue(m_projectRoot + "/images");
    q.addBindValue(m_projectRoot + "/labels");
    q.addBindValue("yolo_txt");
    q.addBindValue(1);
    q.addBindValue("completed");
    if (!q.exec()) return {};

    QString imgPath = m_projectRoot + "/images/" + tag + ".jpg";
    QString lblPath = m_projectRoot + "/labels/" + tag + ".txt";
    if (!writeBinFile(imgPath, imgContent)) return {};
    if (!writeTextFile(lblPath, lblContent)) return {};

    QString sampleId = "sample-iso-" + tag;
    q.prepare("INSERT INTO dataset_samples (id, dataset_id, image_path, label_path, validation_status) "
              "VALUES (?, ?, ?, ?, ?)");
    q.addBindValue(sampleId);
    q.addBindValue(dsId);
    q.addBindValue(imgPath);
    q.addBindValue(lblPath);
    q.addBindValue("valid");
    if (!q.exec()) return {};

    return qMakePair(dsId, sampleId);
}

// === P0-1 用例 ===

void TestSnapshotHash::testManifestContainsContentHashes()
{
    // 准备两个真实样本（内容不同，哈希应不同）
    QString idA = createRealSample("hashA", QByteArray("image-bytes-A"), QByteArray("0 0.5 0.5 0.1 0.1\n"));
    QString idB = createRealSample("hashB", QByteArray("image-bytes-B"), QByteArray("1 0.3 0.3 0.2 0.2\n"));
    QVERIFY(!idA.isEmpty());
    QVERIFY(!idB.isEmpty());

    QString snapshotId = m_service->createSnapshot(m_datasetId, 0.5, "sequential");
    QVERIFY2(!snapshotId.isEmpty(), "createSnapshot 应成功");

    // 读取 manifest 原文，必须是对象数组且带哈希
    QString json = manifestJsonOf(snapshotId);
    QVERIFY(!json.isEmpty());

    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    QJsonArray arr = doc.array();
    QCOMPARE(arr.size(), 2);

    QString expectImgA = sha256Of(m_projectRoot + "/images/hashA.jpg");
    QString expectLblA = sha256Of(m_projectRoot + "/labels/hashA.txt");
    QVERIFY(!expectImgA.isEmpty());
    QVERIFY(!expectLblA.isEmpty());

    bool foundA = false;
    for (const auto &v : arr) {
        QJsonObject obj = v.toObject();
        // 旧格式（纯字符串）在此直接失败
        QVERIFY2(v.isObject(), "manifest 条目必须是含哈希的对象，而不是纯 ID 字符串");
        QVERIFY(obj.contains(QStringLiteral("id")));
        QVERIFY2(obj.contains(QStringLiteral("imageHash")), "manifest 条目必须含 imageHash");
        QVERIFY2(obj.contains(QStringLiteral("labelHash")), "manifest 条目必须含 labelHash");

        if (obj[QStringLiteral("id")].toString() == idA) {
            foundA = true;
            QCOMPARE(obj[QStringLiteral("imageHash")].toString(), expectImgA);
            QCOMPARE(obj[QStringLiteral("labelHash")].toString(), expectLblA);
        }
    }
    QVERIFY2(foundA, "manifest 中应能定位到样本 hashA");
}

void TestSnapshotHash::testPrepareRejectsSourceDrift()
{
    // 独立数据集，避免与其它用例的样本篡改互相干扰
    auto iso = createIsolatedDataset("driftC", QByteArray("drift-img-v1"),
                                     QByteArray("0 0.5 0.5 0.1 0.1\n"));
    QVERIFY(!iso.first.isEmpty());

    QString snapshotId = m_service->createSnapshot(iso.first, 1.0, "sequential");
    QVERIFY(!snapshotId.isEmpty());

    // 篡改源图片内容 → 哈希漂移
    QString imgPath = m_projectRoot + "/images/driftC.jpg";
    QVERIFY(writeBinFile(imgPath, QByteArray("drift-img-V2-MODIFIED")));

    // 默认（materializeFrozenCopy=false）必须拒绝
    QString yamlPath = m_service->prepareSnapshotPhysicalDir(snapshotId, false);
    QVERIFY2(yamlPath.isEmpty(), "源数据漂移时 prepare 必须拒绝");
    QCOMPARE(m_service->lastErrorCode(), QString("E_SOURCE_DRIFT"));
    QVERIFY2(m_service->lastError().contains(QStringLiteral("变更"))
                 || m_service->lastError().contains(QStringLiteral("哈希")),
             "错误信息应说明源数据已变更");
}

void TestSnapshotHash::testPrepareAllowsMaterializeFrozenCopy()
{
    auto iso = createIsolatedDataset("frozenD", QByteArray("frozen-img-v1"),
                                     QByteArray("0 0.4 0.4 0.1 0.1\n"));
    QVERIFY(!iso.first.isEmpty());

    QString snapshotId = m_service->createSnapshot(iso.first, 1.0, "sequential");
    QVERIFY(!snapshotId.isEmpty());

    // 篡改源文件后，显式要求物化冻结副本：允许继续
    QString imgPath = m_projectRoot + "/images/frozenD.jpg";
    QVERIFY(writeBinFile(imgPath, QByteArray("frozen-img-V2")));

    QString yamlPath = m_service->prepareSnapshotPhysicalDir(snapshotId, true);
    QVERIFY2(!yamlPath.isEmpty(), "显式物化冻结副本时应允许 prepare 继续");
    QVERIFY(QFile::exists(yamlPath));
}

void TestSnapshotHash::testPrepareUnchangedSourceSucceeds()
{
    // 独立数据集：源文件全程未改动，prepare 默认路径应成功
    auto iso = createIsolatedDataset("stableE", QByteArray("stable-img"),
                                     QByteArray("0 0.5 0.5 0.1 0.1\n"));
    QVERIFY(!iso.first.isEmpty());

    QString snapshotId = m_service->createSnapshot(iso.first, 1.0, "sequential");
    QVERIFY(!snapshotId.isEmpty());

    // 源文件未变 → 默认路径应成功
    QString yamlPath = m_service->prepareSnapshotPhysicalDir(snapshotId, false);
    QVERIFY2(!yamlPath.isEmpty(),
             qPrintable("源未变更时 prepare 应成功，lastError=" + m_service->lastError()));
    QVERIFY(QFile::exists(yamlPath));
}

void TestSnapshotHash::testLegacyManifestWithoutHashRejected()
{
    // 手工插入一个纯 ID 数组的旧格式 manifest，应被拒绝
    QString snapshotId = "snap-legacy-nohash";
    QSqlQuery q(Database::instance().database());
    q.prepare("INSERT INTO dataset_snapshots (id, dataset_id, sample_manifest_json, "
              "split_manifest_json, taxonomy_version, annotation_revision_boundary) "
              "VALUES (?, ?, ?, ?, ?, ?)");
    q.addBindValue(snapshotId);
    q.addBindValue(m_datasetId);
    q.addBindValue("[\"sample-hashA\"]");   // 旧格式：纯 ID 字符串
    q.addBindValue("{\"train\":[\"sample-hashA\"],\"val\":[]}");
    q.addBindValue(m_taxonomyId + ":v1");
    q.addBindValue("none");
    QVERIFY(q.exec());

    QString yamlPath = m_service->prepareSnapshotPhysicalDir(snapshotId, false);
    QVERIFY2(yamlPath.isEmpty(), "无哈希的旧格式 manifest 必须被拒绝");
    QCOMPARE(m_service->lastErrorCode(), QString("E_MANIFEST_INVALID"));
}

void TestSnapshotHash::testMissingSourceFileRejected()
{
    auto iso = createIsolatedDataset("missingF", QByteArray("missing-img"),
                                     QByteArray("0 0.5 0.5 0.1 0.1\n"));
    QVERIFY(!iso.first.isEmpty());

    QString snapshotId = m_service->createSnapshot(iso.first, 1.0, "sequential");
    QVERIFY(!snapshotId.isEmpty());

    // 删除源图片 → 文件缺失
    QString imgPath = m_projectRoot + "/images/missingF.jpg";
    QVERIFY(QFile::remove(imgPath));

    QString yamlPath = m_service->prepareSnapshotPhysicalDir(snapshotId, false);
    QVERIFY2(yamlPath.isEmpty(), "源文件缺失时 prepare 必须拒绝");
    QCOMPARE(m_service->lastErrorCode(), QString("E_FILE_MISSING"));
}

QTEST_MAIN(TestSnapshotHash)
#include "test_snapshot_hash.moc"
