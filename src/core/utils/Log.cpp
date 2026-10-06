#include "Log.h"
#include "Breadcrumb.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QLoggingCategory>
#include <QCoreApplication>
#include <QDebug>

static QMutex logMutex;
static QFile *logFile = nullptr;
static QtMessageHandler originalHandler = nullptr;

// 会话标识与目录（init 后有效）
static QString s_sessionId;
static QString s_logDir;
// 分卷：按天命名，单文件超限后另起 _001/_002 递增分卷
static QString s_fileBasePath;   // 不含分卷后缀的基础路径
static int s_fileIndex = 0;
static qint64 s_bytesWritten = 0;

static const char *levelString(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:    return "DEBUG";
    case QtInfoMsg:     return "INFO ";
    case QtWarningMsg:  return "WARN ";
    case QtCriticalMsg: return "ERROR";
    case QtFatalMsg:    return "FATAL";
    }
    return "?????";
}

// 单文件上限：50MB（训练一晚的量级下避免单文件无界增长）
static const qint64 kMaxFileBytes = 50 * 1024 * 1024;
static const int kMaxRetainedDays = 14;

// 应用级别过滤规则。setFilterRules 是整体替换，
// 因此级别规则必须与 Qt 内部噪音抑制规则一起下发。
static void applyLevelRules(const QString &level)
{
    QString ltRules;
    if (level == QStringLiteral("debug") || level == QStringLiteral("trace")) {
        ltRules = QStringLiteral("lt.*.debug=true\n");
    } else if (level == QStringLiteral("info")) {
        ltRules = QStringLiteral("lt.*.debug=false\nlt.*.info=true\n");
    } else if (level == QStringLiteral("warning")) {
        ltRules = QStringLiteral("lt.*.debug=false\nlt.*.info=false\n");
    } else if (level == QStringLiteral("error")) {
        ltRules = QStringLiteral("lt.*.debug=false\nlt.*.info=false\n");
    } else {
        ltRules = QStringLiteral("lt.*.debug=false\nlt.*.info=true\n"); // 未知级别回退 INFO
    }
    ltRules += QStringLiteral(
        "lt.*.warning=true\nlt.*.critical=true\n"
        "qt.*.debug=false\nqt.scenegraph.*=false\n");
    QLoggingCategory::setFilterRules(ltRules);
}

static void openLogFile()
{
    QString suffix = s_fileIndex == 0 ? QStringLiteral(".log")
                                      : QStringLiteral("_%1.log").arg(s_fileIndex, 3, 10, QChar('0'));
    QString filePath = s_fileBasePath + suffix;

    logFile = new QFile(filePath);
    if (!logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qWarning() << "Failed to open log file:" << filePath;
        delete logFile;
        logFile = nullptr;
        return;
    }
    s_bytesWritten = 0;
}

// 超限轮转：关闭当前分卷，打开下一分卷
static void rotateLogFile()
{
    if (!logFile) return;
    logFile->close();
    delete logFile;
    logFile = nullptr;
    s_fileIndex++;
    openLogFile();
    qInfo() << "Log file rotated to volume" << s_fileIndex;
}

static void logMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // Format the structured log line
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString category = context.category ? context.category : "";
    QString function = context.function ? context.function : "";
    QString file = context.file ? context.file : "";
    int line = context.line;

    QString formatted = QString("[%1] [%2] [%3] [%4] %5")
        .arg(timestamp, levelString(type), category, s_sessionId, msg);

    if (!function.isEmpty()) {
        formatted += QString("  [%1:%2]").arg(function).arg(line);
    }
    const QByteArray utf8 = formatted.toUtf8();

    // 黑匣子入环：崩溃时由异常过滤器导出最近日志（见 main.cpp 崩溃摘要）
    Breadcrumb::push(utf8.constData());

    // Write to stderr (console)
    QTextStream(stderr) << formatted << "\n";

    // Write to log file (thread-safe), rotate on size overflow
    {
        QMutexLocker locker(&logMutex);
        if (logFile && logFile->isOpen()) {
            logFile->write(utf8);
            logFile->write("\n");
            logFile->flush();
            s_bytesWritten += utf8.size() + 1;
            if (s_bytesWritten >= kMaxFileBytes) {
                rotateLogFile();
            }
        }
    }

    // Forward to original handler for Qt internal processing
    if (originalHandler) {
        originalHandler(type, context, msg);
    }
}

static void cleanOldLogs(const QString &logDir, int maxDays)
{
    QDir dir(logDir);
    const QDate today = QDate::currentDate();
    const QStringList entries = dir.entryList(QStringList() << "labeltorch_*.log", QDir::Files);

    for (const QString &entry : entries) {
        QFileInfo fi(dir.absoluteFilePath(entry));
        qint64 ageDays = fi.lastModified().date().daysTo(today);
        if (ageDays > maxDays) {
            QFile::remove(fi.absoluteFilePath());
        }
    }
}

namespace Log {

void init(const QString &logDir)
{
    // 日志目录：显式参数 > LT_LOG_DIR 环境变量 > AppDataLocation/logs
    QString dir = logDir;
    if (dir.isEmpty())
        dir = qEnvironmentVariable("LT_LOG_DIR");
    if (dir.isEmpty()) {
        dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs";
    }
    s_logDir = dir;
    QDir().mkpath(dir);

    // 会话标识：同一天的多次启动共用一个按天文件，靠 sid 区分会话
    s_sessionId = QStringLiteral("s") + QDateTime::currentDateTime().toString(QStringLiteral("HHmmss"));

    cleanOldLogs(dir, kMaxRetainedDays);

    // 按天命名基础路径；分卷计数从 0 开始
    QString dateStr = QDate::currentDate().toString("yyyyMMdd");
    s_fileBasePath = dir + QStringLiteral("/labeltorch_") + dateStr;
    s_fileIndex = 0;
    openLogFile();

    // 级别：环境变量最高，否则按构建类型给默认（Debug=DEBUG，Release=INFO）
    QString level = qEnvironmentVariable("LT_LOG_LEVEL").toLower();
    if (level.isEmpty()) {
#ifdef _DEBUG
        level = QStringLiteral("debug");
#else
        level = QStringLiteral("info");
#endif
    }
    applyLevelRules(level);

    // Install custom message handler
    originalHandler = qInstallMessageHandler(logMessageHandler);

    // 会话头：一次启动一行，标明版本与级别，便于跨会话定位
    qInfo() << "=== LabelTorch session" << s_sessionId << "version"
            << QCoreApplication::applicationVersion() << "Qt" << QT_VERSION_STR
            << "level" << level << "===";
    qInfo() << "Log file:" << (logFile ? logFile->fileName() : s_fileBasePath + ".log");
}

void setLevel(const QString &level)
{
    QString l = level.toLower();
    if (l != QStringLiteral("debug") && l != QStringLiteral("trace")
        && l != QStringLiteral("info") && l != QStringLiteral("warning")
        && l != QStringLiteral("error")) {
        qWarning() << "Unknown log level:" << level;
        return;
    }
    applyLevelRules(l);
    qInfo() << "Log level set to:" << l;
}

void setCategory(const QString &rule)
{
    QLoggingCategory::setFilterRules(rule);
    qInfo() << "Log category rule set:" << rule;
}

QString sessionId()
{
    return s_sessionId;
}

QString logDirPath()
{
    return s_logDir;
}

void shutdown()
{
    qInfo() << "=== LabelTorch logging shutting down ===";

    // Restore original handler first
    qInstallMessageHandler(originalHandler);
    originalHandler = nullptr;

    // Close and delete log file
    QMutexLocker locker(&logMutex);
    if (logFile) {
        logFile->close();
        delete logFile;
        logFile = nullptr;
    }
}

} // namespace Log
