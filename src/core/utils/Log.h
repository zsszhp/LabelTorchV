#ifndef LOG_H
#define LOG_H

#include <QString>
#include <QLoggingCategory>

// Module-level logging categories
#define LT_LOG_CORE()      QLoggingCategory("lt.core")
#define LT_LOG_DB()        QLoggingCategory("lt.db")
#define LT_LOG_FS()        QLoggingCategory("lt.fs")
#define LT_LOG_IPC()       QLoggingCategory("lt.ipc")
#define LT_LOG_PROJECT()   QLoggingCategory("lt.project")
#define LT_LOG_TAXONOMY()  QLoggingCategory("lt.taxonomy")
#define LT_LOG_DATASET()   QLoggingCategory("lt.dataset")
#define LT_LOG_ANNOTATION() QLoggingCategory("lt.annotation")
#define LT_LOG_TRAINING()  QLoggingCategory("lt.training")
#define LT_LOG_MODEL()     QLoggingCategory("lt.model")
#define LT_LOG_INFERENCE() QLoggingCategory("lt.inference")
#define LT_LOG_EXPORT()    QLoggingCategory("lt.export")
#define LT_LOG_APP()       QLoggingCategory("lt.app")

// Convenience macros with function name
#define ltTrace(category)   qCDebug(category) << __FUNCTION__ << ":"
#define ltDebug(category)   qCDebug(category) << __FUNCTION__ << ":"
#define ltInfo(category)    qCInfo(category) << __FUNCTION__ << ":"
#define ltWarning(category) qCWarning(category) << __FUNCTION__ << ":"
#define ltError(category)   qCCritical(category) << __FUNCTION__ << ":"

namespace Log {

// Initialize logging system (call once at app startup)
// logDir: directory for log files; if empty, uses AppDataLocation/logs
void init(const QString &logDir = {});

// Set minimum log level: "trace", "debug", "info", "warning", "error"
void setLevel(const QString &level);

// Enable/disable specific category (e.g., "lt.ipc=true", "lt.db=false")
void setCategory(const QString &rule);

// Shutdown logging, flush files
void shutdown();

} // namespace Log

#endif // LOG_H
