#include "ThumbnailProvider.h"
#include "ThumbnailGenerator.h"
#include "utils/Log.h"

#include <QImageReader>
#include <QUrl>
#include <QFileInfo>

ThumbnailProvider::ThumbnailProvider(ThumbnailCache *cache)
    : QQuickImageProvider(QQuickImageProvider::Pixmap)
    , m_cache(cache)
{
}

QPixmap ThumbnailProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    // id 为 URL 编码后的绝对路径（原始图或缩略图均可）
    const QString path = QUrl::fromPercentEncoding(id.toUtf8());
    if (path.isEmpty()) {
        return QPixmap();
    }

    // 期望尺寸：优先使用请求尺寸，否则默认 256，避免 4K 整图解码
    QSize target = requestedSize;
    if (target.width() <= 0 || target.height() <= 0) {
        target = QSize(ThumbnailGenerator::kThumbSize, ThumbnailGenerator::kThumbSize);
    }

    // 1) 内存缓存命中
    if (m_cache) {
        QPixmap cached = m_cache->get(path, target);
        if (!cached.isNull()) {
            if (size) *size = cached.size();
            return cached;
        }
    }

    // 2) 解码阶段降采样：QImageReader::setScaledSize 让 JPEG/PNG 解码器直接输出小图
    QImageReader reader(path);
    reader.setAutoTransform(true);
    if (target.width() > 0 && target.height() > 0) {
        reader.setScaledSize(target);
    }
    QImage img = reader.read();
    if (img.isNull()) {
        ltWarning(LT_LOG_CORE()) << "ThumbnailProvider failed to read:" << path
                                 << reader.errorString();
        return QPixmap();
    }

    QPixmap pixmap = QPixmap::fromImage(img);
    if (size) *size = pixmap.size();

    // 3) 回填内存缓存（按字节限容）
    if (m_cache) {
        m_cache->put(path, target, pixmap);
    }
    return pixmap;
}
