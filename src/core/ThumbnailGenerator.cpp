#include "ThumbnailGenerator.h"
#include <QImage>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QThread>
#include <atomic>

class ThumbnailTask : public QRunnable {
public:
    ThumbnailTask(const QString &imagePath, const QString &cacheDir,
                  std::atomic<int> &counter, int total, ThumbnailGenerator *gen)
        : m_imagePath(imagePath), m_cacheDir(cacheDir),
          m_counter(counter), m_total(total), m_gen(gen)
    {
        setAutoDelete(true);
    }

    void run() override
    {
        QString thumbPath = ThumbnailGenerator::thumbnailPath(m_imagePath, m_cacheDir);
        if (!QFileInfo::exists(thumbPath)) {
            QImage img(m_imagePath);
            if (!img.isNull()) {
                QImage scaled = img.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                QDir().mkpath(QFileInfo(thumbPath).absolutePath());
                scaled.save(thumbPath, "JPEG", 85);
            }
        }

        int done = m_counter.fetch_add(1) + 1;
        emit m_gen->progress(done, m_total);
        if (done >= m_total) {
            emit m_gen->finished();
        }
    }

private:
    QString m_imagePath;
    QString m_cacheDir;
    std::atomic<int> &m_counter;
    int m_total;
    ThumbnailGenerator *m_gen;
};

ThumbnailGenerator::ThumbnailGenerator(QObject *parent)
    : QObject(parent)
    , m_counter(0)
{
    m_pool.setMaxThreadCount(qMin(4, QThread::idealThreadCount()));
}

void ThumbnailGenerator::generate(const QStringList &imagePaths, const QString &cacheDir)
{
    if (imagePaths.isEmpty()) {
        emit finished();
        return;
    }

    m_counter.store(0);
    int total = imagePaths.size();

    for (int i = 0; i < total; ++i) {
        auto *task = new ThumbnailTask(imagePaths[i], cacheDir, m_counter, total, this);
        m_pool.start(task);
    }
}

QString ThumbnailGenerator::thumbnailPath(const QString &imagePath, const QString &cacheDir)
{
    QByteArray hash = QCryptographicHash::hash(imagePath.toUtf8(), QCryptographicHash::Md5).toHex();
    return cacheDir + QStringLiteral("/") + QString::fromUtf8(hash) + QStringLiteral(".jpg");
}
