#include "wait_tool.h"
#include <QTimer>
#include <QEventLoop>
#include <QJsonObject>
#include <QJsonDocument>
#include <QCoreApplication>

WaitTool::WaitTool(QObject *parent) : BaseTool(parent) {}

QString WaitTool::description() const {
    return QStringLiteral("等待指定秒数，用于 UI 操作间等待界面加载。最大 60 秒。");
}

QVariantMap WaitTool::parametersSchema() const {
    return QVariantMap{
        { "type", "object" },
        { "properties", QVariantMap{
            { "seconds", QVariantMap{
                { "type", "number" },
                { "description", "等待秒数（支持小数，如 0.5）" }
            }}
        }},
        { "required", QVariantList{"seconds"} }
    };
}

QString WaitTool::execute(const QVariantMap &args) {
    double sec = args.value("seconds").toDouble();
    if (sec <= 0) sec = 0.5;
    if (sec > 60) sec = 60;
    int ms = static_cast<int>(sec * 1000);

    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();

    QJsonObject o;
    o["success"] = true;
    o["waited_seconds"] = sec;
    QString result = QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
    emit executionFinished(name(), result);
    return result;
}
