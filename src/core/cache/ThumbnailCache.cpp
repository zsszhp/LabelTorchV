#include "ThumbnailCache.h"

ThumbnailCache::ThumbnailCache(QObject *parent) : QObject(parent), m_cache(200) {}

QPixmap ThumbnailCache::get(const QString &path, const QSize &size)
{
    QString key = path + QString("_%1x%2").arg(size.width()).arg(size.height());
    if (m_cache.contains(key)) return *m_cache.object(key);
    return {};
}

void ThumbnailCache::put(const QString &path, const QSize &size, const QPixmap &pixmap)
{
    QString key = path + QString("_%1x%2").arg(size.width()).arg(size.height());
    m_cache.insert(key, new QPixmap(pixmap));
}

void ThumbnailCache::clear() { m_cache.clear(); }
