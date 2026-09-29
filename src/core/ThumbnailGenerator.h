#pragma once
#include <QObject>
#include <QStringList>
#include <QThreadPool>
#include <QRunnable>
#include <QPointer>
#include <atomic>

/**
 * @brief 后台缩略图生成器（线程池批量生成 JPEG 缩略图到磁盘缓存）。
 *
 * 工业数据集万张级 4K 图直接进网格会把内存打爆，统一在导入后/浏览时
 * 后台压成 256px JPEG，网格只读缩略图文件。
 */
class ThumbnailGenerator : public QObject {
    Q_OBJECT
public:
    /// 缩略图长边像素（256，兼顾清晰度与内存）
    static constexpr int kThumbSize = 256;

    explicit ThumbnailGenerator(QObject *parent = nullptr);
    /// 析构时等待线程池全部任务结束，避免任务持有悬空指针导致 use-after-free
    ~ThumbnailGenerator() override;

    /**
     * @brief 批量生成缩略图。
     * 重入时先取消/等待上一批任务，防止 m_counter 被中途重置导致进度信号错乱。
     * 已存在的缩略图会跳过重新编码（任务内部判断）。
     * @param imagePaths 待生成缩略图的原始图片路径列表
     * @param cacheDir   缩略图缓存目录
     */
    Q_INVOKABLE void generate(const QStringList &imagePaths, const QString &cacheDir);

    /// 计算缩略图目标路径（不检查是否存在）
    Q_INVOKABLE QString pathFor(const QString &imagePath, const QString &cacheDir) const;

    /**
     * @brief 解析可用缩略图路径。
     * @return 缩略图已存在则返回其路径，否则返回空串（调用方回退原图 + sourceSize）。
     */
    Q_INVOKABLE QString resolve(const QString &imagePath, const QString &cacheDir) const;

    static QString thumbnailPath(const QString &imagePath, const QString &cacheDir);

    /// 判断任务所属批次是否仍是当前批次（供工作线程回调在主线程校验）
    bool isCurrentGeneration(int generation) const { return m_generation.load() == generation; }

signals:
    void progress(int current, int total);
    void finished();

private:
    QThreadPool m_pool;
    std::atomic<int> m_counter;
    /// 批次代号：generate() 重入时递增，旧批次任务回调因代号不匹配被丢弃
    std::atomic<int> m_generation;
};
