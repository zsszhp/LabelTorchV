/**
 * @file test_dataset_hash.cpp
 * @brief P0-6 hash 落库测试
 *
 * 期望行为：导入数据集后，dataset_samples.hash 必须非空（图片内容哈希），
 * 供 detectAnomalies 的重复检测与跨 split 数据泄漏检测使用。
 */
#include <QTest>
#include <QCoreApplication>
#include <QSqlQuery>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <QImage>

#include "Database.h"
#include "DatasetService.h"
#include "ImportScanner.h"

namespace {

/// 创建一张真实可解码的 PNG 图片（供内容哈希计算）
bool createPng(const QString &path, const QColor &color)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QImage img(8, 8, QImage::Format_RGB32);
    img.fill(color);
    return img.save(path, "PNG");
}

bool writeTextFile(const QString &path, const QByteArray &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(content);
    f.close();
    return true;
}

} // namespace

class TestDatasetHash : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void testImportWritesSampleHash();
    void testDuplicateImagesShareHash();
    void testDistinctImagesHaveDistinctHash();

private:
    QTemporaryDir m_tmpDir;
    QString m_projectId;
    DatasetService *m_service = nullptr;

    /// 在临时目录构建 YOLO 数据集并导入，返回 datasetId
    QString importYoloDataset(const QString &tag, const QList<QColor> &colors);
    /// 查询 dataset_samples.hash 列表
    QStringList hashesOf(const QString &datasetId);
};

void TestDatasetHash::initTestCase()
{
    QVERIFY2(m_tmpDir.isValid(), "无法创建临时目录");

    QString dbPath = m_tmpDir.path() + "/test_dataset_hash.db";
    QFile::remove(dbPath);
    QFile::remove(dbPath + "-journal");
    QFile::remove(dbPath + "-wal");
    Database::instance().open(dbPath);
    Database::instance().initializeSchema();

    m_projectId = "proj-ds-hash";
    QSqlQuery q(Database::instance().database());
    q.prepare("INSERT INTO projects (id, name, root_path, task_type) VALUES (?, ?, ?, ?)");
    q.addBindValue(m_projectId);
    q.addBindValue("DsHashProject");
    q.addBindValue(m_tmpDir.path() + "/proj");
    q.addBindValue("detect");
    QVERIFY(q.exec());

    q.prepare("INSERT INTO taxonomies (id, project_id, name, version, class_definitions_json) "
              "VALUES (?, ?, ?, ?, ?)");
    q.addBindValue("tax-ds-hash");
    q.addBindValue(m_projectId);
    q.addBindValue("Default");
    q.addBindValue(1);
    q.addBindValue("[\"defect\"]");
    QVERIFY(q.exec());

    m_service = new DatasetService(this);
}

QString TestDatasetHash::importYoloDataset(const QString &tag, const QList<QColor> &colors)
{
    QString imgDir = m_tmpDir.path() + "/data_" + tag + "/images";
    QString lblDir = m_tmpDir.path() + "/data_" + tag + "/labels";
    QDir().mkpath(imgDir);
    QDir().mkpath(lblDir);

    for (int i = 0; i < colors.size(); ++i) {
        QString stem = QString("%1_%2").arg(tag).arg(i, 3, 10, QChar('0'));
        if (!createPng(imgDir + "/" + stem + ".png", colors[i])) return {};
        if (!writeTextFile(lblDir + "/" + stem + ".txt", QByteArray("0 0.5 0.5 0.1 0.1\n")))
            return {};
    }

    return m_service->importDataset(m_projectId, "DsHash_" + tag, imgDir, lblDir);
}

QStringList TestDatasetHash::hashesOf(const QString &datasetId)
{
    QStringList out;
    QSqlQuery q(Database::instance().database());
    q.prepare("SELECT hash FROM dataset_samples WHERE dataset_id = ? ORDER BY image_path");
    q.addBindValue(datasetId);
    if (!q.exec()) return out;
    while (q.next()) out.append(q.value(0).toString());
    return out;
}

// === P0-6 用例 ===

void TestDatasetHash::testImportWritesSampleHash()
{
    QList<QColor> colors = {QColor(255, 0, 0), QColor(0, 255, 0), QColor(0, 0, 255)};
    QString dsId = importYoloDataset("basic", colors);
    QVERIFY2(!dsId.isEmpty(), "导入 YOLO 数据集应成功");

    QStringList hashes = hashesOf(dsId);
    QCOMPARE(hashes.size(), 3);

    // 关键断言：每个样本的 hash 均非空
    for (int i = 0; i < hashes.size(); ++i) {
        QVERIFY2(!hashes[i].isEmpty(),
                 qPrintable(QString("dataset_samples.hash 不得为空（第 %1 个样本）").arg(i)));
    }
}

void TestDatasetHash::testDuplicateImagesShareHash()
{
    // 两张内容完全相同的图片（同色同尺寸）→ hash 应相同
    QList<QColor> colors = {QColor(10, 20, 30), QColor(10, 20, 30)};
    QString dsId = importYoloDataset("dup", colors);
    QVERIFY(!dsId.isEmpty());

    QStringList hashes = hashesOf(dsId);
    QCOMPARE(hashes.size(), 2);
    QVERIFY(!hashes[0].isEmpty());
    QCOMPARE(hashes[0], hashes[1]);
}

void TestDatasetHash::testDistinctImagesHaveDistinctHash()
{
    // 两张内容不同的图片 → hash 应不同
    QList<QColor> colors = {QColor(200, 0, 0), QColor(0, 200, 0)};
    QString dsId = importYoloDataset("distinct", colors);
    QVERIFY(!dsId.isEmpty());

    QStringList hashes = hashesOf(dsId);
    QCOMPARE(hashes.size(), 2);
    QVERIFY(!hashes[0].isEmpty());
    QVERIFY(!hashes[1].isEmpty());
    QVERIFY2(hashes[0] != hashes[1], "不同内容图片的 hash 不应相同");
}

QTEST_MAIN(TestDatasetHash)
#include "test_dataset_hash.moc"
