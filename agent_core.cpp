#include "agent_core.h"
#include "llm_backend.h"
#include "tool_registry.h"
#include "memory_module.h"
#include "skill_manager.h"
#include "web_search_service.h"
#include <QElapsedTimer>
#include <QRegularExpression>
#include <QTimer>

AgentCore::AgentCore(QObject *parent) : QObject(parent) {}

AgentCore::~AgentCore() {}

void AgentCore::setLLM(LLMBackend *llm) { m_llm = llm; }
void AgentCore::setTools(ToolRegistry *registry) { m_registry = registry; }
void AgentCore::setMemory(MemoryModule *memory) { m_memory = memory; }
void AgentCore::setSkillManager(SkillManager *sm) { m_skillManager = sm; }

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
        "你拥有文件读写、网页搜索、终端命令、剪贴板、记忆等物理操作权限。禁止推诿「无法操作」——不空想，用工具探测。\n\n"
        "## 执行循环（Sense → Think → Act → Memory）\n"
        "每一轮执行遵循以下闭环，直到任务完成：\n"
        "```\n"
        "用户指令\n"
        "    ↓\n"
        "感知（Sense）→ 思考（Think）→ 行动（Act）→ 记忆（Memory）\n"
        "    ↑_______________________________________|\n"
        "```\n"
        "1. **感知（Sense）**：任务开始时读取「已加载技能」和历史 SOP，了解是否有可复用的执行方案\n"
        "2. **思考（Think）**：在 <thinking> 内推演：当前阶段、上步结果是否符合预期、下步最优策略\n"
        "3. **行动（Act）**：选择工具执行，验证结果；失败时升级探测（1次读错误→2次探测状态→3次换方案）\n"
        "4. **记忆（Memory）**：任务成功后调用 `memory` action=save_sop 保存 SOP；系统将自动生成 Summary 技能\n\n"
        "## 行动原则\n"
        "- **探测优先**：失败时先充分获取信息（日志/状态/上下文），关键信息存入记忆，再决定重试或换方案。不可逆操作先询问用户。\n"
        "- **失败升级**：禁止无新信息的重复操作；每次失败必须获得新信息才能重试。\n"
        "- **立即执行**：需要执行动作时，必须实际调用工具，切勿仅描述计划而不调用。用用户语言回复。\n\n"
        "## 技能与记忆（自进化）\n"
        "**感知阶段**：优先参考「已加载技能」（见下方）和调用 `memory` action=recall_sops 检索已有 SOP，按已验证方案执行。\n"
        "**记忆阶段**：任务成功且流程可复用时，调用 `memory` action=save_sop 保存 SOP（name 简短如 xxx_sop，content 含关键步骤、前置条件、易踩坑点）。\n"
        "**失败时**：调用 `memory` action=add_lesson 记录教训（event_summary + lesson），避免重复犯错。\n"
        "**原则**：仅保存行动验证成功的信息；不存猜测、常识、易变状态；SOP 求精不求多。"
    );
}

// 感知阶段：从 SkillManager 中加载与当前任务关键词匹配的技能，注入到 Prompt
QString AgentCore::buildSkillContext() const {
    if (!m_skillManager || m_currentUserInput.isEmpty()) return QString();

    QVariantList allSkills = m_skillManager->skills();
    if (allSkills.isEmpty()) return QString();

    // 按空格、中文标点切词，长度 >= 2 的词作为关键词
    QStringList words = m_currentUserInput.split(
        QRegularExpression(QStringLiteral("[\\s,，。、！？!?；;:：\\-]+")), Qt::SkipEmptyParts);

    QStringList matched;
    for (const QVariant &v : allSkills) {
        QVariantMap skill = v.toMap();
        QString title   = skill["title"].toString();
        QString content = skill["content"].toString();

        bool hit = false;
        for (const QString &w : words) {
            if (w.length() >= 2 &&
                (title.contains(w, Qt::CaseInsensitive) ||
                 content.contains(w, Qt::CaseInsensitive))) {
                hit = true;
                break;
            }
        }
        if (hit)
            matched << QStringLiteral("### 技能：%1\n%2").arg(title, content);
    }

    if (matched.isEmpty()) return QString();

    return QStringLiteral("## 已加载技能（感知阶段 / Sense）\n"
                          "以下技能与当前任务相关，请优先参考执行：\n\n")
           + matched.join(QStringLiteral("\n\n---\n\n"));
}

QString AgentCore::buildSystemPrompt() const {
    QString base = m_systemPromptBase.trimmed().isEmpty()
        ? defaultAgentSystemPrompt()
        : m_systemPromptBase;

    // 感知阶段：注入与本次任务相关的技能
    QString skillCtx = buildSkillContext();
    if (!skillCtx.isEmpty())
        base += "\n\n" + skillCtx;

    // 注入长期记忆（L2 facts + L3 SOPs + 教训）
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
    m_autoSavePending = false;  // 取消上一次未完成的自动保存

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

    // 感知阶段（Sense）：buildSystemPrompt 内部注入技能上下文和记忆上下文
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
    emit finished();

    // 记忆阶段（Memory）：Agent 模式下至少调用过一次工具，说明是真实任务执行
    // 自动生成 Summary 并保存为技能，形成自进化闭环
    if (m_mode != "chat" && m_currentToolRound > 0
        && m_skillManager && m_llm && !m_currentUserInput.isEmpty()) {
        tryAutoSaveSkill(fullContent);
    }
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

            // 在 chatComplete 期间暂时断开 onLLMError，避免 chatComplete 失败时
            // errorOccurred 同时触发 lambda 和 onLLMError 导致双重处理
            disconnect(m_llm, &LLMBackend::errorOccurred, this, &AgentCore::onLLMError);

            connect(m_llm, &LLMBackend::completeReceived, this,
                [this, toolName, args](const QString &rewritten, const QString &) {
                    disconnect(m_llm, &LLMBackend::completeReceived, this, nullptr);
                    disconnect(m_llm, &LLMBackend::errorOccurred, this, nullptr);
                    connect(m_llm, &LLMBackend::errorOccurred, this, &AgentCore::onLLMError);
                    QString q = rewritten.trimmed();
                    if (q.length() > 500) q = q.left(500).trimmed();
                    doExecuteToolAndContinue(toolName, args, q.isEmpty() ? m_currentUserInput : q);
                }, Qt::SingleShotConnection);
            connect(m_llm, &LLMBackend::errorOccurred, this,
                [this, toolName, args, ctx](const QString &) {
                    disconnect(m_llm, &LLMBackend::completeReceived, this, nullptr);
                    disconnect(m_llm, &LLMBackend::errorOccurred, this, nullptr);
                    connect(m_llm, &LLMBackend::errorOccurred, this, &AgentCore::onLLMError);
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

    emit toolStarted(toolName);

    QTimer::singleShot(50, this, [this, toolName, args, actualArgs]() {
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

        // 截断工具结果，避免超大上下文导致 LLM 请求体过大（OOM / API 报错）
        // web_search 结果含完整网页正文，不截断可达 10000+ chars
        static const int kMaxToolResultChars = 4000;
        QString resultForContext = result;
        if (resultForContext.length() > kMaxToolResultChars)
            resultForContext = resultForContext.left(kMaxToolResultChars) + QStringLiteral("\n...[内容过长已截断]");

        QVariantMap toolMsg;
        toolMsg["role"] = "user";
        toolMsg["content"] = QString("[Tool %1 结果]\n%2").arg(toolName, resultForContext);
        m_pendingMessages.append(toolMsg);

        m_llm->chatStream(m_pendingMessages, buildSystemPrompt());
    });
}

void AgentCore::onLLMError(const QString &msg) {
    emit errorOccurred(msg);
    emit finished();
}

// 记忆阶段（Memory）：任务完成后自动生成 Summary 并保存为技能
// 参考 pc-agent-loop 的 post-execution summarization 模式
void AgentCore::tryAutoSaveSkill(const QString &finalContent) {
    if (m_autoSavePending) return;
    m_autoSavePending = true;

    // 构建摘要生成的上下文：取最近对话片段 + 最终回复
    QVariantList summaryMsgs;
    int start = qMax(0, m_pendingMessages.size() - 8);
    for (int i = start; i < m_pendingMessages.size(); ++i)
        summaryMsgs.append(m_pendingMessages[i]);

    if (!finalContent.trimmed().isEmpty()) {
        QVariantMap assistantMsg;
        assistantMsg["role"]    = "assistant";
        assistantMsg["content"] = finalContent;
        summaryMsgs.append(assistantMsg);
    }

    QVariantMap reqMsg;
    reqMsg["role"]    = "user";
    reqMsg["content"] = QStringLiteral(
        "根据以上对话，判断任务是否已成功完成并可复用。\n"
        "若成功，输出如下格式（不要其他内容）：\n"
        "TITLE: [技能名，简短英文或中文，如 git_deploy_skill]\n"
        "CONTENT: [关键步骤与注意事项，100~300字]\n\n"
        "若任务未成功或不值得保存，只输出：SKIP\n\n"
        "原始任务：") + m_currentUserInput;
    summaryMsgs.append(reqMsg);

    const QString sysPrompt = QStringLiteral(
        "你是一个任务经验提炼助手。严格按照要求输出，不要附加解释。"
        "只提炼已验证成功的可复用经验；失败的流程输出 SKIP。");

    // 防止 chatComplete 回调与主流程信号混淆：暂断主流程信号
    disconnect(m_llm, &LLMBackend::errorOccurred, this, &AgentCore::onLLMError);

    connect(m_llm, &LLMBackend::completeReceived, this,
        [this](const QString &summary, const QString &) {
            disconnect(m_llm, &LLMBackend::completeReceived, this, nullptr);
            disconnect(m_llm, &LLMBackend::errorOccurred,    this, nullptr);
            connect(m_llm, &LLMBackend::errorOccurred, this, &AgentCore::onLLMError);
            m_autoSavePending = false;

            QString s = summary.trimmed();
            if (s.isEmpty() || s.startsWith(QLatin1String("SKIP"), Qt::CaseInsensitive))
                return;

            // 解析 TITLE / CONTENT
            QString title, content;
            const QStringList lines = s.split('\n');
            bool inContent = false;
            for (const QString &line : lines) {
                if (!inContent && line.startsWith(QLatin1String("TITLE:"))) {
                    title = line.mid(6).trimmed();
                } else if (line.startsWith(QLatin1String("CONTENT:"))) {
                    content = line.mid(8).trimmed();
                    inContent = true;
                } else if (inContent) {
                    content += '\n' + line;
                }
            }
            content = content.trimmed();

            if (title.isEmpty() || content.isEmpty()) return;

            // 保存到 SkillManager（技能库，供 UI 展示与下次感知阶段加载）
            if (m_skillManager)
                m_skillManager->addSkill(title, content,
                                         QString::fromUtf8("\xF0\x9F\x93\x8B"), "learned");

            // 同步保存到 MemoryModule SOP 层（下次 recall_sops 可检索到）
            if (m_memory)
                m_memory->addSop(title, content, QStringLiteral("auto-summary"));

            emit skillSaved(title);
        }, Qt::SingleShotConnection);

    connect(m_llm, &LLMBackend::errorOccurred, this,
        [this](const QString &) {
            disconnect(m_llm, &LLMBackend::completeReceived, this, nullptr);
            disconnect(m_llm, &LLMBackend::errorOccurred,    this, nullptr);
            connect(m_llm, &LLMBackend::errorOccurred, this, &AgentCore::onLLMError);
            m_autoSavePending = false;
        }, Qt::SingleShotConnection);

    m_llm->chatComplete(summaryMsgs, sysPrompt);
}
