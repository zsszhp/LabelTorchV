#ifndef THUMBNAILCACHE_H
#define THUMBNAILCACHE_H

#include <QObject>
#include <QString>
#include <QPixmap>
#include <QCache>

class ThumbnailCache : public QObject
{
    Q_OBJECT
public:
    explicit ThumbnailCache(QObject *parent = nullptr);
    QPixmap get(const QString &path, const QSize &size);
    void put(const QString &path, const QSize &size, const QPixmap &pixmap);
    void clear();

private:
    QCache<QString, QPixmap> m_cache;
};

#endif
