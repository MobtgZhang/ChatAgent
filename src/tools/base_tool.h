#ifndef BASE_TOOL_H
#define BASE_TOOL_H

#include <QObject>
#include <QString>
#include <QVariantMap>

/**
 * 工具抽象基类
 * 所有 Agent 工具均继承此类，实现 execute 与 schema
 */
class BaseTool : public QObject {
    Q_OBJECT

public:
    explicit BaseTool(QObject *parent = nullptr);

    // 工具唯一标识
    virtual QString name() const = 0;
    // 工具描述（供 LLM 选择时参考）
    virtual QString description() const = 0;
    // OpenAI 函数调用风格参数 schema（JSON 对象）
    virtual QVariantMap parametersSchema() const = 0;

    // 执行工具，返回 JSON 字符串结果
    Q_INVOKABLE virtual QString execute(const QVariantMap &args) = 0;

signals:
    void executionStarted(const QString &toolName);
    void executionFinished(const QString &toolName, const QString &result);
};

#endif // BASE_TOOL_H
