#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "about.h"
#include "settings.h"
#include "history.h"
#include "mainview.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    app.setOrganizationName("ChatAgent");
    app.setApplicationName("ChatAgent");
    app.setApplicationVersion("1.0.0");

    // ── 后端对象 ──────────────────────────────────────────────────────────────
    About    about;
    Settings settings;
    History  history;
    MainView mainView(&settings, &history);

    // ── QML 引擎 ──────────────────────────────────────────────────────────────
    QQmlApplicationEngine engine;
    QQmlContext *ctx = engine.rootContext();
    ctx->setContextProperty("about",    &about);
    ctx->setContextProperty("settings", &settings);
    ctx->setContextProperty("history",  &history);
    ctx->setContextProperty("mainView", &mainView);

    const QUrl url(QStringLiteral("qrc:/src/qml/Main.qml"));
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated,
        &app, [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection
    );
    engine.load(url);

    return app.exec();
}

