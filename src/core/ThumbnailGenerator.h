#pragma once
#include <QObject>
#include <QStringList>
#include <QThreadPool>
#include <QRunnable>
#include <atomic>

class ThumbnailGenerator : public QObject {
    Q_OBJECT
public:
    explicit ThumbnailGenerator(QObject *parent = nullptr);

    Q_INVOKABLE void generate(const QStringList &imagePaths, const QString &cacheDir);

    static QString thumbnailPath(const QString &imagePath, const QString &cacheDir);

signals:
    void progress(int current, int total);
    void finished();

private:
    QThreadPool m_pool;
    std::atomic<int> m_counter;
};
