#include "clipboard_tool.h"
#include <QGuiApplication>
#include <QClipboard>
#include <QJsonObject>
#include <QJsonDocument>

ClipboardTool::ClipboardTool(QObject *parent) : BaseTool(parent) {}

QString ClipboardTool::description() const {
    return QStringLiteral("读取或写入系统剪贴板内容。支持 read/write 操作。");
}

QVariantMap ClipboardTool::parametersSchema() const {
    return QVariantMap{
        { "type", "object" },
        { "properties", QVariantMap{
            { "action", QVariantMap{
                { "type", "string" },
                { "enum", QVariantList{"read", "write"} },
                { "description", "操作：read 读取剪贴板，write 写入剪贴板" }
            }},
            { "content", QVariantMap{
                { "type", "string" },
                { "description", "要写入的内容（write 时必填）" }
            }}
        }},
        { "required", QVariantList{"action"} }
    };
}

QString ClipboardTool::readClipboard() {
    QClipboard *cb = QGuiApplication::clipboard();
    QString text = cb->text();
    QJsonObject o;
    o["content"] = text;
    o["success"] = true;
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

QString ClipboardTool::writeClipboard(const QString &content) {
    QClipboard *cb = QGuiApplication::clipboard();
    cb->setText(content);
    return QStringLiteral("{\"success\":true}");
}

QString ClipboardTool::execute(const QVariantMap &args) {
    QString action = args.value("action").toString();
    QString result;
    if (action == "read") {
        result = readClipboard();
    } else if (action == "write") {
        result = writeClipboard(args.value("content").toString());
    } else {
        result = QStringLiteral("{\"error\":\"未知操作: %1\"}").arg(action);
    }
    emit executionFinished(name(), result);
    return result;
}
