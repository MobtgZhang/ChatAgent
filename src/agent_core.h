#ifndef AGENT_CORE_H
#define AGENT_CORE_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class LLMBackend;
class ToolRegistry;
class MemoryModule;
class MessageModel;
class SkillManager;

/**
 * Agent 核心编排层（Sense → Think → Act → Memory 循环）
 * 用户指令 → 感知（加载技能/记忆）→ 思考（LLM 推演）→ 行动（工具执行）→ 记忆（保存 Summary）
 */
class AgentCore : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(int maxToolRounds READ maxToolRounds WRITE setMaxToolRounds NOTIFY maxToolRoundsChanged)
    // 模式: "chat" | "agent" | "planning"

public:
    explicit AgentCore(QObject *parent = nullptr);
    ~AgentCore();

    void setLLM(LLMBackend *llm);
    void setTools(ToolRegistry *registry);
    void setMemory(MemoryModule *memory);
    void setSkillManager(SkillManager *sm);
    void setSystemPromptBase(const QString &base);  // 来自 Settings，为空时用内置默认
    /** 与 UI 工作区一致：plan | agent | debug，影响系统提示与是否自动保存技能 */
    void setAgentUiMode(const QString &mode);
    /** 空列表表示允许全部工具；否则仅暴露所列工具名 */
    void setAllowedToolNames(const QStringList &names);

    QString mode() const { return m_mode; }
    QString agentUiMode() const { return m_agentUiMode; }
    void setMode(const QString &mode);
    int maxToolRounds() const { return m_maxToolRounds; }
    void setMaxToolRounds(int n);

    // 运行一轮 Agent 循环（用户消息 → Sense → Think → Act → Memory）
    Q_INVOKABLE void run(const QVariantList &messages, const QString &userInput);
    Q_INVOKABLE void stop();

signals:
    void chunkReceived(const QString &content, const QString &reasoning, bool isThinking);
    void toolStarted(const QString &toolName);
    void toolExecuted(const QString &toolName, const QVariantMap &args, const QString &result, double durationSec);
    void finished();
    void errorOccurred(const QString &msg);
    void modeChanged();
    void maxToolRoundsChanged();
    // 任务成功后自动保存 Summary 为技能时触发
    void skillSaved(const QString &skillTitle);

private slots:
    void onChunk(const QString &content, const QString &reasoning, bool isThinking);
    void onLLMFinished(const QString &fullContent);
    void onToolRequested(const QString &toolName, const QVariantMap &args);
    void onLLMError(const QString &msg);

private:
    // 感知阶段：构建系统 prompt（注入技能/记忆上下文）
    QString buildSystemPrompt() const;
    // 感知阶段：从 SkillManager 加载与当前任务相关的技能
    QString buildSkillContext() const;
    void doExecuteToolAndContinue(const QString &toolName, const QVariantMap &args, const QString &effectiveQuery);
    // 记忆阶段：任务完成后自动生成 Summary 并保存为技能
    void tryAutoSaveSkill(const QString &finalContent);

    LLMBackend    *m_llm = nullptr;
    ToolRegistry  *m_registry = nullptr;
    MemoryModule  *m_memory = nullptr;
    SkillManager  *m_skillManager = nullptr;

    QString m_mode = "agent";
    QString m_agentUiMode = QStringLiteral("agent");
    QStringList m_allowedToolNames;  // 空 = 不限制
    QString m_systemPromptBase;   // 用户自定义，空则用内置默认
    QVariantList m_pendingMessages;
    int m_maxToolRounds = 40;
    int m_currentToolRound = 0;
    QString m_currentUserInput;  // 当前用户输入，web_search 只用此 query 搜索
    bool m_autoSavePending = false; // 防止并发的自动保存
};

#endif // AGENT_CORE_H
