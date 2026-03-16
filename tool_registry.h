#ifndef TOOL_REGISTRY_H
#define TOOL_REGISTRY_H

#include <QObject>
#include <QMap>
#include <QVariantMap>
#include <QVariantList>

class BaseTool;

/**
 * 工具注册与分发中心
 * 管理所有 Agent 工具的注册、查询和调用
 */
class ToolRegistry : public QObject {
    Q_OBJECT

public:
    explicit ToolRegistry(QObject *parent = nullptr);

    // 注册工具
    void registerTool(BaseTool *tool);
    // 根据名称执行工具
    Q_INVOKABLE QString execute(const QString &toolName, const QVariantMap &args);
    // 获取所有工具的 OpenAI 函数格式 schema（用于 API 调用）
    QVariantList toolsSchema() const;
    // 获取工具名称列表
    Q_INVOKABLE QStringList toolNames() const;

signals:
    void toolExecuted(const QString &toolName, const QString &result);

private:
    QMap<QString, BaseTool *> m_tools;
};

#endif // TOOL_REGISTRY_H
