#include "ImageDecodeCache.h"
#include "utils/Log.h"

#include <QImageReader>

ImageDecodeCache &ImageDecodeCache::instance()
{
    static ImageDecodeCache cache;
    return cache;
}

ImageDecodeCache::ImageDecodeCache()
    : m_cache(kDefaultCapacityBytes)
{
}

void ImageDecodeCache::setCapacityBytes(int capacityBytes)
{
    QMutexLocker locker(&m_mutex);
    const int cap = capacityBytes > 0 ? capacityBytes : kDefaultCapacityBytes;
    m_capacityBytes = cap;
    m_cache.setMaxCost(cap);
    ltInfo(LT_LOG_CORE()) << "ImageDecodeCache capacityBytes=" << cap;
}

QImage ImageDecodeCache::get(const QString &path)
{
    QMutexLocker locker(&m_mutex);
    QImage *img = m_cache.object(path);
    return img ? *img : QImage();
}

QImage ImageDecodeCache::load(const QString &path)
{
    if (path.isEmpty()) return {};

    // 1) 缓存命中
    {
        QMutexLocker locker(&m_mutex);
        QImage *img = m_cache.object(path);
        if (img) {
            ltTrace(LT_LOG_CORE()) << "ImageDecodeCache hit:" << path;
            return *img;
        }
    }

    // 2) 解码（带 EXIF 方向纠正）
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QImage decoded = reader.read();
    if (decoded.isNull()) {
        ltWarning(LT_LOG_CORE()) << "ImageDecodeCache failed to decode:" << path
                                 << reader.errorString();
        return {};
    }

    put(path, decoded);
    return decoded;
}

void ImageDecodeCache::put(const QString &path, const QImage &image)
{
    if (path.isEmpty() || image.isNull()) return;

    const int cost = static_cast<int>(image.sizeInBytes());
    if (cost <= 0 || cost > m_capacityBytes) {
        // 单张超过总量时不缓存，避免挤空其它条目
        return;
    }

    QMutexLocker locker(&m_mutex);
    m_cache.insert(path, new QImage(image), cost);
    ltTrace(LT_LOG_CORE()) << "ImageDecodeCache put:" << path << "bytes=" << cost;
}

void ImageDecodeCache::remove(const QString &path)
{
    QMutexLocker locker(&m_mutex);
    m_cache.remove(path);
}

void ImageDecodeCache::clear()
{
    QMutexLocker locker(&m_mutex);
    m_cache.clear();
    ltDebug(LT_LOG_CORE()) << "ImageDecodeCache cleared";
}
