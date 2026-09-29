#ifndef THUMBNAILCACHE_H
#define THUMBNAILCACHE_H

#include <QObject>
#include <QString>
#include <QPixmap>
#include <QCache>

/**
 * @brief 缩略图内存缓存（按字节限容）。
 *
 * 工业场景下单张 4K 缩略图约 256KB，按条数限容无法控制真实内存占用，
 * 因此以 pixmap 字节占用作为 cost，总量超过阈值后按 LRU 淘汰。
 */
class ThumbnailCache : public QObject
{
    Q_OBJECT
public:
    /// 默认容量 128MB，可覆盖万张级网格滚动时的解码结果
    static constexpr int kDefaultCapacityBytes = 128 * 1024 * 1024;

    explicit ThumbnailCache(QObject *parent = nullptr);

    /// @param capacityBytes 缓存字节上限（小于 1 时回退默认值）
    void setCapacityBytes(int capacityBytes);
    int capacityBytes() const { return m_capacityBytes; }
    int usedBytes() const { return m_usedBytes; }

    QPixmap get(const QString &path, const QSize &size);
    void put(const QString &path, const QSize &size, const QPixmap &pixmap);
    void clear();

private:
    static QString makeKey(const QString &path, const QSize &size);
    /// QCache 淘汰回调中同步扣减已用字节
    void onEvicted(const QString &key);

    QCache<QString, QPixmap> m_cache;
    int m_capacityBytes = kDefaultCapacityBytes;
    int m_usedBytes = 0;
};

#endif
