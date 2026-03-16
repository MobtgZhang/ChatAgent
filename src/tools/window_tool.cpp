#include "window_tool.h"
#include <QProcess>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

WindowTool::WindowTool(QObject *parent) : BaseTool(parent) {}

QString WindowTool::description() const {
    return QStringLiteral("窗口管理：列出所有窗口、按标题激活窗口、获取当前活动窗口。Linux 需 wmctrl 和 xdotool。");
}

QVariantMap WindowTool::parametersSchema() const {
    return QVariantMap{
        { "type", "object" },
        { "properties", QVariantMap{
            { "action", QVariantMap{
                { "type", "string" },
                { "enum", QVariantList{"list", "activate", "active"} },
                { "description", "操作：list 列窗口，activate 激活指定窗口，active 获取当前活动窗口" }
            }},
            { "title", QVariantMap{
                { "type", "string" },
                { "description", "窗口标题或部分匹配字符串（activate 时用）" }
            }}
        }},
        { "required", QVariantList{"action"} }
    };
}

QString WindowTool::listWindows() {
    QProcess proc;
    proc.start("wmctrl", QStringList{"-l", "-G"});
    if (!proc.waitForFinished(3000)) {
        return QStringLiteral("{\"error\":\"wmctrl 执行失败或未安装，请安装: sudo apt install wmctrl\"}");
    }
    QString out = QString::fromUtf8(proc.readAllStandardOutput());
    QJsonArray arr;
    for (const QString &line : out.split('\n', Qt::SkipEmptyParts)) {
        QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        if (parts.size() >= 8) {
            QJsonObject w;
            w["id"] = parts[0];
            w["desktop"] = parts[1].toInt();
            w["x"] = parts[2].toInt();
            w["y"] = parts[3].toInt();
            w["width"] = parts[4].toInt();
            w["height"] = parts[5].toInt();
            w["client"] = parts[6];
            w["title"] = parts.mid(7).join(' ');
            arr.append(w);
        }
    }
    QJsonObject o;
    o["windows"] = arr;
    o["success"] = true;
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

QString WindowTool::activateWindow(const QString &pattern) {
    if (pattern.isEmpty()) return QStringLiteral("{\"error\":\"请提供窗口标题\"}");
    QProcess proc;
    proc.start("wmctrl", QStringList{"-a", pattern});
    if (!proc.waitForFinished(3000)) {
        return QStringLiteral("{\"error\":\"wmctrl 执行失败或未安装\"}");
    }
    return proc.exitCode() == 0 ? QStringLiteral("{\"success\":true}") :
        QStringLiteral("{\"error\":\"未找到匹配窗口\"}");
}

QString WindowTool::getActiveWindow() {
    QProcess proc;
    proc.start("xdotool", QStringList{"getactivewindow", "getwindowname"});
    if (!proc.waitForFinished(2000)) {
        return QStringLiteral("{\"error\":\"xdotool 执行失败\"}");
    }
    QJsonObject o;
    o["title"] = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    o["success"] = true;
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

QString WindowTool::execute(const QVariantMap &args) {
    QString action = args.value("action").toString();
    QString title = args.value("title").toString();
    QString result;
    if (action == "list") {
        result = listWindows();
    } else if (action == "activate") {
        result = activateWindow(title);
    } else if (action == "active") {
        result = getActiveWindow();
    } else {
        result = QStringLiteral("{\"error\":\"未知操作: %1\"}").arg(action);
    }
    emit executionFinished(name(), result);
    return result;
}
