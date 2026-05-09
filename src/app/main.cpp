#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>

#include "AppController.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName("LabelTorch");
    app.setApplicationName("LabelTorch");
    app.setApplicationVersion("0.1.0");

    QQuickStyle::setStyle("Basic");

    AppController controller;

    QQmlApplicationEngine engine;

    // 注册控制器到QML上下文
    engine.rootContext()->setContextProperty("appController", &controller);

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
