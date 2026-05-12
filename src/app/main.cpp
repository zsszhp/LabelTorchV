#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>
#include <QStandardPaths>
#include <QDir>

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
#include "ExportService.h"
#include "Database.h"
#include "utils/Log.h"
#include "utils/AppSettings.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName("LabelTorch");
    app.setApplicationName("LabelTorch");
    app.setApplicationVersion("0.1.0");

    Log::init();
    ltInfo(LT_LOG_APP()) << "Application starting" << "version" << app.applicationVersion()
                         << "Qt" << QT_VERSION_STR;

    QQuickStyle::setStyle("Basic");

    QString dbPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dbPath);
    Database::instance().open(dbPath + "/labeltorch.db");
    Database::instance().initializeSchema();
    ltInfo(LT_LOG_DB()) << "Database initialized at" << dbPath + "/labeltorch.db";

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
    ExportService exportService;

    QString pythonExec = appSettings.pythonPath();
    if (pythonExec.isEmpty() || pythonExec == QStringLiteral("python")) {
        pythonExec = QStringLiteral("python");
    }
    ipcClient.startBackend(pythonExec);
    ltInfo(LT_LOG_IPC()) << "Python backend start requested" << pythonExec;

    projectService.setTaxonomyService(&taxonomyService);
    trainingService.setIpcClient(&ipcClient);
    inferenceService.setIpcClient(&ipcClient);
    exportService.setIpcClient(&ipcClient);

    QObject::connect(&controller, &AppController::currentProjectIdChanged, [&]() {
        if (controller.projectOpen()) {
            appSettings.addRecentProject(controller.currentProjectName());
            appSettings.setLastProjectPath(controller.currentProjectName());
        }
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
    engine.rootContext()->setContextProperty("exportService", &exportService);

    const QUrl url(u"qrc:/LabelTorch/Shell/qml/Main.qml"_qs);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl) {
            ltError(LT_LOG_APP()) << "Failed to load Main.qml";
            QCoreApplication::exit(-1);
        }
    }, Qt::QueuedConnection);

    ltInfo(LT_LOG_APP()) << "Loading main QML";
    engine.load(url);

    int ret = app.exec();
    ltInfo(LT_LOG_APP()) << "Application exiting with code" << ret;
    Log::shutdown();
    return ret;
}
