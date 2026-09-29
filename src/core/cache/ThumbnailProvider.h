#ifndef THUMBNAILPROVIDER_H
#define THUMBNAILPROVIDER_H

#include <QQuickImageProvider>
#include "ThumbnailCache.h"

/**
 * @brief QML 图像提供器：image://thumb/<url编码的绝对路径>。
 *
 * 优先读磁盘缩略图（cache/thumbnails/*.jpg），命中 ThumbnailCache 则免磁盘 IO；
 * 未命中时按 256px 解码后回填缓存。QImageReader::setScaledSize 在解码阶段
 * 降采样，4K 原图不会整张驻留内存。
 */
class ThumbnailProvider : public QQuickImageProvider
{
public:
    explicit ThumbnailProvider(ThumbnailCache *cache);

    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    ThumbnailCache *m_cache = nullptr;
};

#endif
