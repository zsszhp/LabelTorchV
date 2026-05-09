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
#include "Database.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName("LabelTorch");
    app.setApplicationName("LabelTorch");
    app.setApplicationVersion("0.1.0");

    QQuickStyle::setStyle("Basic");

    // 初始化数据库（应用级，存储在AppData）
    QString dbPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dbPath);
    Database::instance().open(dbPath + "/labeltorch.db");
    Database::instance().initializeSchema();

    // 初始化服务
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

    // 启动Python后端（如果可用）
    QString pythonExec = "F:/A/anaconda/envs/labeltorch/python.exe";
    QString serverScript = "F:/project/my/LabelTorchV/backend/labeltorch_backend/__main__.py";
    ipcClient.startBackend(pythonExec, serverScript);

    // 注入依赖
    projectService.setTaxonomyService(&taxonomyService);

    QQmlApplicationEngine engine;

    // 注册服务到QML上下文
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

    // 加载主窗口
    const QUrl url(u"qrc:/LabelTorch/Shell/qml/Main.qml"_qs);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);

    engine.load(url);

    return app.exec();
}
