#include "ThumbnailCache.h"
#include "utils/Log.h"

ThumbnailCache::ThumbnailCache(QObject *parent) : QObject(parent), m_cache(kDefaultCapacityBytes)
{
    m_usedBytes = 0;
    ltTrace(LT_LOG_CORE()) << "ThumbnailCache constructed capacityBytes=" << kDefaultCapacityBytes;
}

void ThumbnailCache::setCapacityBytes(int capacityBytes)
{
    const int cap = capacityBytes > 0 ? capacityBytes : kDefaultCapacityBytes;
    if (m_capacityBytes == cap) return;
    m_capacityBytes = cap;
    m_cache.setMaxCost(cap);
    ltInfo(LT_LOG_CORE()) << "ThumbnailCache capacityBytes=" << cap;
}

QString ThumbnailCache::makeKey(const QString &path, const QSize &size)
{
    return path + QStringLiteral("_%1x%2").arg(size.width()).arg(size.height());
}

void ThumbnailCache::onEvicted(const QString &key)
{
    Q_UNUSED(key);
    // QCache 淘汰时 cost 由 insert 时记录，这里仅维护 usedBytes 估算值，
    // 实际内存以 costLimit 由 QCache 强制约束
}

QPixmap ThumbnailCache::get(const QString &path, const QSize &size)
{
    const QString key = makeKey(path, size);
    if (m_cache.contains(key)) {
        ltTrace(LT_LOG_CORE()) << "Cache hit:" << key;
        return *m_cache.object(key);
    }
    ltTrace(LT_LOG_CORE()) << "Cache miss:" << key;
    return {};
}

void ThumbnailCache::put(const QString &path, const QSize &size, const QPixmap &pixmap)
{
    if (pixmap.isNull()) return;

    const QString key = makeKey(path, size);
    // cost 以字节计：QPixmap 无 sizeInBytes()，按 ARGB32 估算
    const int cost = pixmap.width() * pixmap.height() * 4;
    if (cost <= 0) return;

    // 单张超过总量时直接拒绝，避免污染缓存
    if (cost > m_capacityBytes) {
        ltDebug(LT_LOG_CORE()) << "Thumbnail too large for cache:" << key << "bytes=" << cost;
        return;
    }

    if (m_cache.contains(key)) {
        const QPixmap *old = m_cache.object(key);
        if (old) m_usedBytes -= old->width() * old->height() * 4;
    }
    m_cache.insert(key, new QPixmap(pixmap), cost);
    m_usedBytes += cost;
    ltTrace(LT_LOG_CORE()) << "Cache put:" << key << "bytes=" << cost
                           << "used=" << m_usedBytes << "/" << m_capacityBytes;
}

void ThumbnailCache::clear()
{
    ltDebug(LT_LOG_CORE()) << "Cache cleared, usedBytes was" << m_usedBytes;
    m_cache.clear();
    m_usedBytes = 0;
}
