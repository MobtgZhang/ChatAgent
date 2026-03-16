#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>

#include "about.h"
#include "locale_bridge.h"
#include "settings.h"
#include "history.h"
#include "memory_module.h"
#include "mainview.h"
#include "clipboard_bridge.h"
#include "skill_manager.h"

int main(int argc, char *argv[]) {
    // 输入法设置（必须在创建 QGuiApplication 之前）
    // 仅在 Linux 下设置 fcitx/ibus，避免在 Windows 上覆盖系统 IME（搜狗等中文输入法候选框依赖系统默认）
#if defined(Q_OS_LINUX)
    // 兼容 X11 与 Wayland；Qt 6.7+ 支持 QT_IM_MODULES 多级 fallback
    qputenv("QT_IM_MODULES", "fcitx5;fcitx;wayland;ibus");
    qputenv("QT_IM_MODULE", "fcitx5");
    qputenv("XMODIFIERS", "@im=fcitx");  // X11/XWayland 需要
#endif
    // Windows / macOS：不设置 QT_IM_MODULE，使用系统默认 IME（如 Windows TSF、搜狗、微软拼音等）

    QtWebEngineQuick::initialize();
    QGuiApplication app(argc, argv);
    app.setOrganizationName("ChatAgent");
    app.setApplicationName("ChatAgent");
    app.setApplicationVersion("1.3.0");
    app.setWindowIcon(QIcon("qrc:/src/icons/app_icon.png"));

    // ── 后端对象 ──────────────────────────────────────────────────────────────
    About        about;
    Settings     settings;
    LocaleBridge locale(&settings);
    settings.setLocaleBridge(&locale);
    settings.refreshModels();
    History      history(&settings);
    // SkillManager 先于 MainView 创建，以便 setupAgent 完成后立即注入
    SkillManager skillManager(&settings);
    MainView     mainView(&settings, &history);
    mainView.setSkillManager(&skillManager);  // 连接技能库到 AgentCore（感知/记忆阶段）

    // ── QML 引擎 ──────────────────────────────────────────────────────────────
    QQmlApplicationEngine engine;
    QQmlContext *ctx = engine.rootContext();
    ctx->setContextProperty("about",       &about);
    ctx->setContextProperty("locale",      &locale);
    ctx->setContextProperty("localeBridge", &locale);  // 独立名称，避免 Qt 6 内置 locale 覆盖
    ctx->setContextProperty("settings",    &settings);
    ctx->setContextProperty("history",     &history);
    ctx->setContextProperty("mainView",    &mainView);
    ClipboardBridge clipboardBridge;
    ctx->setContextProperty("clipboardBridge", &clipboardBridge);
    ctx->setContextProperty("agentMemory", static_cast<QObject*>(mainView.agentMemory()));
    ctx->setContextProperty("skillManager", &skillManager);

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

    // 关闭最后一个窗口时退出进程，避免关闭界面后后台仍驻留
    QObject::connect(&app, &QGuiApplication::lastWindowClosed, &app, &QCoreApplication::quit);

    return app.exec();
}
