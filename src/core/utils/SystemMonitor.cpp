#include "SystemMonitor.h"
#include "Log.h"
#include "cache/ThumbnailCache.h"

#include <QDateTime>
#include <QFile>
#include <QStandardPaths>
#include <QSysInfo>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif

namespace {
// 阈值告警线与恢复线（滞后区间，避免临界值来回刷告警）
constexpr double kMemAlertPct = 85.0;
constexpr double kMemRecoverPct = 80.0;
constexpr double kDiskAlertGb = 5.0;
constexpr double kDiskRecoverGb = 6.0;
} // namespace

SystemMonitor::SystemMonitor(QObject *parent)
    : QObject(parent)
{
}

SystemMonitor &SystemMonitor::instance()
{
    static SystemMonitor s_instance;
    return s_instance;
}

void SystemMonitor::setDbPath(const QString &path)
{
    m_dbPath = path;
}

void SystemMonitor::setThumbnailCache(ThumbnailCache *cache)
{
    m_thumbCache = cache;
}

void SystemMonitor::start(int intervalMs)
{
    logStartupSnapshot();

    if (!m_timer) {
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, [this]() { sample(QStringLiteral("periodic")); });
    }
    m_timer->start(intervalMs);

    sample(QStringLiteral("startup"));
}

void SystemMonitor::sampleNow(const QString &reason)
{
    sample(reason);
}

void SystemMonitor::logStartupSnapshot()
{
#ifdef Q_OS_WIN
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);

    const double totalMemGb = static_cast<double>(mem.ullTotalPhys) / (1024.0 * 1024.0 * 1024.0);

    // 磁盘剩余（AppData 所在盘，与 sample() 同一口径）
    double diskFreeGb = 0;
    const std::wstring appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdWString();
    ULARGE_INTEGER freeBytes;
    if (GetDiskFreeSpaceExW(appData.c_str(), &freeBytes, nullptr, nullptr)) {
        diskFreeGb = static_cast<double>(freeBytes.QuadPart) / (1024.0 * 1024.0 * 1024.0);
    }

    const QString line = QStringLiteral("sys_env cores=%1 total_mem_gb=%2 disk_free_gb=%3 os=%4")
                             .arg(si.dwNumberOfProcessors)
                             .arg(totalMemGb, 0, 'f', 1)
                             .arg(diskFreeGb, 0, 'f', 1)
                             .arg(QSysInfo::prettyProductName());
    qCInfo(LT_LOG_SYS()).noquote() << line;
#else
    qCInfo(LT_LOG_SYS()).noquote()
        << QStringLiteral("sys_env os=%1").arg(QSysInfo::prettyProductName());
#endif
}

void SystemMonitor::sample(const QString &reason)
{
#ifdef Q_OS_WIN
    // 进程内存 + 系统内存
    PROCESS_MEMORY_COUNTERS_EX pmc;
    ZeroMemory(&pmc, sizeof(pmc));
    pmc.cb = sizeof(pmc);
    double procMemMb = 0;
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&pmc), sizeof(pmc))) {
        procMemMb = static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
    }

    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);
    const double sysMemUsedPct = 100.0 - static_cast<double>(mem.ullAvailPhys) * 100.0
                                       / static_cast<double>(mem.ullTotalPhys);
    const double totalMemGb = static_cast<double>(mem.ullTotalPhys) / (1024.0 * 1024.0 * 1024.0);

    // 进程 CPU%：GetProcessTimes 差值 / (墙钟差值 x 核数)
    FILETIME ftCreate, ftExit, ftKernel, ftUser;
    quint64 procCpuMs = m_prevProcCpuMs;
    if (GetProcessTimes(GetCurrentProcess(), &ftCreate, &ftExit, &ftKernel, &ftUser)) {
        const quint64 kernelMs = (static_cast<quint64>(ftKernel.dwHighDateTime) << 32
                                  | ftKernel.dwLowDateTime) / 10000; // 100ns → ms
        const quint64 userMs = (static_cast<quint64>(ftUser.dwHighDateTime) << 32
                                | ftUser.dwLowDateTime) / 10000;
        procCpuMs = kernelMs + userMs;
    }
    const quint64 wallMs = static_cast<quint64>(QDateTime::currentMSecsSinceEpoch());

    // 系统 CPU%：GetSystemTimes 差值（kernel 时间含 idle）
    FILETIME ftIdle, ftSysK, ftSysU;
    quint64 idleMs = m_prevIdleMs, sysMs = m_prevSysMs;
    if (GetSystemTimes(&ftIdle, &ftSysK, &ftSysU)) {
        idleMs = (static_cast<quint64>(ftIdle.dwHighDateTime) << 32 | ftIdle.dwLowDateTime) / 10000;
        sysMs = (static_cast<quint64>(ftSysK.dwHighDateTime) << 32 | ftSysK.dwLowDateTime) / 10
                + (static_cast<quint64>(ftSysU.dwHighDateTime) << 32 | ftSysU.dwLowDateTime) / 10000;
    }

    double procCpuPct = 0, sysCpuPct = 0;
    if (m_hasPrev) {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        const quint64 wallDelta = wallMs > m_prevWallMs ? wallMs - m_prevWallMs : 0;
        if (wallDelta > 0) {
            procCpuPct = 100.0 * static_cast<double>(procCpuMs - m_prevProcCpuMs)
                         / (static_cast<double>(wallDelta) * si.dwNumberOfProcessors);
            const quint64 sysDelta = sysMs > m_prevSysMs ? sysMs - m_prevSysMs : 0;
            const quint64 idleDelta = idleMs > m_prevIdleMs ? idleMs - m_prevIdleMs : 0;
            if (sysDelta > 0)
                sysCpuPct = 100.0 * static_cast<double>(sysDelta - idleDelta) / sysDelta;
        }
    }
    m_prevProcCpuMs = procCpuMs;
    m_prevWallMs = wallMs;
    m_prevIdleMs = idleMs;
    m_prevSysMs = sysMs;
    m_hasPrev = true;

    // 磁盘剩余（AppData 所在盘）
    double diskFreeGb = 0;
    const std::wstring root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdWString();
    ULARGE_INTEGER freeBytes;
    if (GetDiskFreeSpaceExW(root.c_str(), &freeBytes, nullptr, nullptr)) {
        diskFreeGb = static_cast<double>(freeBytes.QuadPart) / (1024.0 * 1024.0 * 1024.0);
    }

    // DB 大小
    double dbSizeMb = 0;
    if (!m_dbPath.isEmpty()) {
        const qint64 dbBytes = QFile(m_dbPath).size();
        dbSizeMb = static_cast<double>(dbBytes) / (1024.0 * 1024.0);
    }

    // 缩略图缓存
    double thumbMb = 0;
    if (m_thumbCache) {
        thumbMb = static_cast<double>(m_thumbCache->usedBytes()) / (1024.0 * 1024.0);
    }

    checkThresholds(sysMemUsedPct, diskFreeGb);

    QString line = QStringLiteral(
                       "sys_sample reason=%1 proc_mem_mb=%2 proc_cpu_pct=%3 sys_cpu_pct=%4 "
                       "sys_mem_used_pct=%5 total_mem_gb=%6 disk_free_gb=%7 db_size_mb=%8 thumb_mb=%9")
                       .arg(reason,
                            QString::number(procMemMb, 'f', 1),
                            // 多线程进程 CPU% 可远超 100%（按核数归一上限），仅截断异常值
                            QString::number(qMin(procCpuPct, 10000.0), 'f', 1),
                            QString::number(qMin(sysCpuPct, 100.0), 'f', 1),
                            QString::number(sysMemUsedPct, 'f', 1),
                            QString::number(totalMemGb, 'f', 1),
                            QString::number(diskFreeGb, 'f', 1),
                            QString::number(dbSizeMb, 'f', 1),
                            QString::number(thumbMb, 'f', 1));
    qCInfo(LT_LOG_SYS()).noquote() << line;
#else
    Q_UNUSED(reason);
#endif
}

void SystemMonitor::checkThresholds(double sysMemUsedPct, double diskFreeGb)
{
    // 内存：>85% 告警，回落到 <80% 才重置
    if (!m_memAlerted && sysMemUsedPct > kMemAlertPct) {
        m_memAlerted = true;
        qCWarning(LT_LOG_SYS()) << "sys_alert memory usage high:" << sysMemUsedPct << "%";
    } else if (m_memAlerted && sysMemUsedPct < kMemRecoverPct) {
        m_memAlerted = false;
        qCInfo(LT_LOG_SYS()) << "sys_alert memory usage recovered:" << sysMemUsedPct << "%";
    }

    // 磁盘：<5GB 告警，回升到 >6GB 才重置
    if (!m_diskAlerted && diskFreeGb > 0 && diskFreeGb < kDiskAlertGb) {
        m_diskAlerted = true;
        qCWarning(LT_LOG_SYS()) << "sys_alert disk free low:" << diskFreeGb << "GB";
    } else if (m_diskAlerted && diskFreeGb > kDiskRecoverGb) {
        m_diskAlerted = false;
        qCInfo(LT_LOG_SYS()) << "sys_alert disk free recovered:" << diskFreeGb << "GB";
    }
}
