#ifndef IMAGEDECODECACHE_H
#define IMAGEDECODECACHE_H

#include <QImage>
#include <QCache>
#include <QString>
#include <QMutex>

/**
 * @brief 画布/推理用解码图像共享缓存（按字节限容）。
 *
 * 标注页前后翻图时同一张 4K 图会被反复 QImage::load，
 * 这里按路径缓存解码结果，命中后零解码开销；总量按字节 LRU 淘汰。
 */
class ImageDecodeCache
{
public:
    static ImageDecodeCache &instance();

    /// 默认 256MB：可容纳约十几张 4K 解码图，翻页/回退零解码
    static constexpr int kDefaultCapacityBytes = 256 * 1024 * 1024;

    void setCapacityBytes(int capacityBytes);

    /**
     * @brief 取解码图像；未命中返回 null QImage。
     * @param path 原始图片绝对路径
     * @return 缓存中的图像副本（QImage 隐式共享，拷贝廉价）
     */
    QImage get(const QString &path);

    /**
     * @brief 解码并回填缓存（缓存命中直接返回，不重复解码）。
     * @param path 原始图片绝对路径
     * @return 解码图像；失败返回 null QImage
     */
    QImage load(const QString &path);

    void put(const QString &path, const QImage &image);
    void remove(const QString &path);
    void clear();

private:
    ImageDecodeCache();

    mutable QMutex m_mutex;
    QCache<QString, QImage> m_cache;
    int m_capacityBytes = kDefaultCapacityBytes;
};

#endif
