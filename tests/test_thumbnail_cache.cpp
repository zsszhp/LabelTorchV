/**
 * @file test_thumbnail_cache.cpp
 * @brief P1-21 ThumbnailCache 字节限容测试
 *
 * 验证缓存按 pixmap 字节占用淘汰，超容量后旧条目被 LRU 丢弃。
 */
#include <QtTest>
#include <QPixmap>
#include <QImage>

#include "cache/ThumbnailCache.h"

class TestThumbnailCache : public QObject
{
    Q_OBJECT

private slots:
    /// 默认容量应为 128MB
    void test_default_capacity()
    {
        ThumbnailCache cache;
        QCOMPARE(cache.capacityBytes(), ThumbnailCache::kDefaultCapacityBytes);
    }

    /// put/get 基本往返
    void test_put_get()
    {
        ThumbnailCache cache;
        QPixmap px(64, 64);
        px.fill(Qt::red);
        cache.put(QStringLiteral("/tmp/a.jpg"), QSize(64, 64), px);
        QPixmap got = cache.get(QStringLiteral("/tmp/a.jpg"), QSize(64, 64));
        QVERIFY(!got.isNull());
        QCOMPARE(got.size(), QSize(64, 64));
    }

    /// 不同 size 是不同 key
    void test_key_by_size()
    {
        ThumbnailCache cache;
        QPixmap px(64, 64);
        px.fill(Qt::blue);
        cache.put(QStringLiteral("/tmp/a.jpg"), QSize(64, 64), px);
        QVERIFY(cache.get(QStringLiteral("/tmp/a.jpg"), QSize(128, 128)).isNull());
        QVERIFY(!cache.get(QStringLiteral("/tmp/a.jpg"), QSize(64, 64)).isNull());
    }

    /// 容量按字节：塞满后旧条目被淘汰
    void test_byte_limited_eviction()
    {
        ThumbnailCache cache;
        // 设 200KB 上限：一张 64x64 ARGB32 = 16KB，可放约 12 张
        cache.setCapacityBytes(200 * 1024);

        const QString pathPrefix = QStringLiteral("/tmp/evict_");
        for (int i = 0; i < 20; ++i) {
            QPixmap px(64, 64);
            px.fill(Qt::green);
            cache.put(pathPrefix + QString::number(i), QSize(64, 64), px);
        }

        // 最早插入的应被淘汰，最近插入的应还在
        QVERIFY(cache.get(pathPrefix + QStringLiteral("0"), QSize(64, 64)).isNull());
        QVERIFY(cache.get(pathPrefix + QStringLiteral("1"), QSize(64, 64)).isNull());
        QVERIFY(!cache.get(pathPrefix + QStringLiteral("19"), QSize(64, 64)).isNull());
    }

    /// 单张超过总容量的不入缓存
    void test_oversized_rejected()
    {
        ThumbnailCache cache;
        cache.setCapacityBytes(4 * 1024); // 4KB
        QPixmap px(64, 64);              // 16KB
        px.fill(Qt::yellow);
        cache.put(QStringLiteral("/tmp/big.jpg"), QSize(64, 64), px);
        QVERIFY(cache.get(QStringLiteral("/tmp/big.jpg"), QSize(64, 64)).isNull());
    }

    /// clear 清空全部
    void test_clear()
    {
        ThumbnailCache cache;
        QPixmap px(32, 32);
        px.fill(Qt::white);
        cache.put(QStringLiteral("/tmp/c.jpg"), QSize(32, 32), px);
        cache.clear();
        QVERIFY(cache.get(QStringLiteral("/tmp/c.jpg"), QSize(32, 32)).isNull());
        QCOMPARE(cache.usedBytes(), 0);
    }
};

QTEST_MAIN(TestThumbnailCache)
#include "test_thumbnail_cache.moc"
