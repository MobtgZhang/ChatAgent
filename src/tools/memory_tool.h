#ifndef MEMORY_TOOL_H
#define MEMORY_TOOL_H

#include "base_tool.h"

class MemoryModule;

/**
 * 自进化记忆工具（参考 GenericAgent / OpenClaw）
 * 支持保存 SOP、召回 SOP、记录教训，实现智能体学习与记忆
 */
class MemoryTool : public BaseTool {
    Q_OBJECT

public:
    explicit MemoryTool(MemoryModule *memory, QObject *parent = nullptr);

    QString name() const override { return QStringLiteral("memory"); }
    QString description() const override;
    QVariantMap parametersSchema() const override;
    QString execute(const QVariantMap &args) override;

private:
    MemoryModule *m_memory = nullptr;
};

#endif // MEMORY_TOOL_H
