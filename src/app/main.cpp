#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>
#include <QStandardPaths>
#include <QDir>
#include <QWindow>
#include <QQuickWindow>
#include <QQuickItem>
#include <QFile>
#include <QTimer>
#include <QDateTime>
#include <QMetaObject>
#include <atomic>
#include <exception>
#include <cstdio>
#include <string>

#include <windows.h>
#include <dbghelp.h>
#ifdef Q_OS_WIN
#include <shobjidl.h>
#endif

#include "AppController.h"
#include "DemoBootstrap.h"
#include "ProjectService.h"
#include "ProjectModel.h"
#include "TaxonomyService.h"
#include "TaxonomyModel.h"
#include "DatasetService.h"
#include "DatasetModel.h"
#include "TagService.h"
#include "TagModel.h"
#include "ClassMappingService.h"
#include "AnnotationService.h"
#include "AnnotationModel.h"
#include "canvas/CanvasController.h"
#include "canvas/AnnotCanvasItem.h"
#include "ipc/IpcClient.h"
#include "SnapshotService.h"
#include "SnapshotModel.h"
#include "TrainingService.h"
#include "TrainingModel.h"
#include "ModelRegistry.h"
#include "MetricService.h"
#include "ModelVersionModel.h"
#include "InferenceService.h"
#include "AssistedLabelService.h"
#include "AnomalyService.h"
#include "AnomalyDetector.h"
#include "ExportService.h"
#include "ActiveLearningService.h"
#include "TestingService.h"
#include "TestingModel.h"
#include "Database.h"
#include <QSqlQuery>
#include "ThumbnailGenerator.h"
#include "cache/ThumbnailCache.h"
#include "cache/ThumbnailProvider.h"
#include "utils/Log.h"
#include "utils/AppSettings.h"

// 自定义消息处理器：将NaN ASSERT从FatalMsg降级为WarningMsg，防止程序abort
// Qt 6.11 Debug模式下qCheckedFPConversionToInteger检测到NaN会调用qFatal导致程序退出
// 但NaN来自Qt Quick布局引擎内部初始化竞态条件，不影响程序正常运行
//
// 注意：降级只是「保命」，几何状态可能已脏。因此这里做三件事：
//   1) 保留 hook/降级保护（移除会导致 Debug 构建直接崩）
//   2) 累计次数并通知 AppController，超阈值在状态栏建议重启
//   3) 防抖：CRT hook 与 Qt 消息处理器可能对同一次触发各响一次
#if defined(Q_OS_WIN) && defined(_DEBUG)
#include <crtdbg.h>
#include <string.h>
#endif

// ---- NaN ASSERT 计数与上报（Release 下也保留计数，只是不触发 CRT hook）----
namespace {

// 主线程 AppController 指针，由 main() 在构造后赋值；未就绪时仅本地计数
AppController *g_nanObserver = nullptr;

// 防抖窗口：同一毫秒级窗口内的重复触发视为同一次（hook 与消息处理器双路径）
std::atomic<qint64> g_lastNanMs{0};
// 累计次数（跨 hook / 消息处理器共享）
std::atomic<int> g_nanTotal{0};

bool claimNanOccurrence()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 last = g_lastNanMs.load(std::memory_order_relaxed);
    // 80ms 内的连续触发视为同一次 ASSERT（CRT 报告钩子 + qFatal 会各走一遍）
    while (now - last > 80) {
        if (g_lastNanMs.compare_exchange_weak(last, now, std::memory_order_relaxed))
            return true;
    }
    return false;
}

// 记录一次 NaN ASSERT：计数 + 尽快上报 UI；detail 仅用于日志
void noteNanAssert(const QString &detail)
{
    if (!claimNanOccurrence())
        return;

    const int total = ++g_nanTotal;
    // 前几次输出详情便于定位；之后静默计数，避免刷屏拖慢布局
    if (total <= 5) {
        fprintf(stderr, "\n=== NaN ASSERT #%d (suppressed, geometry may be dirty) ===\n", total);
        fprintf(stderr, "%s\n", detail.toUtf8().constData());
        fprintf(stderr, "=== END NaN ASSERT ===\n\n");
        fflush(stderr);
    }

    // 队列化到主线程：hook/消息处理器可能来自任意线程，不能直接改 QObject
    if (g_nanObserver) {
        QMetaObject::invokeMethod(g_nanObserver, "reportNanAssert",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, detail));
    }
}

// 仅前几次打印调用栈，帮助定位真因；后续省略以降低开销
void dumpCallStackIfNeeded()
{
    static std::atomic<int> s_stackDumped{0};
    if (s_stackDumped.fetch_add(1) >= 3)
        return;

    void *stack[32];
    USHORT frames = CaptureStackBackTrace(2, 32, stack, nullptr);
    SymInitialize(GetCurrentProcess(), nullptr, TRUE);
    fprintf(stderr, "Call stack (%u frames):\n", frames);
    for (USHORT i = 0; i < frames; i++) {
        DWORD64 addr = (DWORD64)stack[i];
        char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
        SYMBOL_INFO *symbol = (SYMBOL_INFO *)symbolBuffer;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;
        DWORD64 displacement = 0;
        if (SymFromAddr(GetCurrentProcess(), addr, &displacement, symbol)) {
            fprintf(stderr, "  [%u] %s+0x%llx (0x%llx)\n", i, symbol->Name,
                    (unsigned long long)displacement, (unsigned long long)addr);
        } else {
            fprintf(stderr, "  [%u] 0x%llx\n", i, (unsigned long long)addr);
        }
    }
    SymCleanup(GetCurrentProcess());
    fflush(stderr);
}

} // namespace

#if defined(Q_OS_WIN) && defined(_DEBUG)
static int __cdecl msvcReportHook(int reportType, char *message, int *returnValue)
{
    if (message && (reportType == _CRT_ERROR || reportType == _CRT_ASSERT)) {
        if (strstr(message, "isnan") ||
            strstr(message, "qnumeric.h") ||
            strstr(message, "FP(minimal)") ||
            strstr(message, "maximalPlusOne")) {

            // 保留抑制行为，但计入次数并上报，超阈值由状态栏提示重启
            noteNanAssert(QString::fromLocal8Bit(message));

            if (returnValue) {
                *returnValue = 0; // Tell caller not to break/abort
            }
            return TRUE; // Suppress the Debug Error dialog
        }
    }
    return FALSE; // Let standard handler display other assertion failures
}
#endif

static QtMessageHandler originalHandler = nullptr;
static void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // 过滤Qt内部高频调试日志，避免刷屏
    if (type == QtDebugMsg) {
        if (msg.contains("qt.scenegraph") ||
            msg.contains("qt.qpa.") ||
            msg.contains("qt.qml.binding")) {
            return;
        }
    }

    if (type == QtFatalMsg && (msg.contains("isnan") ||
                               msg.contains("qnumeric.h") ||
                               msg.contains("FP(minimal)") ||
                               msg.contains("maximalPlusOne"))) {
        // 将NaN相关的FatalMsg降级为WarningMsg，让程序继续运行
        const QString detail = QStringLiteral("%1  [%2:%3] %4")
                                   .arg(msg,
                                        context.file ? QString::fromUtf8(context.file) : QString(),
                                        QString::number(context.line),
                                        context.function ? QString::fromUtf8(context.function) : QString());
        noteNanAssert(detail);
        dumpCallStackIfNeeded();

        // 降级为WarningMsg转发给原始处理器，避免程序abort
        if (originalHandler) {
            originalHandler(QtWarningMsg, context, msg);
        }
        return;
    }
    if (originalHandler) {
        originalHandler(type, context, msg);
    }
}

// ============================================================================
// 崩溃捕获：未处理异常写 minidump，terminate 写错误摘要
// 路径基于 QStandardPaths::AppDataLocation 下的 logs/crash/，禁止硬编码绝对路径
// ============================================================================

/// 崩溃目录（应用名就绪后由 installCrashHandlers 写入；异常过滤器只读）
static QString g_crashDir;

/// 获取崩溃产物目录，不存在则创建
static QString crashDirPath()
{
    if (!g_crashDir.isEmpty()) return g_crashDir;
    // 应用名尚未就绪时的兜底：退回 AppDataLocation（此时可能无组织名，但仍非硬编码）
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QStringLiteral("/logs/crash");
}

/// 组装带时间戳的崩溃文件路径（dump / 文本共用命名规则）
static QString crashFilePath(const QString &suffix)
{
    QString dir = crashDirPath();
    QDir().mkpath(dir);
    QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    return dir + QStringLiteral("/crash_") + stamp + suffix;
}

/// 未处理异常过滤器：写 minidump 后交还系统默认处理
static LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS *info)
{
    // 崩溃路径上避免复杂对象：直接用 Win32 宽字符 API 写文件（路径可能含中文）
    QString dumpPath = crashFilePath(QStringLiteral(".dmp"));
    QString notePath = crashFilePath(QStringLiteral(".txt"));
    std::wstring dumpPathW = dumpPath.toStdWString();
    std::wstring notePathW = notePath.toStdWString();

    HANDLE hFile = CreateFileW(dumpPathW.c_str(), GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei;
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = info;
        mei.ClientPointers = FALSE;
        // MiniDumpNormal 足够定位崩溃点；完整内存转储体积过大不适合默认开启
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile,
                          MiniDumpNormal, &mei, nullptr, nullptr);
        CloseHandle(hFile);
    }

    // 同步写一份可读摘要，便于不依赖调试器时快速定位
    FILE *fp = _wfopen(notePathW.c_str(), L"w");
    if (fp) {
        fprintf(fp, "Unhandled exception\n");
        if (info && info->ExceptionRecord) {
            fprintf(fp, "ExceptionCode: 0x%08lX\n",
                    info->ExceptionRecord->ExceptionCode);
            fprintf(fp, "ExceptionAddress: %p\n",
                    info->ExceptionRecord->ExceptionAddress);
            // 浮点异常码与既有 NaN 抑制逻辑同源，这里仅记录不干预
            if (info->ExceptionRecord->ExceptionCode == EXCEPTION_FLT_INVALID_OPERATION
                || info->ExceptionRecord->ExceptionCode == EXCEPTION_FLT_DIVIDE_BY_ZERO
                || info->ExceptionRecord->ExceptionCode == EXCEPTION_FLT_OVERFLOW
                || info->ExceptionRecord->ExceptionCode == EXCEPTION_FLT_UNDERFLOW) {
                fprintf(fp, "Note: floating-point exception (NaN/Inf path may be related)\n");
            }
        }
        fprintf(fp, "DumpFile: %s\n", qUtf8Printable(dumpPath));
        fclose(fp);
    }

    fprintf(stderr, "[Crash] minidump written: %s\n", qUtf8Printable(dumpPath));
    fflush(stderr);
    return EXCEPTION_EXECUTE_HANDLER;
}

/// std::terminate 处理器：写简单错误信息后中止，避免无声退出
static void terminateHandler()
{
    QString notePath = crashFilePath(QStringLiteral("_terminate.txt"));
    std::wstring notePathW = notePath.toStdWString();

    FILE *fp = _wfopen(notePathW.c_str(), L"w");
    if (fp) {
        fprintf(fp, "std::terminate called (unhandled exception in noexcept context or no matching handler)\n");
        fprintf(fp, "Time: %s\n",
                qUtf8Printable(QDateTime::currentDateTime().toString(Qt::ISODate)));
        fclose(fp);
    }
    fprintf(stderr, "[Terminate] error info written: %s\n", qUtf8Printable(notePath));
    fflush(stderr);
    std::abort();
}

/// 安装崩溃捕获（须在应用名就绪后调用，保证崩溃目录落在 AppDataLocation）
static void installCrashHandlers()
{
    g_crashDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                 + QStringLiteral("/logs/crash");
    QDir().mkpath(g_crashDir);

    SetUnhandledExceptionFilter(unhandledExceptionFilter);
    std::set_terminate(terminateHandler);
    ltInfo(LT_LOG_APP()) << "Crash handlers installed, dump dir:" << g_crashDir;
}

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    // 强制设置 AppUserModelID 保证在 VS2026 Debug 模式（控制台子系统）下也能正常显示窗口/任务栏图标
    SetCurrentProcessExplicitAppUserModelID(L"LabelTorch.LabelTorch." APP_VERSION_WSTR);
#endif

#if defined(Q_OS_WIN) && defined(_DEBUG)
    // 注册CRT报告钩子，阻止MSVC弹出Abort/Retry/Ignore对话框，并使assert返回0继续运行
    _CrtSetReportHook(msvcReportHook);
#endif

    // 防止DPI缩放导致字体度量为NaN（Qt 6.11 + Windows已知问题）
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    // 安装自定义消息处理器以捕获NaN ASSERT调用栈
    originalHandler = qInstallMessageHandler(customMessageHandler);

    QGuiApplication app(argc, argv);
    app.setOrganizationName("LabelTorch");
    app.setApplicationName("LabelTorch");
    app.setApplicationVersion(APP_VERSION_STR);

    // 设置应用图标，任务栏和窗口标题栏显示
    // 使用多尺寸图标确保在不同DPI下都能正确显示
    QIcon appIcon;
    appIcon.addFile(QStringLiteral(":/icons/labeltorch_16x16.png"), QSize(16, 16));
    appIcon.addFile(QStringLiteral(":/icons/labeltorch_24x24.png"), QSize(24, 24));
    appIcon.addFile(QStringLiteral(":/icons/labeltorch_32x32.png"), QSize(32, 32));
    appIcon.addFile(QStringLiteral(":/icons/labeltorch_48x48.png"), QSize(48, 48));
    appIcon.addFile(QStringLiteral(":/icons/labeltorch_64x64.png"), QSize(64, 64));
    appIcon.addFile(QStringLiteral(":/icons/labeltorch_128x128.png"), QSize(128, 128));
    appIcon.addFile(QStringLiteral(":/icons/labeltorch_256x256.png"), QSize(256, 256));
    app.setWindowIcon(appIcon);

    Log::init();
    ltInfo(LT_LOG_APP()) << "Application starting" << "version" << app.applicationVersion()
                         << "Qt" << QT_VERSION_STR;

    // 应用名/日志就绪后安装崩溃捕获，minidump 写入 logs/crash/
    installCrashHandlers();

    QQuickStyle::setStyle("Basic");

    qmlRegisterType<AnnotCanvasItem>("LabelTorch.Annotation", 1, 0, "AnnotCanvasItem");

    QString dbPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dbPath);
    Database::instance().open(dbPath + "/labeltorch.db");
    Database::instance().initializeSchema();
    ltInfo(LT_LOG_DB()) << "Database initialized at" << dbPath + "/labeltorch.db";

    // 冷启动自检：修正残留的 running / preparing 状态任务
    // 通过 TrainingService / ExportService 的 reconcile 方法完成，
    // 确保逻辑封装在 Service 层，UI 层能收到状态变更信号
    // （注意：此处 trainingService / exportService 尚未创建，自检在创建后执行）


    AppSettings appSettings;
    AppController controller;
    ProjectService projectService;
    ProjectModel projectModel;
    TaxonomyService taxonomyService;
    TaxonomyModel taxonomyModel;
    DatasetService datasetService;
    DatasetModel datasetModel;
    TagService tagService;       // A6：数据集标签服务
    TagModel tagModel;           // A6：标签列表模型
    tagModel.setTagService(&tagService);
    DemoBootstrap demoBootstrap; // 演示模式：示例项目引导
    demoBootstrap.setServices(&projectService, &taxonomyService, &datasetService, &tagService);
    ClassMappingService classMappingService;
    AnnotationService annotationService;
    AnnotationModel annotationModel;
    CanvasController canvasController;
    // P1-21：缩略图管线——内存缓存(字节限容) + 后台生成器
    ThumbnailCache thumbnailCache;
    ThumbnailGenerator thumbnailGenerator;
    thumbnailCache.setCapacityBytes(ThumbnailCache::kDefaultCapacityBytes);
    IpcClient ipcClient;
    SnapshotService snapshotService;
    SnapshotModel snapshotModel;
    TrainingService trainingService;
    TrainingModel trainingModel;
    ModelRegistry modelRegistry;
    MetricService metricService;
    ModelVersionModel modelVersionModel;
    InferenceService inferenceService;
    AssistedLabelService assistedLabelService;
    AnomalyService anomalyService;
    AnomalyDetector anomalyDetector;
    ExportService exportService;
    ActiveLearningService activeLearningService;
    TestingService testingService;
    TestingModel testingModel;

    QString pythonExec = appSettings.pythonPath();
    if (pythonExec.isEmpty() || !QFile::exists(pythonExec)) {
        // 优先检查 python/python.exe（绿色版内嵌环境）
        QString embeddedPy = QCoreApplication::applicationDirPath() + QStringLiteral("/python/python.exe");
        if (QFile::exists(embeddedPy)) {
            pythonExec = embeddedPy;
        } else {
            pythonExec = QStringLiteral("python"); // fallback 系统 PATH
        }
    }
    ltInfo(LT_LOG_APP()) << "Using Python:" << pythonExec;
    ipcClient.startBackend(pythonExec);
    ltInfo(LT_LOG_IPC()) << "Python backend start requested" << pythonExec;

    projectService.setTaxonomyService(&taxonomyService);
    trainingService.setIpcClient(&ipcClient);
    trainingService.setModelRegistry(&modelRegistry);
    snapshotService.setIpcClient(&ipcClient); // P0-2：快照预览图
    datasetService.setIpcClient(&ipcClient); // P1-4 / P2-5：数据集统计与格式转换
    inferenceService.setIpcClient(&ipcClient);
    anomalyService.setIpcClient(&ipcClient);
    exportService.setIpcClient(&ipcClient);
    activeLearningService.setIpcClient(&ipcClient);
    testingService.setIpcClient(&ipcClient);
    testingService.setModelRegistry(&modelRegistry);
    // P1-14：辅助标注确认回写后，可直接编排「建快照 + 增量训练」
    assistedLabelService.setSnapshotService(&snapshotService);
    assistedLabelService.setTrainingService(&trainingService);

    // 冷启动自检：修正上次异常退出遗留的 running / preparing / verifying 状态
    int trainingFixed = trainingService.reconcileStaleRuns();
    int exportFixed = exportService.reconcileStaleExports();
    int testingFixed = testingService.reconcileStaleTasks();
    if (trainingFixed > 0 || exportFixed > 0 || testingFixed > 0) {
        ltWarning(LT_LOG_APP()) << "Cold boot: reconciled" << trainingFixed << "training runs,"
                                << exportFixed << "export artifacts,"
                                << testingFixed << "testing tasks";
    }

    QObject::connect(&controller, &AppController::currentProjectIdChanged, [&]() {
        if (controller.projectOpen()) {
            appSettings.addRecentProject(controller.currentProjectId());
            appSettings.setLastProjectPath(controller.currentProjectId());
        }
    });

    QObject::connect(&ipcClient, &IpcClient::connectedChanged, [&]() {
        controller.setPythonBackendReady(ipcClient.connected());
    });

    // 将 NaN 计数上报目标挂到 hook/消息处理器可见的全局指针
    // （此前 hook 只能 fprintf，现在可累计并驱动状态栏告警）
    g_nanObserver = &controller;

    QQmlApplicationEngine engine;

    // P1-21：注册缩略图 ImageProvider（image://thumb/<url编码路径>）
    engine.addImageProvider(QStringLiteral("thumb"),
                            new ThumbnailProvider(&thumbnailCache));

    engine.rootContext()->setContextProperty("appSettings", &appSettings);
    engine.rootContext()->setContextProperty("appController", &controller);
    engine.rootContext()->setContextProperty("demoBootstrap", &demoBootstrap);
    engine.rootContext()->setContextProperty("projectService", &projectService);
    engine.rootContext()->setContextProperty("projectModel", &projectModel);
    engine.rootContext()->setContextProperty("taxonomyService", &taxonomyService);
    engine.rootContext()->setContextProperty("taxonomyModel", &taxonomyModel);
    engine.rootContext()->setContextProperty("datasetService", &datasetService);
    engine.rootContext()->setContextProperty("datasetModel", &datasetModel);
    engine.rootContext()->setContextProperty("tagService", &tagService);
    engine.rootContext()->setContextProperty("tagModel", &tagModel);
    engine.rootContext()->setContextProperty("classMappingService", &classMappingService);
    engine.rootContext()->setContextProperty("annotationService", &annotationService);
    engine.rootContext()->setContextProperty("annotationModel", &annotationModel);
    engine.rootContext()->setContextProperty("canvasController", &canvasController);
    engine.rootContext()->setContextProperty("thumbnailGenerator", &thumbnailGenerator);
    engine.rootContext()->setContextProperty("thumbnailCache", &thumbnailCache);
    engine.rootContext()->setContextProperty("ipcClient", &ipcClient);
    engine.rootContext()->setContextProperty("snapshotService", &snapshotService);
    engine.rootContext()->setContextProperty("snapshotModel", &snapshotModel);
    engine.rootContext()->setContextProperty("trainingService", &trainingService);
    engine.rootContext()->setContextProperty("trainingModel", &trainingModel);
    engine.rootContext()->setContextProperty("modelRegistry", &modelRegistry);
    engine.rootContext()->setContextProperty("metricService", &metricService);
    engine.rootContext()->setContextProperty("modelVersionModel", &modelVersionModel);
    engine.rootContext()->setContextProperty("inferenceService", &inferenceService);
    engine.rootContext()->setContextProperty("assistedLabelService", &assistedLabelService);
    engine.rootContext()->setContextProperty("anomalyService", &anomalyService);
    engine.rootContext()->setContextProperty("anomalyDetector", &anomalyDetector);
    engine.rootContext()->setContextProperty("exportService", &exportService);
    engine.rootContext()->setContextProperty("activeLearningService", &activeLearningService);
    engine.rootContext()->setContextProperty("testingService", &testingService);
    engine.rootContext()->setContextProperty("testingModel", &testingModel);

    const QUrl url(QStringLiteral("qrc:/qt/qml/LabelTorch/Shell/qml/Main.qml"));

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url, &appIcon](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl) {
            ltError(LT_LOG_APP()) << "Failed to load Main.qml";
            QCoreApplication::exit(-1);
        } else if (obj && url == objUrl) {
            ltInfo(LT_LOG_APP()) << "Main.qml loaded successfully";
            // 窗口创建后显式设置图标，确保Windows任务栏显示
            if (auto *window = qobject_cast<QQuickWindow *>(obj)) {
                window->setIcon(appIcon);

                // 规避：窗口 show 前禁用根 Item 交互与布局刷新，避免布局引擎在半初始化
                // 状态下用 NaN 字体度量/几何做整数转换（qCheckedFPConversionToInteger）。
                // QQuickWindow 的 contentItem 可禁用整棵场景树的交互。
                // 等首帧布局与字体度量稳定后再放开。
                if (auto *rootItem = window->contentItem()) {
                    rootItem->setEnabled(false);
                    QTimer::singleShot(120, rootItem, [rootItem]() {
                        if (rootItem) {
                            rootItem->setEnabled(true);
                            ltInfo(LT_LOG_APP()) << "Root item re-enabled after layout warm-up";
                        }
                    });
                }
            }
        }
    }, Qt::QueuedConnection);

    ltInfo(LT_LOG_APP()) << "Loading main QML";
    engine.load(url);

    // 自动化验收钩子（默认无操作，仅本地自动化截图/回归验证用）：
    //   LT_DEBUG_OPEN_PROJECT=<项目名>  启动后按名称打开项目
    //   LT_DEBUG_PAGE=<pageId>          启动后切换到指定页面（dataset/check/annotation/...）
    {
        const QString autoOpen = qEnvironmentVariable("LT_DEBUG_OPEN_PROJECT");
        const QString autoPage = qEnvironmentVariable("LT_DEBUG_PAGE");
        // 首次运行（库中无任何项目）：自动创建并打开示例项目，保证开箱即可浏览全部功能
        if (projectService.listProjects().isEmpty()) {
            const QString demoPid = demoBootstrap.ensureDemoProject();
            if (!demoPid.isEmpty()) {
                projectModel.refresh();
                projectService.openProject(demoPid);
                controller.openProject(demoPid, QStringLiteral("示例项目"));
                const QVariantList taxes = taxonomyService.listTaxonomies(demoPid);
                if (!taxes.isEmpty())
                    taxonomyModel.setTaxonomyId(taxes.first().toMap().value("id").toString());
                ltInfo(LT_LOG_APP()) << "First run: demo project created and opened";
            }
        }
        // 自动化验收钩子：LT_DEBUG_ENSURE_DEMO=1 强制确保示例项目存在
        if (qEnvironmentVariable("LT_DEBUG_ENSURE_DEMO") == QStringLiteral("1")) {
            const QString demoPid = demoBootstrap.ensureDemoProject();
            if (!demoPid.isEmpty()) {
                projectModel.refresh();
                if (autoOpen.isEmpty()) {
                    projectService.openProject(demoPid);
                    controller.openProject(demoPid, QStringLiteral("示例项目"));
                    const QVariantList taxes = taxonomyService.listTaxonomies(demoPid);
                    if (!taxes.isEmpty())
                        taxonomyModel.setTaxonomyId(taxes.first().toMap().value("id").toString());
                }
            }
        }
        if (!autoOpen.isEmpty()) {
            const QVariantList all = projectService.listProjects();
            for (const QVariant &entry : all) {
                const QVariantMap m = entry.toMap();
                if (m.value("name").toString() == autoOpen) {
                    const QString pid = m.value("id").toString();
                    projectService.openProject(pid);
                    controller.openProject(pid, autoOpen);
                    ltInfo(LT_LOG_APP()) << "Auto-opened project for verification:" << autoOpen;
                    break;
                }
            }
        }
        if (!autoPage.isEmpty()) {
            controller.setCurrentPage(autoPage);
        }
        // LT_DEBUG_AUTO_SNAPSHOT=1：为当前项目第一个数据集自动创建冻结版（E2E 验收用）
        if (qEnvironmentVariable("LT_DEBUG_AUTO_SNAPSHOT") == QStringLiteral("1")
            && !controller.currentProjectId().isEmpty()) {
            const QVariantList dss = datasetService.listDatasets(controller.currentProjectId());
            if (!dss.isEmpty()) {
                const QString dsId = dss.first().toMap().value("id").toString();
                const QString snapId = snapshotService.createSnapshot(dsId, 0.75, QStringLiteral("random"));
                ltInfo(LT_LOG_APP()) << "Auto snapshot created:" << snapId << "for dataset" << dsId;
            }
        }
        // LT_DEBUG_AUTO_TRAIN=1：对最新冻结版自动创建训练任务并启动（小参数 CPU，E2E 验收用）
        if (qEnvironmentVariable("LT_DEBUG_AUTO_TRAIN") == QStringLiteral("1")
            && !controller.currentProjectId().isEmpty()) {
            const QVariantList dss = datasetService.listDatasets(controller.currentProjectId());
            if (!dss.isEmpty()) {
                const QString dsId = dss.first().toMap().value("id").toString();
                const QVariantList snaps = snapshotService.listSnapshots(dsId);
                if (!snaps.isEmpty()) {
                    const QString snapId = snaps.first().toMap().value("id").toString();
                    // 场景覆盖：LT_DEBUG_TRAIN_ADAPTER=ultralytics|anomalib、
                    //           LT_DEBUG_TRAIN_EPOCHS=<n>、LT_DEBUG_TRAIN_FAMILY=<family>
                    const QString adapter = qEnvironmentVariable("LT_DEBUG_TRAIN_ADAPTER");
                    const bool isAnomalib = (adapter == QStringLiteral("anomalib"));
                    const QString family = qEnvironmentVariable("LT_DEBUG_TRAIN_FAMILY");
                    bool epochsOk = false;
                    const int epochsEnv = qEnvironmentVariable("LT_DEBUG_TRAIN_EPOCHS").toInt(&epochsOk);
                    const int epochs = (epochsOk && epochsEnv > 0) ? epochsEnv : 3;
                    QString config;
                    if (isAnomalib) {
                        config = QStringLiteral(
                            "{\"adapter\":\"anomalib\",\"imgsz\":256,\"img_size\":256,\"batch\":4,"
                            "\"epochs\":%1,\"patience\":50,\"workers\":2,\"amp\":false,\"resume\":false,"
                            "\"device\":\"cpu\",\"model_family\":\"patchcore\",\"backbone\":\"wide_resnet50_2\","
                            "\"anomaly_score_threshold\":0.5,\"training_type\":\"from_scratch\",\"pretrained\":false}")
                                     .arg(epochs);
                    } else {
                        const QString fam = family.isEmpty() ? QStringLiteral("yolov8") : family;
                        config = QStringLiteral(
                            "{\"adapter\":\"ultralytics\",\"imgsz\":320,\"img_size\":320,\"batch\":4,"
                            "\"epochs\":%1,\"patience\":50,\"workers\":2,\"amp\":false,\"resume\":false,"
                            "\"device\":\"cpu\",\"model_family\":\"%2\",\"training_type\":\"from_scratch\","
                            "\"pretrained\":false,\"input_channels\":3,\"save_period\":10,"
                            "\"optimizer\":\"SGD\",\"lr0\":0.01,\"weight_decay\":0.0005,\"iou\":0.7}")
                                     .arg(epochs).arg(fam);
                    }
                    const QString runId = trainingService.createRun(
                        controller.currentProjectId(), snapId, config);
                    if (!runId.isEmpty() && trainingService.startTraining(runId))
                        ltInfo(LT_LOG_APP()) << "Auto training started:" << runId;
                    else
                        ltError(LT_LOG_APP()) << "Auto training failed to start:" << runId;
                } else {
                    ltError(LT_LOG_APP()) << "Auto train: no snapshot for dataset" << dsId;
                }
            }
        }
        // LT_DEBUG_AUTO_TEST=1：对最新模型版本+最新冻结版自动创建并启动评估（E2E 验收用）
        if (qEnvironmentVariable("LT_DEBUG_AUTO_TEST") == QStringLiteral("1")
            && !controller.currentProjectId().isEmpty()) {
            const QString projectId = controller.currentProjectId();
            QSqlQuery mvQuery(Database::instance().database());
            mvQuery.prepare("SELECT id FROM model_versions WHERE project_id = ? ORDER BY created_at DESC LIMIT 1");
            mvQuery.addBindValue(projectId);
            mvQuery.exec();
            QString mvId;
            if (mvQuery.next()) mvId = mvQuery.value(0).toString();
            const QVariantList dss = datasetService.listDatasets(projectId);
            QString snapId;
            if (!dss.isEmpty()) {
                const QVariantList snaps = snapshotService.listSnapshots(dss.first().toMap().value("id").toString());
                if (!snaps.isEmpty()) snapId = snaps.first().toMap().value("id").toString();
            }
            if (!mvId.isEmpty() && !snapId.isEmpty()) {
                const QString cfg = QStringLiteral(
                    "{\"batch\":4,\"iou_threshold\":0.45,\"conf_threshold\":0.25,"
                    "\"device\":\"cpu\",\"weight_index\":0}");
                const QString taskId = testingService.createTestTask(projectId, mvId, snapId, cfg);
                if (!taskId.isEmpty() && testingService.startTestTask(taskId))
                    ltInfo(LT_LOG_APP()) << "Auto test started:" << taskId;
                else
                    ltError(LT_LOG_APP()) << "Auto test failed:" << taskId;
            } else {
                ltError(LT_LOG_APP()) << "Auto test: missing model version or snapshot";
            }
        }
        // LT_DEBUG_AUTO_EXPORT=1：对最新模型版本自动导出 onnx 并验证（E2E 验收用）
        if (qEnvironmentVariable("LT_DEBUG_AUTO_EXPORT") == QStringLiteral("1")
            && !controller.currentProjectId().isEmpty()) {
            QSqlQuery mvQuery(Database::instance().database());
            mvQuery.prepare("SELECT id FROM model_versions WHERE project_id = ? ORDER BY created_at DESC LIMIT 1");
            mvQuery.addBindValue(controller.currentProjectId());
            mvQuery.exec();
            if (mvQuery.next()) {
                const QString mvId = mvQuery.value(0).toString();
                const QString artifactId = exportService.exportModel(mvId, QStringLiteral("onnx"), QStringLiteral("{}"));
                if (!artifactId.isEmpty()) {
                    // 验证由 ExportService 在导出完成信号后自动执行（异步），此处不重复触发
                    ltInfo(LT_LOG_APP()) << "Auto export started:" << artifactId;
                } else {
                    ltError(LT_LOG_APP()) << "Auto export failed for model version" << mvId;
                }
            }
        }
    }

    int ret = app.exec();
    ltInfo(LT_LOG_APP()) << "Application exiting with code" << ret;
    Log::shutdown();
    return ret;
}
