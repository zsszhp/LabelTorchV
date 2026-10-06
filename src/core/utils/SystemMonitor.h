#ifndef SYSTEMMONITOR_H
#define SYSTEMMONITOR_H

#include <QObject>
#include <QString>
#include <QTimer>

class ThumbnailCache;

// 系统占用监控（lt.sys 类别）：
// - 启动环境快照（CPU 核数/内存总量/OS/磁盘剩余）
// - 周期采样（默认 30s）：进程内存、进程 CPU%、系统内存占用、磁盘剩余、DB 大小、缩略图缓存
// - 事件采样：训练/推理/导出等重操作启动时由服务层调 sampleNow(reason)
// - 阈值告警：系统内存占用 >85%、磁盘剩余 <5GB 时记 WARNING，恢复后重置（带滞后区间）
class SystemMonitor : public QObject
{
    Q_OBJECT
public:
    static SystemMonitor &instance();

    // 启动周期采样（main 线程调用）；先打一条启动环境快照
    void start(int intervalMs = 30000);

    // 事件采样：reason 标明触发源（如 "training_start"）
    void sampleNow(const QString &reason);

    void setDbPath(const QString &path);
    void setThumbnailCache(ThumbnailCache *cache);

private:
    explicit SystemMonitor(QObject *parent = nullptr);

    void sample(const QString &reason);
    void logStartupSnapshot();
    void checkThresholds(double sysMemUsedPct, double diskFreeGb);

    QTimer *m_timer = nullptr;
    QString m_dbPath;
    ThumbnailCache *m_thumbCache = nullptr;

    // 进程 CPU% 计算用的上次采样值
    quint64 m_prevProcCpuMs = 0;
    quint64 m_prevWallMs = 0;
    // 系统 CPU%（GetSystemTimes）
    quint64 m_prevIdleMs = 0;
    quint64 m_prevSysMs = 0;
    bool m_hasPrev = false;

    // 阈值告警状态（true=已告警未恢复）
    bool m_memAlerted = false;
    bool m_diskAlerted = false;
};

#endif // SYSTEMMONITOR_H
