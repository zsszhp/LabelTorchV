#ifndef LOG_H
#define LOG_H

#include <QString>
#include <QLoggingCategory>

// Module-level logging categories.
//
// 崩溃修复（2026-10-06 拉框标注闪退）：旧写法 `QLoggingCategory("lt.xxx")` 每次
// 展开都构造一个【临时】QLoggingCategory——其构造向全局 QLoggingRegistry 注册、
// 析构立即注销（反汇编 Qt6Core!QLoggingCategory::~QLoggingCategory 证实）。
// 全代码库每条日志都走一遍「注册→输出→注销」，多线程（缩略图/IPC/训练回调）下
// 全局注册表竞态导致隐式共享 QString 引用计数损坏，表现为跨页面随机的
// 0xC0000005 闪退（09:26 标注保存、10-03 DatasetModel roleNames 风暴均此模式）。
//
// 正确用法（Qt 文档亦要求 category 为静态存活对象）：函数局部 static，
// C++11 magic statics 保证线程安全初始化，进程退出时才析构，零注册风暴。
#define LT_LOG_CATEGORY_IMPL(name)                          \
    []() -> QLoggingCategory & {                            \
        static QLoggingCategory cat(name);                  \
        return cat;                                         \
    }()

#define LT_LOG_CORE()      LT_LOG_CATEGORY_IMPL("lt.core")
#define LT_LOG_DB()        LT_LOG_CATEGORY_IMPL("lt.db")
#define LT_LOG_FS()        LT_LOG_CATEGORY_IMPL("lt.fs")
#define LT_LOG_IPC()       LT_LOG_CATEGORY_IMPL("lt.ipc")
#define LT_LOG_PROJECT()   LT_LOG_CATEGORY_IMPL("lt.project")
#define LT_LOG_TAXONOMY()  LT_LOG_CATEGORY_IMPL("lt.taxonomy")
#define LT_LOG_DATASET()   LT_LOG_CATEGORY_IMPL("lt.dataset")
#define LT_LOG_ANNOTATION() LT_LOG_CATEGORY_IMPL("lt.annotation")
#define LT_LOG_TRAINING()  LT_LOG_CATEGORY_IMPL("lt.training")
#define LT_LOG_MODEL()     LT_LOG_CATEGORY_IMPL("lt.model")
#define LT_LOG_INFERENCE() LT_LOG_CATEGORY_IMPL("lt.inference")
#define LT_LOG_EXPORT()    LT_LOG_CATEGORY_IMPL("lt.export")
#define LT_LOG_TESTING()   LT_LOG_CATEGORY_IMPL("lt.testing")
#define LT_LOG_APP()       LT_LOG_CATEGORY_IMPL("lt.app")
#define LT_LOG_UI()        LT_LOG_CATEGORY_IMPL("lt.ui")
#define LT_LOG_SYS()       LT_LOG_CATEGORY_IMPL("lt.sys")

// Convenience macros with function name
#define ltTrace(category)   qCDebug(category) << __FUNCTION__ << ":"
#define ltDebug(category)   qCDebug(category) << __FUNCTION__ << ":"
#define ltInfo(category)    qCInfo(category) << __FUNCTION__ << ":"
#define ltWarning(category) qCWarning(category) << __FUNCTION__ << ":"
#define ltError(category)   qCCritical(category) << __FUNCTION__ << ":"

namespace Log {

// Initialize logging system (call once at app startup, after QCoreApplication
// has application name/version set).
// logDir: directory for log files; if empty, uses AppDataLocation/logs,
//         overridable via LT_LOG_DIR environment variable.
// Level: LT_LOG_LEVEL env > caller-set default via setLevel > build default
//        (Debug build: DEBUG / Release build: INFO).
void init(const QString &logDir = {});

// Set minimum log level: "debug", "info", "warning", "error" ("trace" == debug)
void setLevel(const QString &level);

// Enable/disable specific category (e.g., "lt.ipc=true", "lt.db=false")
void setCategory(const QString &rule);

// Short session id ("s" + start time), stamped on every log line so that
// multiple launches sharing one daily file can be told apart.
QString sessionId();

// Directory the log files live in (valid after init()).
QString logDirPath();

// Shutdown logging, flush files
void shutdown();

} // namespace Log

#endif // LOG_H
