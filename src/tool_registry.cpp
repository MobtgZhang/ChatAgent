#include "tool_registry.h"
#include "tools/base_tool.h"
#include <QJsonObject>
#include <QJsonDocument>

ToolRegistry::ToolRegistry(QObject *parent) : QObject(parent) {}

void ToolRegistry::registerTool(BaseTool *tool) {
    if (!tool) return;
    m_tools[tool->name()] = tool;
}

QString ToolRegistry::execute(const QString &toolName, const QVariantMap &args) {
    BaseTool *t = m_tools.value(toolName);
    if (!t) {
        return QStringLiteral("{\"error\":\"未知工具: %1\"}").arg(toolName);
    }
    QString result = t->execute(args);
    emit toolExecuted(toolName, result);
    return result;
}

QVariantList ToolRegistry::toolsSchema() const {
    QVariantList list;
    for (BaseTool *t : m_tools) {
        if (!t) continue;
        QVariantMap m;
        m["type"] = "function";
        m["function"] = QVariantMap{
            { "name", t->name() },
            { "description", t->description() },
            { "parameters", t->parametersSchema() }
        };
        list.append(m);
    }
    return list;
}

QVariantList ToolRegistry::toolsSchemaAllowNames(const QStringList &allowedNames) const {
    QVariantList list;
    for (BaseTool *t : m_tools) {
        if (!t) continue;
        if (!allowedNames.contains(t->name()))
            continue;
        QVariantMap m;
        m["type"] = "function";
        m["function"] = QVariantMap{
            { "name", t->name() },
            { "description", t->description() },
            { "parameters", t->parametersSchema() }
        };
        list.append(m);
    }
    return list;
}

QStringList ToolRegistry::toolNames() const {
    return m_tools.keys();
}
