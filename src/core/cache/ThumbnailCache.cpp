#include "ThumbnailCache.h"
#include "utils/Log.h"

ThumbnailCache::ThumbnailCache(QObject *parent) : QObject(parent), m_cache(200)
{
    ltTrace(LT_LOG_CORE()) << "ThumbnailCache constructed capacity=200";
}

QPixmap ThumbnailCache::get(const QString &path, const QSize &size)
{
    QString key = path + QString("_%1x%2").arg(size.width()).arg(size.height());
    if (m_cache.contains(key)) {
        ltTrace(LT_LOG_CORE()) << "Cache hit:" << key;
        return *m_cache.object(key);
    }
    ltTrace(LT_LOG_CORE()) << "Cache miss:" << key;
    return {};
}

void ThumbnailCache::put(const QString &path, const QSize &size, const QPixmap &pixmap)
{
    QString key = path + QString("_%1x%2").arg(size.width()).arg(size.height());
    m_cache.insert(key, new QPixmap(pixmap));
    ltTrace(LT_LOG_CORE()) << "Cache put:" << key;
}

void ThumbnailCache::clear()
{
    ltDebug(LT_LOG_CORE()) << "Cache cleared";
    m_cache.clear();
}
