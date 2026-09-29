#include "ThumbnailGenerator.h"
#include <QImage>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QThread>
#include <QMetaObject>
#include <QCoreApplication>
#include <atomic>

/**
 * @brief 单张缩略图生成任务。
 * 任务在线程池工作线程执行，完成时通过 QueuedConnection 回到主线程发信号，
 * 并用 QPointer + 批次代号双重校验，防止生成器销毁或批次重入后误发信号。
 */
class ThumbnailTask : public QRunnable {
public:
    ThumbnailTask(const QString &imagePath, const QString &cacheDir,
                  std::atomic<int> &counter, int total,
                  QPointer<ThumbnailGenerator> gen, int generation)
        : m_imagePath(imagePath), m_cacheDir(cacheDir),
          m_counter(counter), m_total(total),
          m_gen(gen), m_generation(generation)
    {
        setAutoDelete(true);
    }

    void run() override
    {
        QString thumbPath = ThumbnailGenerator::thumbnailPath(m_imagePath, m_cacheDir);
        if (!QFileInfo::exists(thumbPath)) {
            QImage img(m_imagePath);
            if (!img.isNull()) {
                // 统一压到 256px 长边，JPEG 质量 85 在清晰度/体积间取平衡
                QImage scaled = img.scaled(ThumbnailGenerator::kThumbSize,
                                           ThumbnailGenerator::kThumbSize,
                                           Qt::KeepAspectRatio, Qt::SmoothTransformation);
                QDir().mkpath(QFileInfo(thumbPath).absolutePath());
                scaled.save(thumbPath, "JPEG", 85);
            }
        }

        int done = m_counter.fetch_add(1) + 1;

        // 回主线程发信号：工作线程不能直接触碰 QObject 信号/接收方
        QPointer<ThumbnailGenerator> gen = m_gen;
        const int generation = m_generation;
        QMetaObject::invokeMethod(QCoreApplication::instance(), [gen, generation, done, total = m_total]() {
            // 生成器已销毁或批次已被新 generate() 取代时丢弃本次回调
            if (!gen || !gen->isCurrentGeneration(generation)) return;
            emit gen->progress(done, total);
            if (done >= total) {
                emit gen->finished();
            }
        }, Qt::QueuedConnection);
    }

private:
    QString m_imagePath;
    QString m_cacheDir;
    std::atomic<int> &m_counter;
    int m_total;
    QPointer<ThumbnailGenerator> m_gen;
    int m_generation;
};

ThumbnailGenerator::ThumbnailGenerator(QObject *parent)
    : QObject(parent)
    , m_counter(0)
    , m_generation(0)
{
    // 缩略图生成为 IO + 轻量编码，4 线程足够吃满磁盘而不抢占训练/推理
    m_pool.setMaxThreadCount(qMin(4, QThread::idealThreadCount()));
}

ThumbnailGenerator::~ThumbnailGenerator()
{
    // 先禁止新任务进入，再等待在跑任务结束，确保任务不会再访问本对象
    m_pool.clear();
    m_pool.waitForDone();
}

void ThumbnailGenerator::generate(const QStringList &imagePaths, const QString &cacheDir)
{
    // 重入保护：取消尚未开始的旧任务并等待在跑任务结束，
    // 否则 m_counter.store(0) 会与旧批次的 fetch_add 竞态，进度/完成信号错乱
    m_generation.fetch_add(1);
    m_pool.clear();
    m_pool.waitForDone();

    if (imagePaths.isEmpty()) {
        emit finished();
        return;
    }

    m_counter.store(0);
    const int generation = m_generation.load();
    int total = imagePaths.size();

    for (int i = 0; i < total; ++i) {
        auto *task = new ThumbnailTask(imagePaths[i], cacheDir, m_counter, total, this, generation);
        m_pool.start(task);
    }
}

QString ThumbnailGenerator::pathFor(const QString &imagePath, const QString &cacheDir) const
{
    return thumbnailPath(imagePath, cacheDir);
}

QString ThumbnailGenerator::resolve(const QString &imagePath, const QString &cacheDir) const
{
    const QString thumb = thumbnailPath(imagePath, cacheDir);
    return QFileInfo::exists(thumb) ? thumb : QString();
}

QString ThumbnailGenerator::thumbnailPath(const QString &imagePath, const QString &cacheDir)
{
    QByteArray hash = QCryptographicHash::hash(imagePath.toUtf8(), QCryptographicHash::Md5).toHex();
    return cacheDir + QStringLiteral("/") + QString::fromUtf8(hash) + QStringLiteral(".jpg");
}
