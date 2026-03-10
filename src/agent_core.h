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

/**
 * Agent 核心编排层（ReAct 循环）
 * 负责：意图识别 → 工具选择 → 执行 → 结果反思
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
    void setSystemPromptBase(const QString &base);  // 来自 Settings，为空时用内置默认

    QString mode() const { return m_mode; }
    void setMode(const QString &mode);
    int maxToolRounds() const { return m_maxToolRounds; }
    void setMaxToolRounds(int n);

    // 运行一轮 Agent 循环（用户消息 → 可能多轮工具调用 → 最终回复）
    Q_INVOKABLE void run(const QVariantList &messages, const QString &userInput);
    Q_INVOKABLE void stop();

signals:
    void chunkReceived(const QString &content, const QString &reasoning, bool isThinking);
    void toolExecuted(const QString &toolName, const QVariantMap &args, const QString &result, double durationSec);
    void finished();
    void errorOccurred(const QString &msg);
    void modeChanged();
    void maxToolRoundsChanged();

private slots:
    void onChunk(const QString &content, const QString &reasoning, bool isThinking);
    void onLLMFinished(const QString &fullContent);
    void onToolRequested(const QString &toolName, const QVariantMap &args);
    void onLLMError(const QString &msg);

private:
    QString buildSystemPrompt() const;
    void doExecuteToolAndContinue(const QString &toolName, const QVariantMap &args, const QString &effectiveQuery);

    LLMBackend    *m_llm = nullptr;
    ToolRegistry  *m_registry = nullptr;
    MemoryModule  *m_memory = nullptr;

    QString m_mode = "agent";
    QString m_systemPromptBase;   // 用户自定义，空则用内置默认
    QVariantList m_pendingMessages;
    int m_maxToolRounds = 40;
    int m_currentToolRound = 0;
    QString m_currentUserInput;  // 当前用户输入，web_search 只用此 query 搜索
};

#endif // AGENT_CORE_H
