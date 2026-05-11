#include "Log.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QLoggingCategory>
#include <QDebug>

static QMutex logMutex;
static QFile *logFile = nullptr;
static QtMessageHandler originalHandler = nullptr;

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

static void logMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // Format the structured log line
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString category = context.category ? context.category : "";
    QString function = context.function ? context.function : "";
    QString file = context.file ? context.file : "";
    int line = context.line;

    QString formatted = QString("[%1] [%2] [%3] %4")
        .arg(timestamp, levelString(type), category, msg);

    if (!function.isEmpty()) {
        formatted += QString("  [%1:%2]").arg(function).arg(line);
    }

    // Write to stderr (console)
    QTextStream(stderr) << formatted << "\n";

    // Write to log file (thread-safe)
    QMutexLocker locker(&logMutex);
    if (logFile && logFile->isOpen()) {
        QTextStream stream(logFile);
        stream << formatted << "\n";
        stream.flush();
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
    // Determine log directory
    QString dir = logDir;
    if (dir.isEmpty()) {
        dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs";
    }

    // Create log directory if needed
    QDir().mkpath(dir);

    // Clean logs older than 7 days
    cleanOldLogs(dir, 7);

    // Open log file with date-based name
    QString dateStr = QDate::currentDate().toString("yyyyMMdd");
    QString filePath = dir + QString("/labeltorch_%1.log").arg(dateStr);

    logFile = new QFile(filePath);
    if (!logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        delete logFile;
        logFile = nullptr;
        qWarning() << "Failed to open log file:" << filePath;
        return;
    }

    // Install custom message handler
    originalHandler = qInstallMessageHandler(logMessageHandler);

    // Default: enable info level and above for all categories
    QLoggingCategory::setFilterRules(QStringLiteral("lt.*=true\nqt.*=false"));

    qInfo() << "=== LabelTorch logging initialized ===";
    qInfo() << "Log file:" << filePath;
}

void setLevel(const QString &level)
{
    QString l = level.toLower();

    // Set filter rules based on level
    // "trace"/"debug" -> enable all lt.* debug output
    // "info" -> enable lt.* info and above
    // "warning" -> enable lt.* warning and above
    // "error" -> enable lt.* critical only
    QString rule;
    if (l == "trace" || l == "debug") {
        rule = QStringLiteral("lt.*.debug=true\nlt.*.info=true\nlt.*.warning=true\nlt.*.critical=true");
    } else if (l == "info") {
        rule = QStringLiteral("lt.*.debug=false\nlt.*.info=true\nlt.*.warning=true\nlt.*.critical=true");
    } else if (l == "warning") {
        rule = QStringLiteral("lt.*.debug=false\nlt.*.info=false\nlt.*.warning=true\nlt.*.critical=true");
    } else if (l == "error") {
        rule = QStringLiteral("lt.*.debug=false\nlt.*.info=false\nlt.*.warning=false\nlt.*.critical=true");
    } else {
        qWarning() << "Unknown log level:" << level;
        return;
    }

    QLoggingCategory::setFilterRules(rule);
    qInfo() << "Log level set to:" << level;
}

void setCategory(const QString &rule)
{
    QLoggingCategory::setFilterRules(rule);
    qInfo() << "Log category rule set:" << rule;
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
