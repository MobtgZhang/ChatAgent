#include "keyboard_tool.h"
#include <QProcess>
#include <QJsonObject>
#include <QJsonDocument>

KeyboardTool::KeyboardTool(QObject *parent) : BaseTool(parent) {}

QString KeyboardTool::description() const {
    return QStringLiteral("模拟键盘输入：输入文本、按单键或组合键。支持 type/press/combo 三种操作。Linux 需安装 xdotool。");
}

QVariantMap KeyboardTool::parametersSchema() const {
    return QVariantMap{
        { "type", "object" },
        { "properties", QVariantMap{
            { "action", QVariantMap{
                { "type", "string" },
                { "enum", QVariantList{"type", "press", "combo"} },
                { "description", "操作：type 输入文本，press 按单键，combo 按组合键（如 ctrl+c）" }
            }},
            { "text", QVariantMap{
                { "type", "string" },
                { "description", "要输入的文本（type 操作时必填）" }
            }},
            { "key", QVariantMap{
                { "type", "string" },
                { "description", "按键名（press 时用，如 Return Tab Escape）" }
            }},
            { "keys", QVariantMap{
                { "type", "string" },
                { "description", "组合键（combo 时用，如 ctrl+c alt+Tab）" }
            }}
        }},
        { "required", QVariantList{"action"} }
    };
}

QString KeyboardTool::typeText(const QString &text) {
    if (text.isEmpty()) return QStringLiteral("{\"error\":\"文本为空\"}");
    QProcess proc;
    proc.start("xdotool", QStringList{"type", "--clearmodifiers", text});
    if (!proc.waitForFinished(5000)) {
        return QStringLiteral("{\"error\":\"xdotool 执行失败或未安装\"}");
    }
    if (proc.exitCode() != 0) {
        return QString("{\"error\":\"xdotool: %1\"}").arg(QString::fromUtf8(proc.readAllStandardError()));
    }
    return QStringLiteral("{\"success\":true}");
}

QString KeyboardTool::keyPress(const QString &key) {
    if (key.isEmpty()) return QStringLiteral("{\"error\":\"按键为空\"}");
    QProcess proc;
    proc.start("xdotool", QStringList{"key", key});
    if (!proc.waitForFinished(3000)) {
        return QStringLiteral("{\"error\":\"xdotool 执行失败或未安装\"}");
    }
    return proc.exitCode() == 0 ? QStringLiteral("{\"success\":true}") :
        QString("{\"error\":\"%1\"}").arg(QString::fromUtf8(proc.readAllStandardError()));
}

QString KeyboardTool::keyCombo(const QString &keys) {
    if (keys.isEmpty()) return QStringLiteral("{\"error\":\"组合键为空\"}");
    QProcess proc;
    proc.start("xdotool", QStringList{"key", keys});
    if (!proc.waitForFinished(3000)) {
        return QStringLiteral("{\"error\":\"xdotool 执行失败或未安装\"}");
    }
    return proc.exitCode() == 0 ? QStringLiteral("{\"success\":true}") :
        QString("{\"error\":\"%1\"}").arg(QString::fromUtf8(proc.readAllStandardError()));
}

QString KeyboardTool::execute(const QVariantMap &args) {
    QString action = args.value("action").toString();
    QString result;
    if (action == "type") {
        result = typeText(args.value("text").toString());
    } else if (action == "press") {
        result = keyPress(args.value("key").toString());
    } else if (action == "combo") {
        result = keyCombo(args.value("keys").toString());
    } else {
        result = QStringLiteral("{\"error\":\"未知操作: %1\"}").arg(action);
    }
    emit executionFinished(name(), result);
    return result;
}
