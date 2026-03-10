#include "agent_core.h"
#include "llm_backend.h"
#include "tool_registry.h"
#include "memory_module.h"
#include "web_search_service.h"
#include <QElapsedTimer>

AgentCore::AgentCore(QObject *parent) : QObject(parent) {}

AgentCore::~AgentCore() {}

void AgentCore::setLLM(LLMBackend *llm) { m_llm = llm; }
void AgentCore::setTools(ToolRegistry *registry) { m_registry = registry; }
void AgentCore::setMemory(MemoryModule *memory) { m_memory = memory; }

void AgentCore::setMode(const QString &mode) {
    if (m_mode == mode) return;
    m_mode = mode;
    emit modeChanged();
}

void AgentCore::setSystemPromptBase(const QString &base) {
    m_systemPromptBase = base;
}

void AgentCore::setMaxToolRounds(int n) {
    n = qBound(5, n, 40);
    if (m_maxToolRounds == n) return;
    m_maxToolRounds = n;
    emit maxToolRoundsChanged();
}

static QString defaultAgentSystemPrompt() {
    return QStringLiteral(
        "# Role: 物理级全能执行者（自进化）\n"
        "你拥有文件读写、网页搜索、终端命令、剪贴板、记忆等物理操作权限。禁止推诿「无法操作」——不空想，用工具探测。\n"
        "## 行动原则\n"
        "调用工具前在 <thinking> 内推演：当前阶段、上步结果是否符合预期、下步策略。\n"
        "- **探测优先**：失败时先充分获取信息（日志/状态/上下文），关键信息存入记忆，再决定重试或换方案。不可逆操作先询问用户。\n"
        "- **失败升级**：1次→读错误理解原因，2次→探测环境状态，3次→深度分析后换方案或问用户。禁止无新信息的重复操作。\n"
        "- **立即执行**：需要执行动作时，必须实际调用工具，切勿仅描述计划而不调用。用用户语言回复。\n"
        "## 自进化（学习与记忆）\n"
        "**任务前**：接到任务先调用 `memory` action=recall_sops，检查是否已有相关 SOP；若有则 `memory` action=read_sop 读取后直接按 SOP 执行。\n"
        "**任务后**：任务成功且流程可复用时，调用 `memory` action=save_sop 保存 SOP（name 简短如 xxx_sop，content 含关键步骤、前置条件、易踩坑点）。\n"
        "**失败时**：遇到可归纳的失败模式，调用 `memory` action=add_lesson 记录教训（event_summary + lesson），避免重复犯错。\n"
        "**原则**：仅保存行动验证成功的信息；不存猜测、常识、易变状态；SOP 求精不求多。"
    );
}

QString AgentCore::buildSystemPrompt() const {
    QString base = m_systemPromptBase.trimmed().isEmpty()
        ? defaultAgentSystemPrompt()
        : m_systemPromptBase;

    if (m_memory) {
        QString ctx = m_memory->buildContextForPrompt();
        if (!ctx.isEmpty())
            base += "\n\n" + ctx;
    }

    if (m_mode == "planning") {
        base += "\n\n[Mode: Planning] Break complex tasks into steps. Plan first, then execute tools in order.";
    }

    return base;
}

void AgentCore::run(const QVariantList &messages, const QString &userInput) {
    m_currentToolRound = 0;
    m_pendingMessages = messages;
    m_currentUserInput = userInput.trimmed();

    // 添加用户消息
    QVariantMap userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = userInput;
    m_pendingMessages.append(userMsg);

    if (!m_llm) {
        emit errorOccurred("LLM backend not set");
        return;
    }

    m_llm->setTools(m_registry ? m_registry->toolsSchema() : QVariantList{});
    QString sysPrompt = buildSystemPrompt();

    disconnect(m_llm, nullptr, this, nullptr);
    connect(m_llm, &LLMBackend::chunkReceived, this, &AgentCore::onChunk);
    connect(m_llm, &LLMBackend::finished, this, &AgentCore::onLLMFinished);
    connect(m_llm, &LLMBackend::toolCallRequested, this, &AgentCore::onToolRequested);
    connect(m_llm, &LLMBackend::errorOccurred, this, &AgentCore::onLLMError);

    m_llm->chatStream(m_pendingMessages, sysPrompt);
}

void AgentCore::stop() {
    if (m_llm)
        m_llm->abort();
}

void AgentCore::onChunk(const QString &content, const QString &reasoning, bool isThinking) {
    emit chunkReceived(content, reasoning, isThinking);
}

void AgentCore::onLLMFinished(const QString &fullContent) {
    Q_UNUSED(fullContent);
    emit finished();
}

void AgentCore::onToolRequested(const QString &toolName, const QVariantMap &args) {
    if (!m_registry) return;

    // web_search：参考 MindSearch，History + Query Rewrite（LLM）→ 搜索
    if (toolName == QLatin1String("web_search") && !m_currentUserInput.isEmpty()) {
        QString ctx = WebSearchService::buildConversationContextFromMessages(m_pendingMessages, true);
        if (!ctx.isEmpty() && m_llm) {
            const QString sysPrompt = QStringLiteral(
                "你是一个搜索查询改写助手。根据「主问题」「历史对话」和「当前问题」，输出适合搜索引擎的查询。\n"
                "要求：1) 解决指代；2) 保持简洁；3) 多个子查询用分号分隔；4) 只输出查询内容，不要其他文字。");
            QVariantMap userMsg;
            userMsg["role"] = QStringLiteral("user");
            userMsg["content"] = ctx + QStringLiteral("\n\n当前问题：") + m_currentUserInput;
            QVariantList msgs;
            msgs.append(userMsg);
            connect(m_llm, &LLMBackend::completeReceived, this,
                [this, toolName, args](const QString &rewritten, const QString &) {
                    disconnect(m_llm, &LLMBackend::completeReceived, this, nullptr);
                    disconnect(m_llm, &LLMBackend::errorOccurred, this, nullptr);
                    QString q = rewritten.trimmed();
                    if (q.length() > 500) q = q.left(500).trimmed();
                    doExecuteToolAndContinue(toolName, args, q.isEmpty() ? m_currentUserInput : q);
                }, Qt::SingleShotConnection);
            connect(m_llm, &LLMBackend::errorOccurred, this,
                [this, toolName, args, ctx](const QString &) {
                    disconnect(m_llm, &LLMBackend::completeReceived, this, nullptr);
                    disconnect(m_llm, &LLMBackend::errorOccurred, this, nullptr);
                    QString fallback = WebSearchService::buildContextualSearchQuery(ctx, m_currentUserInput);
                    doExecuteToolAndContinue(toolName, args, fallback.isEmpty() ? m_currentUserInput : fallback);
                }, Qt::SingleShotConnection);
            m_llm->chatComplete(msgs, sysPrompt);
            return;
        }
    }

    // 非 web_search 或无需改写：直接执行
    QVariantMap actualArgs = args;
    if (toolName == QLatin1String("web_search") && !m_currentUserInput.isEmpty()) {
        QString ctx = WebSearchService::buildConversationContextFromMessages(m_pendingMessages, true);
        actualArgs["query"] = WebSearchService::buildContextualSearchQuery(ctx, m_currentUserInput);
    }
    doExecuteToolAndContinue(toolName, args, actualArgs["query"].toString());
}

void AgentCore::doExecuteToolAndContinue(const QString &toolName, const QVariantMap &args, const QString &effectiveQuery) {
    QVariantMap actualArgs = args;
    if (toolName == QLatin1String("web_search"))
        actualArgs["query"] = effectiveQuery;

    QElapsedTimer timer;
    timer.start();
    QString result = m_registry->execute(toolName, actualArgs);
    double durationSec = timer.nsecsElapsed() / 1e9;
    emit toolExecuted(toolName, args, result, durationSec);

    m_currentToolRound++;
    if (m_currentToolRound > m_maxToolRounds) {
        emit errorOccurred("工具调用轮次超限");
        emit finished();
        return;
    }

    QVariantMap toolMsg;
    toolMsg["role"] = "user";
    toolMsg["content"] = QString("[Tool %1 结果]\n%2").arg(toolName, result);
    m_pendingMessages.append(toolMsg);

    m_llm->chatStream(m_pendingMessages, buildSystemPrompt());
}

void AgentCore::onLLMError(const QString &msg) {
    emit errorOccurred(msg);
    emit finished();
}
