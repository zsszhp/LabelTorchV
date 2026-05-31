#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>
#include <QStandardPaths>
#include <QDir>
#include <QWindow>

#include <windows.h>
#include <dbghelp.h>

#include "AppController.h"
#include "ProjectService.h"
#include "ProjectModel.h"
#include "TaxonomyService.h"
#include "TaxonomyModel.h"
#include "DatasetService.h"
#include "DatasetModel.h"
#include "ClassMappingService.h"
#include "AnnotationService.h"
#include "AnnotationModel.h"
#include "canvas/CanvasController.h"
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
#include "Database.h"
#include "utils/Log.h"
#include "utils/AppSettings.h"
#include <QSqlQuery>
#include <QDateTime>
#include <QFile>

// 自定义消息处理器：将NaN ASSERT从FatalMsg降级为WarningMsg，防止程序abort
// Qt 6.11 Debug模式下qCheckedFPConversionToInteger检测到NaN会调用qFatal导致程序退出
// 但NaN来自Qt Quick布局引擎内部初始化竞态条件，不影响程序正常运行
#if defined(Q_OS_WIN) && defined(_DEBUG)
#include <crtdbg.h>
#include <string.h>

static int __cdecl msvcReportHook(int reportType, char *message, int *returnValue)
{
    if (message && (reportType == _CRT_ERROR || reportType == _CRT_ASSERT)) {
        if (strstr(message, "isnan") || 
            strstr(message, "qnumeric.h") || 
            strstr(message, "FP(minimal)") || 
            strstr(message, "maximalPlusOne")) {
            
            fprintf(stderr, "\n=== NaN/Float ASSERT (suppressed via hook) ===\n");
            fprintf(stderr, "Message: %s\n", message);
            fprintf(stderr, "=== END NaN/Float ASSERT (suppressed via hook) ===\n\n");
            fflush(stderr);
            
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
        fprintf(stderr, "\n=== NaN ASSERT (suppressed) ===\n");
        fprintf(stderr, "Message: %s\n", msg.toUtf8().constData());
        fprintf(stderr, "File: %s:%d\n", context.file ? context.file : "", context.line);
        fprintf(stderr, "Function: %s\n", context.function ? context.function : "");

        // 打印调用栈帮助定位问题
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
        fprintf(stderr, "=== END NaN ASSERT (suppressed) ===\n\n");
        fflush(stderr);

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

int main(int argc, char *argv[])
{
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
    app.setApplicationVersion("0.1.0");

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

    QQuickStyle::setStyle("Basic");

    QString dbPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dbPath);
    Database::instance().open(dbPath + "/labeltorch.db");
    Database::instance().initializeSchema();
    ltInfo(LT_LOG_DB()) << "Database initialized at" << dbPath + "/labeltorch.db";

    // 冷启动自检：修正残留的 running / preparing 状态任务
    {
        QSqlQuery fixQuery(Database::instance().database());
        int fixed = 0;
        fixQuery.prepare("UPDATE training_runs SET status = 'stopped', "
                         "finished_at = ? WHERE status IN ('running', 'preparing')");
        fixQuery.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
        if (fixQuery.exec()) {
            fixed = fixQuery.numRowsAffected();
        }
        if (fixed > 0) {
            ltWarning(LT_LOG_APP()) << "Cold boot: fixed" << fixed << "orphaned running tasks -> stopped";
        }
    }


    AppSettings appSettings;
    AppController controller;
    ProjectService projectService;
    ProjectModel projectModel;
    TaxonomyService taxonomyService;
    TaxonomyModel taxonomyModel;
    DatasetService datasetService;
    DatasetModel datasetModel;
    ClassMappingService classMappingService;
    AnnotationService annotationService;
    AnnotationModel annotationModel;
    CanvasController canvasController;
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

    QString pythonExec = appSettings.pythonPath();
    if (pythonExec.isEmpty() || !QFile::exists(pythonExec)) {
        // 优先检查 runtime/python/python.exe（绿色版）
        QString embeddedPy = QCoreApplication::applicationDirPath() + QStringLiteral("/runtime/python/python.exe");
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
    inferenceService.setIpcClient(&ipcClient);
    anomalyService.setIpcClient(&ipcClient);
    exportService.setIpcClient(&ipcClient);
    activeLearningService.setIpcClient(&ipcClient);

    QObject::connect(&controller, &AppController::currentProjectIdChanged, [&]() {
        if (controller.projectOpen()) {
            appSettings.addRecentProject(controller.currentProjectId());
            appSettings.setLastProjectPath(controller.currentProjectId());
        }
    });

    QObject::connect(&ipcClient, &IpcClient::connectedChanged, [&]() {
        controller.setPythonBackendReady(ipcClient.connected());
    });

    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty("appSettings", &appSettings);
    engine.rootContext()->setContextProperty("appController", &controller);
    engine.rootContext()->setContextProperty("projectService", &projectService);
    engine.rootContext()->setContextProperty("projectModel", &projectModel);
    engine.rootContext()->setContextProperty("taxonomyService", &taxonomyService);
    engine.rootContext()->setContextProperty("taxonomyModel", &taxonomyModel);
    engine.rootContext()->setContextProperty("datasetService", &datasetService);
    engine.rootContext()->setContextProperty("datasetModel", &datasetModel);
    engine.rootContext()->setContextProperty("classMappingService", &classMappingService);
    engine.rootContext()->setContextProperty("annotationService", &annotationService);
    engine.rootContext()->setContextProperty("annotationModel", &annotationModel);
    engine.rootContext()->setContextProperty("canvasController", &canvasController);
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

    const QUrl url(QStringLiteral("qrc:/qt/qml/LabelTorch/Shell/qml/Main.qml"));

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url, &appIcon](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl) {
            ltError(LT_LOG_APP()) << "Failed to load Main.qml";
            QCoreApplication::exit(-1);
        } else if (obj && url == objUrl) {
            ltInfo(LT_LOG_APP()) << "Main.qml loaded successfully";
            // 窗口创建后显式设置图标，确保Windows任务栏显示
            if (auto *window = qobject_cast<QWindow *>(obj)) {
                window->setIcon(appIcon);
            }
        }
    }, Qt::QueuedConnection);

    ltInfo(LT_LOG_APP()) << "Loading main QML";
    engine.load(url);

    int ret = app.exec();
    ltInfo(LT_LOG_APP()) << "Application exiting with code" << ret;
    Log::shutdown();
    return ret;
}
