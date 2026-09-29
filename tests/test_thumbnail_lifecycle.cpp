/**
 * @file test_thumbnail_lifecycle.cpp
 * @brief P0-11 ThumbnailGenerator 生命周期测试
 *
 * 期望行为：
 * 1. ThumbnailGenerator 析构不崩溃（析构应等待线程池任务结束，避免 use-after-free）
 * 2. 反复「创建 → 提交任务 → 立即析构」不崩溃
 * 3. 空列表 / 不存在路径等边界情况下析构同样安全
 */
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QImage>
#include <QEventLoop>
#include <QTimer>

#include "ThumbnailGenerator.h"

namespace {

/// 创建一张真实可解码的小图片
bool createPng(const QString &path, const QColor &color)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QImage img(16, 16, QImage::Format_RGB32);
    img.fill(color);
    return img.save(path, "PNG");
}

} // namespace

class TestThumbnailLifecycle : public QObject
{
    Q_OBJECT

private slots:
    void testConstructDestructNoCrash();
    void testGenerateEmptyListThenDestruct();
    void testGenerateThenImmediateDestruct();
    void testRepeatedCreateDestroyWithPendingWork();
    void testNonexistentPathsThenDestruct();
};

// === P0-11 用例 ===

void TestThumbnailLifecycle::testConstructDestructNoCrash()
{
    // 简单实例化 + 析构，不得崩溃
    for (int i = 0; i < 20; ++i) {
        ThumbnailGenerator *gen = new ThumbnailGenerator();
        delete gen;
    }
    QVERIFY(true);
}

void TestThumbnailLifecycle::testGenerateEmptyListThenDestruct()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    ThumbnailGenerator *gen = new ThumbnailGenerator();
    gen->generate(QStringList(), tmpDir.path() + "/thumbs");
    // 立即析构：空列表应安全
    delete gen;
    QVERIFY(true);
}

void TestThumbnailLifecycle::testGenerateThenImmediateDestruct()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    // 构造若干真实图片
    QStringList paths;
    for (int i = 0; i < 8; ++i) {
        QString p = tmpDir.path() + QString("/img_%1.png").arg(i);
        QVERIFY(createPng(p, QColor(i * 30 % 256, 100, 50)));
        paths.append(p);
    }

    ThumbnailGenerator *gen = new ThumbnailGenerator();
    gen->generate(paths, tmpDir.path() + "/thumbs");
    // 不等待完成，立即析构：析构必须 waitForDone，不得 use-after-free
    delete gen;
    QVERIFY(true);
}

void TestThumbnailLifecycle::testRepeatedCreateDestroyWithPendingWork()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    QStringList paths;
    for (int i = 0; i < 6; ++i) {
        QString p = tmpDir.path() + QString("/rep_%1.png").arg(i);
        QVERIFY(createPng(p, QColor(i * 40 % 256, 20, 200)));
        paths.append(p);
    }

    // 反复「创建 → 提交任务 → 立即析构」，模拟快速切换项目/数据集
    for (int round = 0; round < 30; ++round) {
        ThumbnailGenerator *gen = new ThumbnailGenerator();
        gen->generate(paths, tmpDir.path() + "/thumbs_round");
        delete gen;
    }
    QVERIFY(true);
}

void TestThumbnailLifecycle::testNonexistentPathsThenDestruct()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    QStringList paths;
    for (int i = 0; i < 5; ++i) {
        paths.append(tmpDir.path() + QString("/missing_%1.png").arg(i));
    }

    ThumbnailGenerator *gen = new ThumbnailGenerator();
    gen->generate(paths, tmpDir.path() + "/thumbs_missing");
    delete gen;
    QVERIFY(true);
}

QTEST_MAIN(TestThumbnailLifecycle)
#include "test_thumbnail_lifecycle.moc"
