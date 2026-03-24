#include "mainview.h"
#include "settings.h"
#include "history.h"
#include "agent_core.h"
#include "llm_backend.h"
#include "memory_module.h"
#include "skill_manager.h"
#include "tool_registry.h"
#include "web_search_service.h"
#include "tools/file_tool.h"
#include "tools/shell_tool.h"
#include "tools/websearch_tool.h"
#include "tools/keyboard_tool.h"
#include "tools/window_tool.h"
#include "tools/clipboard_tool.h"
#include "tools/wait_tool.h"
#include "tools/python_tool.h"
#include "tools/memory_tool.h"

#include <QGuiApplication>
#include <QClipboard>
#include <QFile>
#include <QTextStream>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QDebug>
#include <QDateTime>
#include <QTimer>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QMap>

namespace {
QString normalizeChatMode(QString m) {
    m = m.trimmed().toLower();
    if (m == QLatin1String("chat"))
        return QStringLiteral("ask");
    if (m == QLatin1String("planning"))
        return QStringLiteral("plan");
    if (m == QLatin1String("ask") || m == QLatin1String("plan")
        || m == QLatin1String("agent") || m == QLatin1String("debug"))
        return m;
    return QStringLiteral("ask");
}
} // namespace

// ── 构造 ──────────────────────────────────────────────────────────────────────
MainView::MainView(Settings *settings, History *history, QObject *parent)
    : QObject(parent), m_settings(settings), m_history(history)
{
    m_nam = new QNetworkAccessManager(this);
    setupAgent();

    // 启动时加载或新建默认会话
    QString firstId = m_history->firstSessionId();
    if (firstId.isEmpty()) {
        // 默认标题：新标题(模型名)
        newSession(QString());
    } else {
        loadSession(firstId);
    }
}

// ── 工具 ──────────────────────────────────────────────────────────────────────
void MainView::setIsStreaming(bool v) {
    if (m_isStreaming == v) return;
    m_isStreaming = v;
    emit isStreamingChanged();
}

void MainView::appendMessage(const QVariantMap &msg) {
    m_messagesModel.appendOne(msg);
}

// ── 会话管理 ──────────────────────────────────────────────────────────────────
void MainView::newSession(const QString &name, bool addWelcome) {
    if (m_activeReply) m_activeReply->abort();

    QString sessionName = name.isEmpty()
            ? QStringLiteral("新标题(%1)").arg(m_settings->modelName())
            : name;
    QString id = m_history->createSession(sessionName);

    m_currentSession = id;
    m_sessionName    = sessionName;
    m_messagesModel.clearAll();

    // 可选：添加欢迎消息
    if (addWelcome) {
        QVariantMap welcome;
        welcome["role"]      = "ai";
        welcome["content"]   = QString("你好！我是 **%1**，有什么可以帮助你的吗？")
                               .arg(m_settings->modelName());
        welcome["thinking"]  = "";
        welcome["isThinking"]= false;
        welcome["id"]        = "welcome";
        m_messagesModel.appendOne(welcome);
    }
    emit currentSessionChanged();
    emit sessionNameChanged();
    saveCurrentSession();
}

void MainView::loadSession(const QString &id) {
    if (m_activeReply) m_activeReply->abort();
    loadSessionFile(id);
    m_currentSession = id;
    emit currentSessionChanged();
}

void MainView::clearMessages() {
    // 清空 = 删除当前会话历史 + 新建空对话（无欢迎消息）
    if (!m_currentSession.isEmpty())
        m_history->deleteNode(m_currentSession);
    newSession(QString(), false);  // 不添加"你好XXX"欢迎消息
}

void MainView::setChatMode(const QString &mode) {
    const QString m = normalizeChatMode(mode);
    if (m_chatMode == m) return;
    m_chatMode = m;
    emit chatModeChanged();
    applyLlmForCurrentChatMode();
    saveCurrentSession();
}

void MainView::applyLlmForCurrentChatMode() {
    if (!m_llmBackend || !m_settings) return;
    const QString wm = normalizeChatMode(m_chatMode);
    m_llmBackend->setModel(m_settings->effectiveModelForChatMode(wm));
    m_llmBackend->setChatRequestParams(
        m_settings->effectiveTemperatureForChatMode(wm),
        m_settings->maxTokens());
}

void MainView::configureAgentForChatMode() {
    if (!m_agentCore) return;
    const QString cm = normalizeChatMode(m_chatMode);
    if (cm == QLatin1String("plan")) {
        m_agentCore->setAgentUiMode(QStringLiteral("plan"));
        m_agentCore->setAllowedToolNames(
            {QStringLiteral("file"), QStringLiteral("web_search"),
             QStringLiteral("memory"), QStringLiteral("wait")});
        m_agentCore->setMode(QStringLiteral("planning"));
    } else if (cm == QLatin1String("debug")) {
        m_agentCore->setAgentUiMode(QStringLiteral("debug"));
        QStringList allow{QStringLiteral("file"), QStringLiteral("web_search"),
                          QStringLiteral("memory"), QStringLiteral("wait")};
        if (m_settings && m_settings->debugAllowShell()) {
            allow << QStringLiteral("shell") << QStringLiteral("python");
        }
        m_agentCore->setAllowedToolNames(allow);
        m_agentCore->setMode(QStringLiteral("agent"));
    } else {
        m_agentCore->setAgentUiMode(QStringLiteral("agent"));
        m_agentCore->setAllowedToolNames(QStringList());
        m_agentCore->setMode(QStringLiteral("agent"));
    }
}

void MainView::setupAgent() {
    m_agentMemory = new MemoryModule(m_settings, this);

    LLMBackend *llm = new LLMBackend(this);
    m_llmBackend = llm;
    llm->setApiKey(m_settings->apiKey());
    llm->setApiUrl(m_settings->apiUrl());
    llm->setChatRequestParams(m_settings->temperature(), m_settings->maxTokens());
    connect(m_settings, &Settings::apiKeyChanged, this, [this, llm]() { llm->setApiKey(m_settings->apiKey()); });
    connect(m_settings, &Settings::apiUrlChanged, this, [this, llm]() { llm->setApiUrl(m_settings->apiUrl()); });
    auto syncLlm = [this]() { applyLlmForCurrentChatMode(); };
    connect(m_settings, &Settings::modelNameChanged, this, syncLlm);
    connect(m_settings, &Settings::modelNameAskChanged, this, syncLlm);
    connect(m_settings, &Settings::modelNamePlanChanged, this, syncLlm);
    connect(m_settings, &Settings::modelNameAgentChanged, this, syncLlm);
    connect(m_settings, &Settings::modelNameDebugChanged, this, syncLlm);
    connect(m_settings, &Settings::temperatureChanged, this, syncLlm);
    connect(m_settings, &Settings::temperatureAskChanged, this, syncLlm);
    connect(m_settings, &Settings::temperaturePlanChanged, this, syncLlm);
    connect(m_settings, &Settings::temperatureAgentChanged, this, syncLlm);
    connect(m_settings, &Settings::temperatureDebugChanged, this, syncLlm);
    connect(m_settings, &Settings::maxTokensChanged, this, syncLlm);

    ToolRegistry *reg = new ToolRegistry(this);
    m_toolRegistry = reg;
    reg->registerTool(new FileTool(m_settings, reg));
    reg->registerTool(new ShellTool(reg));
    reg->registerTool(new WebSearchTool(m_settings, reg));
    reg->registerTool(new KeyboardTool(reg));
    reg->registerTool(new WindowTool(reg));
    reg->registerTool(new ClipboardTool(reg));
    reg->registerTool(new WaitTool(reg));
    reg->registerTool(new PythonTool(m_settings, reg));
    reg->registerTool(new MemoryTool(m_agentMemory, reg));

    m_agentCore = new AgentCore(this);
    m_agentCore->setLLM(llm);
    m_agentCore->setTools(reg);
    m_agentCore->setMemory(m_agentMemory);
    m_agentCore->setSystemPromptBase(m_settings->systemPrompt());
    m_agentCore->setMaxToolRounds(m_settings->maxToolRounds());
    connect(m_settings, &Settings::systemPromptChanged, this, [this]() { m_agentCore->setSystemPromptBase(m_settings->systemPrompt()); });
    connect(m_settings, &Settings::maxToolRoundsChanged, this, [this]() { m_agentCore->setMaxToolRounds(m_settings->maxToolRounds()); });
    connect(m_settings, &Settings::debugAllowShellChanged, this, [this]() {
        if (normalizeChatMode(m_chatMode) == QLatin1String("debug"))
            configureAgentForChatMode();
    });

    connect(m_agentCore, &AgentCore::chunkReceived, this, [this](const QString &content, const QString &reasoning, bool isThinking) {
        if (normalizeChatMode(m_chatMode) != QLatin1String("ask"))
            m_messagesModel.updateLastAiMessageAgent(reasoning, content, isThinking);
        else
            updateLastAiMessage(reasoning, content, isThinking);
    });
    connect(m_agentCore, &AgentCore::toolStarted, this, [this](const QString &toolName) {
        if (m_messagesModel.isEmpty()) return;
        const int row = m_messagesModel.size() - 1;
        QVariantMap last = m_messagesModel.at(row);
        if (last.value("role").toString() != QLatin1String("ai")) return;
        last["pendingToolName"] = toolName;
        last["pendingToolStartMs"] = QDateTime::currentMSecsSinceEpoch();
        m_messagesModel.replaceAtRow(row, last);
    });
    connect(m_agentCore, &AgentCore::toolExecuted, this, [this](const QString &toolName, const QVariantMap &args, const QString &result, double durationSec) {
        if (!m_messagesModel.isEmpty()) {
            const int row = m_messagesModel.size() - 1;
            QVariantMap last = m_messagesModel.at(row);
            last.remove("pendingToolName");
            last.remove("pendingToolStartMs");
            m_messagesModel.replaceAtRow(row, last);
        }
        m_messagesModel.appendToolBlockToLastAiMessage(toolName, args, result, durationSec);

        if (normalizeChatMode(m_chatMode) != QLatin1String("ask")) {
            if (!m_messagesModel.isEmpty()) {
                const int row = m_messagesModel.size() - 1;
                QVariantMap last = m_messagesModel.at(row);
                if (last.value("role").toString() == QLatin1String("ai")) {
                    last["isThinking"] = false;
                    m_messagesModel.replaceAtRow(row, last);
                }
            }
            QVariantMap aiMsg;
            aiMsg["role"]      = "ai";
            aiMsg["content"]   = "";
            aiMsg["thinking"]  = "";
            aiMsg["isThinking"]= true;
            aiMsg["blocks"]    = QVariantList{};
            aiMsg["id"]        = QString::number(QDateTime::currentMSecsSinceEpoch());
            m_messagesModel.appendOne(aiMsg);
        }
    });
    connect(m_agentCore, &AgentCore::finished, this, [this]() {
        setIsStreaming(false);

        if (normalizeChatMode(m_chatMode) != QLatin1String("ask") && !m_messagesModel.isEmpty()) {
            const int row = m_messagesModel.size() - 1;
            QVariantMap last = m_messagesModel.at(row);
            if (last.value("role").toString() == QLatin1String("ai")) {
                const QVariantList blocks = last.value("blocks").toList();
                bool hasContent = !last.value("content").toString().isEmpty()
                               || !last.value("thinking").toString().isEmpty();
                bool hasNonEmptyBlocks = false;
                for (const QVariant &b : blocks) {
                    const QVariantMap bm = b.toMap();
                    if (!bm.value("content").toString().isEmpty()
                        || bm.value("type").toString() == QLatin1String("tool")) {
                        hasNonEmptyBlocks = true;
                        break;
                    }
                }
                if (!hasContent && !hasNonEmptyBlocks) {
                    m_messagesModel.removeAtRow(row);
                } else {
                    last["isThinking"] = false;
                    m_messagesModel.replaceAtRow(row, last);
                }
            }
        }

        const QString sessionId = m_currentSession;
        QTimer::singleShot(0, this, [this, sessionId]() {
            if (m_currentSession != sessionId) return;
            autoNameCurrentSession();
            saveCurrentSession();
            m_history->touchSession(sessionId);
        });
    });
    connect(m_agentCore, &AgentCore::errorOccurred, this, &MainView::errorOccurred);

    applyLlmForCurrentChatMode();

    connect(m_agentCore, &AgentCore::skillSaved, this, [this](const QString &title) {
        if (m_messagesModel.isEmpty()
            || m_messagesModel.at(m_messagesModel.size() - 1).value("role").toString() != QLatin1String("ai")) {
            QVariantMap aiMsg;
            aiMsg["role"]      = "ai";
            aiMsg["content"]   = "";
            aiMsg["thinking"]  = "";
            aiMsg["isThinking"]= false;
            aiMsg["blocks"]    = QVariantList{};
            aiMsg["id"]        = QString::number(QDateTime::currentMSecsSinceEpoch());
            m_messagesModel.appendOne(aiMsg);
        }
        m_messagesModel.appendToolBlockToLastAiMessage(
            QStringLiteral("skill_saved"),
            {{ QStringLiteral("title"), title }},
            QString::fromUtf8("\xF0\x9F\x93\x8B \xe6\x8a\x80\xe8\x83\xbd\xe5\xb7\xb2\xe5\xad\xa6\xe4\xb9\xa0\xef\xbc\x9a") + title,
            0.0);
    });
}

void MainView::setSkillManager(SkillManager *sm) {
    m_skillManager = sm;
    if (m_agentCore)
        m_agentCore->setSkillManager(sm);
}

void MainView::renameSession(const QString &name) {
    if (name.trimmed().isEmpty()) return;
    m_sessionName = name.trimmed();
    m_history->renameNode(m_currentSession, m_sessionName);
    emit sessionNameChanged();
}

bool MainView::exportCurrentChat(const QString &filePath) {
    if (filePath.isEmpty()) return false;
    QString path = filePath;
    if (path.startsWith(QLatin1String("file:")))
        path = QUrl(path).toLocalFile();
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;

    QTextStream out(&f);

    out << "# " << m_sessionName << "\n\n";

    const QVariantList list = m_messagesModel.toVariantList();
    int aiRoundCount = 0;
    for (const QVariant &v : list) {
        QVariantMap m = v.toMap();
        QString role = m["role"].toString();
        QString content = m["content"].toString().trimmed();
        QString thinking = m["thinking"].toString().trimmed();

        if (role == "user") {
            out << "## 用户\n\n" << content << "\n\n";
        } else if (role == "ai") {
            aiRoundCount++;
            out << "## 助手\n\n";
            QVariantList blocks = m["blocks"].toList();
            if (!blocks.isEmpty()) {
                QString roundPrefix = QString("（智能体 第%1轮）").arg(aiRoundCount);
                for (const QVariant &bv : blocks) {
                    QVariantMap b = bv.toMap();
                    QString t = b["type"].toString();
                    QString c = b["content"].toString();
                    if (t == "thinking" && !c.isEmpty())
                        out << "### " << roundPrefix << "思考过程\n\n" << c << "\n\n";
                    else if (t == "reply" && !c.isEmpty())
                        out << "### 回复\n\n" << c << "\n\n";
                    else if (t == "tool") {
                        out << "### " << roundPrefix << "工具: " << b["toolName"].toString() << "\n\n";
                        out << "结果:\n```\n" << b["result"].toString() << "\n```\n\n";
                    }
                }
            } else {
                if (!thinking.isEmpty())
                    out << "### 思考过程\n\n" << thinking << "\n\n";
                out << content << "\n\n";
            }
        }
    }

    out.flush();
    f.close();
    return f.error() == QFile::NoError;
}

// ── 消息编辑 ──────────────────────────────────────────────────────────────────
void MainView::editMessage(int index, const QString &content) {
    if (index < 0 || index >= m_messagesModel.size()) return;
    QVariantMap msg = m_messagesModel.at(index);
    msg["content"] = content.trimmed();
    m_messagesModel.replaceAtRow(index, msg);
    saveCurrentSession();
}

void MainView::editAndRegenerate(int index, const QString &content) {
    if (index < 0 || index >= m_messagesModel.size() || m_isStreaming) return;
    QVariantMap msg = m_messagesModel.at(index);
    const QString role = msg["role"].toString();
    const QString newContent = content.trimmed();

    if (role == QLatin1String("user")) {
        if (newContent != msg["content"].toString())
            m_messagesModel.appendEditHistoryNode(index, newContent, -1);
        else
            m_messagesModel.truncateTo(index + 1);
    } else {
        msg["content"] = newContent;
        m_messagesModel.replaceAtRow(index, msg);
    }

    if (role == QLatin1String("user")) {
        if (normalizeChatMode(m_chatMode) == QLatin1String("ask")) {
            if (m_settings->chatOnline()) {
                QVariantMap aiMsg;
                aiMsg["role"]         = "ai";
                aiMsg["content"]       = "";
                aiMsg["thinking"]      = "";
                aiMsg["isThinking"]    = true;
                aiMsg["ragSearchStatus"] = "searching";
                aiMsg["id"]            = QString::number(QDateTime::currentMSecsSinceEpoch());
                m_messagesModel.appendOne(aiMsg);
                setIsStreaming(true);
                startRagFetchAndApiCall(newContent);
            } else {
                startApiCall(m_messagesModel.toVariantList(), QString());
            }
        } else {
            startAgentCall(newContent);
        }
    } else {
        // AI 消息：仅保存修改，不重新生成
        saveCurrentSession();
    }
}

void MainView::setUserMessageVersion(int index, int dfsPosition) {
    if (index < 0 || index >= m_messagesModel.size()) return;
    m_messagesModel.setUserMessageEditIndex(index, dfsPosition);
    saveCurrentSession();
}

void MainView::deleteMessage(int index) {
    // 只允许删除用户消息；并且删除该条及其之后的所有消息
    if (index < 0 || index >= m_messagesModel.size()) return;

    const QVariantMap msg = m_messagesModel.at(index);
    if (msg.value("role").toString() != QLatin1String("user"))
        return;

    // 截断到 index（不含 index 本身），等价于删除当前及其之后的所有消息
    m_messagesModel.truncateTo(index);
    saveCurrentSession();
}

void MainView::copyToClipboard(const QString &text) {
    if (text.isEmpty()) return;
    QClipboard *cb = QGuiApplication::clipboard();
    if (cb) cb->setText(text);
}

void MainView::resendFrom(int index) {
    // 截断到 index（不含），然后重新发送
    if (index < 0 || index >= m_messagesModel.size()) return;
    m_messagesModel.truncateTo(index);

    // 找最后一条 user 消息重发
    for (int i = m_messagesModel.size() - 1; i >= 0; --i) {
        QVariantMap msg = m_messagesModel.at(i);
        if (msg["role"].toString() == "user") {
            QString userContent = msg["content"].toString();
            if (normalizeChatMode(m_chatMode) == QLatin1String("ask")) {
                if (m_settings->chatOnline()) {
                    QVariantMap aiMsg;
                    aiMsg["role"]         = "ai";
                    aiMsg["content"]      = "";
                    aiMsg["thinking"]     = "";
                    aiMsg["isThinking"]   = true;
                    aiMsg["ragSearchStatus"] = "searching";
                    aiMsg["id"]           = QString::number(QDateTime::currentMSecsSinceEpoch());
                    m_messagesModel.appendOne(aiMsg);
                    setIsStreaming(true);
                    startRagFetchAndApiCall(userContent);
                } else {
                    startApiCall(m_messagesModel.toVariantList(), QString());
                }
            } else {
                startAgentCall(userContent);
            }
            return;
        }
    }
}

// ── 发送消息 ──────────────────────────────────────────────────────────────────
void MainView::sendMessage(const QString &text) {
    if (text.trimmed().isEmpty() || m_isStreaming) return;

    // 添加用户消息
    QVariantMap userMsg;
    userMsg["role"]      = "user";
    userMsg["content"]   = text.trimmed();
    userMsg["thinking"]  = "";
    userMsg["isThinking"]= false;
    userMsg["id"]        = QString::number(QDateTime::currentMSecsSinceEpoch());
    m_messagesModel.appendOne(userMsg);

    // 参考 ChatOllama/GitHub 策略：首条用户消息时立即用其内容做临时标题(≤20字)
    int userCount = 0;
    for (int i = 0; i < m_messagesModel.size(); ++i) {
        if (m_messagesModel.at(i).value("role").toString() == QLatin1String("user"))
            ++userCount;
    }
    if ((m_sessionName.startsWith(QStringLiteral("新标题("))
         || m_sessionName == QStringLiteral("新标题")
         || m_sessionName == QStringLiteral("新对话"))
        && userCount == 1) {
        QString tmp = text.trimmed();
        if (tmp.length() > 20) tmp = tmp.left(20) + QStringLiteral("…");
        if (!tmp.isEmpty()) {
            m_sessionName = tmp;
            m_history->renameNode(m_currentSession, m_sessionName);
            emit sessionNameChanged();
        }
    }

    if (normalizeChatMode(m_chatMode) == QLatin1String("ask")) {
        if (m_settings->chatOnline()) {
            // 联网：先添加 AI 占位并显示「搜索中」，异步 RAG 完成后显示「搜索结束」再发起 API
            QVariantMap aiMsg;
            aiMsg["role"]         = "ai";
            aiMsg["content"]      = "";
            aiMsg["thinking"]     = "";
            aiMsg["isThinking"]   = true;
            aiMsg["ragSearchStatus"] = "searching";
            aiMsg["id"]           = QString::number(QDateTime::currentMSecsSinceEpoch());
            m_messagesModel.appendOne(aiMsg);
            setIsStreaming(true);
            startRagFetchAndApiCall(text.trimmed());
        } else {
            startApiCall(m_messagesModel.toVariantList(), QString());
        }
    } else {
        startAgentCall(text.trimmed());
    }
}

// ── 停止生成 ──────────────────────────────────────────────────────────────────
void MainView::startAgentCall(const QString &userInput) {
    QVariantMap aiMsg;
    aiMsg["role"]      = "ai";
    aiMsg["content"]   = "";
    aiMsg["thinking"]  = "";
    aiMsg["isThinking"]= true;
    aiMsg["blocks"]    = QVariantList{};
    aiMsg["id"]        = QString::number(QDateTime::currentMSecsSinceEpoch());
    m_messagesModel.appendOne(aiMsg);
    setIsStreaming(true);

    applyLlmForCurrentChatMode();
    configureAgentForChatMode();

    QVariantList msgs;
    const QVariantList full = m_messagesModel.toVariantList();
    for (int i = 0; i < full.size() - 1; ++i) {
        msgs.append(full[i]);
    }
    m_agentCore->run(msgs, userInput);
}

void MainView::stopGeneration() {
    if (m_agentCore && normalizeChatMode(m_chatMode) != QLatin1String("ask"))
        m_agentCore->stop();
    if (m_activeReply) {
        m_activeReply->abort();
        m_activeReply = nullptr;
    }
    setIsStreaming(false);

    // 若当前为联网对话，停止时也结束 RAG 状态，避免 UI 永远停留在「重写中 / 查询中」
    m_rewriteStreaming = false;
    if (!m_messagesModel.isEmpty()) {
        m_messagesModel.updateLastAiMessageRagSearchStatus(QString());
    }

    // 标记最后 AI 消息已完成
    if (!m_messagesModel.isEmpty()) {
        const int lastRow = m_messagesModel.size() - 1;
        QVariantMap last = m_messagesModel.at(lastRow);
        if (last["role"].toString() == "ai" && last["isThinking"].toBool()) {
            last["isThinking"] = false;
            m_messagesModel.replaceAtRow(lastRow, last);
        }
    }
    saveCurrentSession();
}

// ── RAG 异步检索（ChatGPT 风格：搜索 + 抓取网页正文）──────────────────────────
// 参考 MindSearch：History + Query Rewrite → 搜索。有历史时用 LLM 改写，否则直接搜索
void MainView::startRagFetchAndApiCall(const QString &userQuery) {
    if (userQuery.isEmpty()) {
        m_messagesModel.updateLastAiMessageRagSearchStatus("done");
        doStartApiCall(QString());
        return;
    }
    const QVariantList msgsSnapshot = m_messagesModel.toVariantList();
    QString conversationContext = WebSearchService::buildConversationContextFromMessages(
        msgsSnapshot, true);

    // 统计历史中已存在的用户消息数量（不含当前这条），用于判断是否需要改写
    int userCountBefore = 0;
    for (const QVariant &v : msgsSnapshot) {
        const QVariantMap m = v.toMap();
        if (m.value("role").toString() == QLatin1String("user"))
            ++userCountBefore;
    }

    // Query Rewrite：有对话历史时，用 LLM 将「历史+当前问题」改写为适合搜索引擎的 query
    // 若当前只有一个用户 query（无有效历史），则直接搜索，不再走改写流程
    if (userCountBefore > 1 && !conversationContext.isEmpty()
        && m_llmBackend && !m_settings->apiKey().trimmed().isEmpty()) {
        applyLlmForCurrentChatMode();
        m_messagesModel.updateLastAiMessageRagSearchStatus("rewriting");
        const QString sysPrompt = QStringLiteral(
            "你是一个搜索查询改写助手。根据「主问题」「历史对话」和「当前问题」，输出适合搜索引擎的查询。\n"
            "要求：1) 解决指代（它、这个、that 等），用具体名词替换；2) 保持简洁，每个子查询为单一问题；3) 若有多个子查询用分号分隔；4) 只输出查询内容，不要解释或其他文字。");
        QVariantMap userMsg;
        userMsg["role"] = QStringLiteral("user");
        userMsg["content"] = conversationContext + QStringLiteral("\n\n当前问题：") + userQuery;
        QVariantList msgs;
        msgs.append(userMsg);

        // 一次性连接（SingleShotConnection）：收到改写结果或错误后执行搜索
        qint64 rewriteStart = QDateTime::currentMSecsSinceEpoch();
        connect(m_llmBackend, &LLMBackend::completeReceived, this,
            [this, userQuery, rewriteStart](const QString &rewritten, const QString &reasoning) {
                int dur = int(QDateTime::currentMSecsSinceEpoch() - rewriteStart);
                QString q = rewritten.trimmed();
                if (q.length() > 500) q = q.left(500).trimmed();
                QString think = reasoning.trimmed();
                if (think.isEmpty()) think = QStringLiteral("改写结果：") + (q.isEmpty() ? userQuery : q);
                onQueryRewriteDone(userQuery, q.isEmpty() ? userQuery : q, dur, think);
            }, Qt::SingleShotConnection);
        connect(m_llmBackend, &LLMBackend::errorOccurred, this,
            [this, userQuery, conversationContext, rewriteStart](const QString &) {
                int dur = int(QDateTime::currentMSecsSinceEpoch() - rewriteStart);
                QString fallback = WebSearchService::buildContextualSearchQuery(conversationContext, userQuery);
                onQueryRewriteDone(userQuery, fallback.isEmpty() ? userQuery : fallback, dur, QStringLiteral("回退规则改写"));
            }, Qt::SingleShotConnection);
        m_llmBackend->chatComplete(msgs, sysPrompt);
        return;
    }

    // 无历史或无可用 LLM：直接搜索，使用规则-based 改写（若有 context）
    QString effectiveQuery = userQuery;
    if (!conversationContext.isEmpty())
        effectiveQuery = WebSearchService::buildContextualSearchQuery(conversationContext, userQuery);
    if (effectiveQuery.isEmpty()) effectiveQuery = userQuery;
    onQueryRewriteDone(userQuery, effectiveQuery, 0, QString());
}

void MainView::onQueryRewriteDone(const QString &userQuery, const QString &rewrittenQuery,
                                 int rewriteDurationMs, const QString &rewriteThinking) {
    // 启动“前端流式”：将完整 rewriteThinking 按小块写入模型，让 QML 看到逐字增长的效果
    startRewriteThinkingStream(rewriteDurationMs, rewriteThinking);
    m_messagesModel.updateLastAiMessageRagSearchStatus("searching");
    QString engine = m_settings->searchEngine().trimmed().toLower();
    if (engine != QLatin1String("duckduckgo") && engine != QLatin1String("bing")
        && engine != QLatin1String("brave") && engine != QLatin1String("google")
        && engine != QLatin1String("tencent")) {
        engine = QStringLiteral("duckduckgo");
    }
    QString proxyMode = m_settings->proxyMode().trimmed().toLower();
    QString proxyUrl = m_settings->proxyUrl().trimmed();
    if (proxyMode != QLatin1String("system") && proxyMode != QLatin1String("manual"))
        proxyMode = QStringLiteral("off");
    QString apiKey = m_settings->webSearchApiKey().trimmed();
    QString tcId = m_settings->tencentSecretId().trimmed();
    QString tcKey = m_settings->tencentSecretKey().trimmed();

    qint64 searchStart = QDateTime::currentMSecsSinceEpoch();
    using ResultType = QVector<SearchResult>;
    auto *watcher = new QFutureWatcher<ResultType>(this);
    connect(watcher, &QFutureWatcher<ResultType>::finished, this,
        [this, watcher, userQuery, rewriteDurationMs, rewriteThinking, searchStart]() {
            watcher->deleteLater();
            int searchDur = int(QDateTime::currentMSecsSinceEpoch() - searchStart);
            onRagSearchDone(userQuery, watcher->result(), rewriteDurationMs, rewriteThinking, searchDur);
        });
    watcher->setFuture(QtConcurrent::run([rewrittenQuery, engine, proxyMode, proxyUrl, apiKey, tcId, tcKey]() {
        return WebSearchService::searchWithContent(rewrittenQuery, engine, proxyMode, proxyUrl, nullptr, 5, apiKey, tcId, tcKey);
    }));
}

void MainView::onRagSearchDone(const QString &userQuery,
                               const QVector<SearchResult> &results,
                               int rewriteDurationMs, const QString &rewriteThinking, int searchDurationMs) {
    QString ragContext;
    QVariantList ragLinks;

    if (results.isEmpty()) {
        ragContext = QString();
    } else {
        // ChatGPT 风格：编号引用 [1] [2]...，含标题、URL、正文摘要
        QStringList parts;
        for (int i = 0; i < results.size(); ++i) {
            const SearchResult &r = results.at(i);
            QString block = QStringLiteral("[%1] %2\n%3")
                .arg(i + 1)
                .arg(r.title)
                .arg(r.snippet.isEmpty() ? r.title : r.snippet);
            parts << block;
            ragLinks.append(QVariantMap{
                {QStringLiteral("text"), r.title},
                {QStringLiteral("url"), r.url},
                {QStringLiteral("snippet"), r.snippet},
                {QStringLiteral("index"), i + 1}
            });
        }
        ragContext = parts.join(QStringLiteral("\n\n"));
    }

    m_messagesModel.updateLastAiMessageRagSearchStatus("done");
    m_messagesModel.updateLastAiMessageRagLinks(ragLinks);
    m_messagesModel.updateLastAiMessageRagMeta(rewriteDurationMs, rewriteThinking, searchDurationMs);
    doStartApiCall(ragContext);
}

// ── 核心：发起 API 请求 ───────────────────────────────────────────────────────
void MainView::startApiCall(const QVariantList & /*history*/, const QString &ragContext) {
    // 添加 AI 占位消息（非 RAG 路径）
    QVariantMap aiMsg;
    aiMsg["role"]      = "ai";
    aiMsg["content"]   = "";
    aiMsg["thinking"]  = "";
    aiMsg["isThinking"]= true;
    aiMsg["id"]        = QString::number(QDateTime::currentMSecsSinceEpoch());
    m_messagesModel.appendOne(aiMsg);
    setIsStreaming(true);
    doStartApiCall(ragContext);
}

void MainView::doStartApiCall(const QString &ragContext) {
    applyLlmForCurrentChatMode();

    // 构造请求
    // 构造请求：根据 apiUrl 推导出 /chat/completions 地址
    QUrl apiUrl(m_settings->apiUrl());
    QString path = apiUrl.path();

    // 1) 确保路径里包含 /v1（兼容 https://api.openai.com 和其他兼容端点）
    const QString v1Segment = QStringLiteral("/v1");
    if (!path.contains(v1Segment)) {
        if (!path.endsWith(QLatin1Char('/')))
            path.append(QLatin1Char('/'));
        path.append(QStringLiteral("v1"));
    }

    // 2) 如果还没有 /chat/completions，则补上
    const QString completionsPath = QStringLiteral("/chat/completions");
    if (!path.contains(completionsPath)) {
        if (!path.endsWith(QLatin1Char('/')))
            path.append(QLatin1Char('/'));
        path.append(QStringLiteral("chat/completions"));
    }
    apiUrl.setPath(path);

    QNetworkRequest req{apiUrl};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization",
                     ("Bearer " + m_settings->apiKey()).toUtf8());

    QJsonObject body;
    const QString wm = normalizeChatMode(m_chatMode);
    body["model"]  = m_settings->effectiveModelForChatMode(wm);
    body["stream"] = true;
    body["temperature"] = m_settings->effectiveTemperatureForChatMode(wm);
    body["top_p"]       = m_settings->topP();
    body["max_tokens"]  = m_settings->maxTokens();
    // 始终请求思考过程，兼容多种 API：
    // - DeepSeek: thinking.type = enabled
    // - Qwen/通义: enable_thinking = true
    body["thinking"]        = QJsonObject{{"type", "enabled"}};
    body["enable_thinking"]  = true;

    // 构建 messages 数组（system + 历史）
    QJsonArray msgs;
    QString systemContent = m_settings->systemPrompt().trimmed();
    if (!ragContext.isEmpty())
        systemContent = (systemContent.isEmpty() ? QString() : systemContent + "\n\n")
            + QStringLiteral("【联网检索结果】以下是从网络搜索并抓取的网页内容，请据此回答用户问题。"
                             "回答时请注明引用来源，使用 [1]、[2] 等编号对应下方各条。每条格式为 [编号] 标题 + 正文摘要。\n\n")
            + ragContext;
    if (!systemContent.isEmpty())
        msgs.append(QJsonObject{{"role","system"},{"content",systemContent}});

    const QVariantList snapshot = m_messagesModel.toVariantList();
    for (const QVariant &v : snapshot) {
        QVariantMap m = v.toMap();
        QString role = m["role"].toString();
        if (role == "ai") role = "assistant";
        QString content = m["content"].toString();
        if (content.isEmpty()) continue;
        msgs.append(QJsonObject{{"role", role}, {"content", content}});
    }
    body["messages"] = msgs;

    QNetworkReply *reply = m_nam->post(req, QJsonDocument(body).toJson());
    m_activeReply = reply;

    // ── 流式读取 ──────────────────────────────────────────────────────────────
    connect(reply, &QNetworkReply::readyRead, this, [this, reply]() {
        while (reply->canReadLine()) {
            QByteArray line = reply->readLine().trimmed();
            if (!line.startsWith("data: ")) continue;

            QByteArray data = line.mid(6);
            if (data == "[DONE]") {
                updateLastAiMessage("", "", false);
                setIsStreaming(false);
                const QString sessionId = m_currentSession;
                // 延迟执行保存等耗时操作到下一事件循环，避免阻塞 UI 主线程导致界面卡死
                QTimer::singleShot(0, this, [this, sessionId]() {
                    if (m_currentSession != sessionId) return;  // 用户已切换会话
                    autoNameCurrentSession();
                    saveCurrentSession();
                    m_history->touchSession(sessionId);
                });
                return;
            }

            QJsonObject delta = QJsonDocument::fromJson(data)
                                .object()["choices"].toArray()
                                .first().toObject()["delta"].toObject();

            QString reasoning = delta.value("reasoning_content").toString();
            QString content   = delta.value("content").toString();

            bool isThinking = content.isEmpty()
                && (!reasoning.isEmpty()
                    || m_messagesModel.at(m_messagesModel.size() - 1).value("content").toString().isEmpty());

            updateLastAiMessage(reasoning, content, isThinking);
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError
            && reply->error() != QNetworkReply::OperationCanceledError) {
            qWarning() << "API Error:" << reply->errorString();
            if (reply->isOpen()) {
                updateLastAiMessage("",
                    "\n\n> **[错误]** " + reply->errorString(), false);
            }
            emit errorOccurred(reply->errorString());
        }
        setIsStreaming(false);
        const QString sessionId = m_currentSession;
        // 延迟执行保存到下一事件循环，避免阻塞 UI 主线程
        QTimer::singleShot(0, this, [this, sessionId]() {
            if (m_currentSession != sessionId) return;
            saveCurrentSession();
        });
        reply->deleteLater();
        if (m_activeReply == reply) m_activeReply = nullptr;
    });
}

void MainView::startRewriteThinkingStream(int rewriteDurationMs, const QString &fullThinking) {
    m_rewriteFullThinking = fullThinking;
    m_rewriteStreamPos = 0;
    m_rewriteStreaming = !m_rewriteFullThinking.isEmpty();

    // 若没有任何思考内容（例如规则回退），直接写空字符串和耗时信息
    if (!m_rewriteStreaming) {
        m_messagesModel.updateLastAiMessageRagMeta(rewriteDurationMs, QString(), 0);
        return;
    }

    // 先写入一个很短的前缀，避免“重写中”气泡内部长时间完全空白
    appendRewriteThinkingChunk(rewriteDurationMs);
}

void MainView::appendRewriteThinkingChunk(int rewriteDurationMs) {
    if (!m_rewriteStreaming || m_rewriteFullThinking.isEmpty())
        return;

    // 每次追加固定长度的子串，模拟“流式增长”
    const int chunkSize = 32;
    m_rewriteStreamPos = qMin(m_rewriteStreamPos + chunkSize, m_rewriteFullThinking.size());
    const QString current = m_rewriteFullThinking.left(m_rewriteStreamPos);

    // 将当前进度写入模型，QML 端的 rewriteThinking 绑定会随之更新
    m_messagesModel.updateLastAiMessageRagMeta(rewriteDurationMs, current, 0);

    if (m_rewriteStreamPos >= m_rewriteFullThinking.size()) {
        // 已写完全部内容
        m_rewriteStreaming = false;
        return;
    }

    // 继续下一小块写入
    QTimer::singleShot(40, this, [this, rewriteDurationMs]() {
        appendRewriteThinkingChunk(rewriteDurationMs);
    });
}

// ── 更新最后一条 AI 消息 ──────────────────────────────────────────────────────
void MainView::updateLastAiMessage(const QString &reasoningChunk,
                                   const QString &contentChunk,
                                   bool isThinking) {
    m_messagesModel.updateLastAiMessageAppend(reasoningChunk, contentChunk, isThinking);
}

// ── 持久化 ────────────────────────────────────────────────────────────────────
static QJsonObject messageToJson(const QVariantMap &m) {
    QJsonObject o;
    o["role"]       = m["role"].toString();
    o["content"]    = m["content"].toString();
    o["thinking"]   = m["thinking"].toString();
    o["isThinking"] = false;
    o["id"]         = m["id"].toString();
    o["ragSearchStatus"] = m["ragSearchStatus"].toString();
    if (m.contains("rewriteDurationMs")) o["rewriteDurationMs"] = m["rewriteDurationMs"].toInt();
    if (m.contains("rewriteThinking")) o["rewriteThinking"] = m["rewriteThinking"].toString();
    if (m.contains("searchDurationMs")) o["searchDurationMs"] = m["searchDurationMs"].toInt();
    if (m.contains("ragLinks")) {
        QJsonArray linksArr;
        for (const QVariant &lv : m["ragLinks"].toList()) {
            QVariantMap lm = lv.toMap();
            QJsonObject lo;
            lo["text"] = lm["text"].toString();
            lo["url"] = lm["url"].toString();
            if (lm.contains("snippet")) lo["snippet"] = lm["snippet"].toString();
            if (lm.contains("index")) lo["index"] = lm["index"].toInt();
            linksArr.append(lo);
        }
        o["ragLinks"] = linksArr;
    }
    if (m.contains("blocks")) {
        QJsonArray blocksArr;
        for (const QVariant &b : m["blocks"].toList()) {
            QVariantMap bm = b.toMap();
            QJsonObject bo;
            bo["type"] = bm["type"].toString();
            bo["content"] = bm["content"].toString();
            if (bm.contains("toolName")) {
                bo["toolName"] = bm["toolName"].toString();
                bo["result"] = bm["result"].toString();
                bo["args"] = QJsonObject::fromVariantMap(bm["args"].toMap());
                if (bm.contains("durationSec")) bo["durationSec"] = bm["durationSec"].toDouble();
            } else if (bm["type"].toString() == QLatin1String("thinking") && bm.contains("durationSec")) {
                bo["durationSec"] = bm["durationSec"].toDouble();
            }
            blocksArr.append(bo);
        }
        o["blocks"] = blocksArr;
    }
    return o;
}

static QVariantMap jsonToMessage(const QJsonObject &o) {
    QVariantMap msg;
    QString role = o["role"].toString();
    msg["role"] = (role == QLatin1String("assistant")) ? QStringLiteral("ai") : role;
    msg["content"] = o["content"].toString();
    msg["thinking"] = o["thinking"].toString();
    msg["isThinking"] = false;
    msg["id"] = o["id"].toString();
    msg["ragSearchStatus"] = o["ragSearchStatus"].toString();
    if (o.contains("rewriteDurationMs")) msg["rewriteDurationMs"] = o["rewriteDurationMs"].toInt();
    if (o.contains("rewriteThinking")) msg["rewriteThinking"] = o["rewriteThinking"].toString();
    if (o.contains("searchDurationMs")) msg["searchDurationMs"] = o["searchDurationMs"].toInt();
    if (o.contains("ragLinks")) {
        QVariantList links;
        for (const QJsonValue &lv : o["ragLinks"].toArray()) {
            QJsonObject lm = lv.toObject();
            QVariantMap linkMap;
            linkMap["text"] = lm["text"].toString();
            linkMap["url"] = lm["url"].toString();
            if (lm.contains("snippet")) linkMap["snippet"] = lm["snippet"].toString();
            if (lm.contains("index")) linkMap["index"] = lm["index"].toInt();
            links.append(linkMap);
        }
        msg["ragLinks"] = links;
    }
    if (o.contains("blocks")) {
        QVariantList blocks;
        for (const QJsonValue &b : o["blocks"].toArray()) {
            QJsonObject bo = b.toObject();
            QVariantMap bm;
            bm["type"] = bo["type"].toString();
            bm["content"] = bo["content"].toString();
            if (bo.contains("toolName")) {
                bm["toolName"] = bo["toolName"].toString();
                bm["result"] = bo["result"].toString();
                bm["args"] = bo["args"].toObject().toVariantMap();
                if (bo.contains("durationSec")) bm["durationSec"] = bo["durationSec"].toDouble();
            } else if (bo["type"].toString() == QLatin1String("thinking") && bo.contains("durationSec")) {
                bm["durationSec"] = bo["durationSec"].toDouble();
            }
            blocks.append(bm);
        }
        msg["blocks"] = blocks;
    }
    return msg;
}

void MainView::saveCurrentSession() {
    if (m_currentSession.isEmpty()) return;

    // 保存前先将当前可见 tail 写回各分支的 branchTails，避免历史版本切换后消息丢失
    m_messagesModel.persistCurrentBranchTails();

    QJsonArray arr;
    const QVariantList list = m_messagesModel.toVariantList();
    for (int idx = 0; idx < list.size(); ++idx) {
        QVariantMap m = list.at(idx).toMap();
        QJsonObject o = messageToJson(m);
        if (m.contains("editHistory")) {
            QJsonArray histArr;
            for (const QVariant &h : m["editHistory"].toList()) {
                QVariantMap hm = h.toMap();
                QJsonObject ho;
                ho["content"] = hm["content"].toString();
                ho["parentIndex"] = hm["parentIndex"].toInt();
                histArr.append(ho);
            }
            o["editHistory"] = histArr;
            o["currentEditIndex"] = m["currentEditIndex"].toInt();
        }
        if (m.contains("branchTails")) {
            QJsonArray tailsArr;
            for (const QVariant &t : m["branchTails"].toList()) {
                QJsonArray msgArr;
                for (const QVariant &em : t.toList()) {
                    msgArr.append(messageToJson(em.toMap()));
                }
                tailsArr.append(msgArr);
            }
            o["branchTails"] = tailsArr;
        }
        arr.append(o);
    }

    QJsonObject root;
    root["id"]       = m_currentSession;
    root["name"]     = m_sessionName;
    root["chatMode"] = m_chatMode;
    root["messages"] = arr;

    QFile f(m_history->sessionFilePath(m_currentSession));
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

void MainView::loadSessionFile(const QString &id) {
    QFile f(m_history->sessionFilePath(id));
    if (!f.open(QIODevice::ReadOnly)) {
        m_messagesModel.clearAll();
        m_sessionName = "新对话";
        emit sessionNameChanged();
        return;
    }

    QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    m_sessionName = root["name"].toString("新对话");
    emit sessionNameChanged();

    if (root.contains(QStringLiteral("chatMode"))) {
        const QString cm = normalizeChatMode(root[QStringLiteral("chatMode")].toString());
        if (m_chatMode != cm) {
            m_chatMode = cm;
            emit chatModeChanged();
        }
    }

    m_messagesModel.clearAll();
    for (const QJsonValue &v : root["messages"].toArray()) {
        QJsonObject o = v.toObject();
        QVariantMap msg;
        {
            const QString role = o["role"].toString();
            msg["role"] = (role == QLatin1String("assistant")) ? QStringLiteral("ai") : role;
        }
        msg["content"]    = o["content"].toString();
        msg["thinking"]   = o["thinking"].toString();
        msg["isThinking"] = false;
        msg["id"]         = o["id"].toString();
        msg["ragSearchStatus"] = o["ragSearchStatus"].toString();
        if (o.contains("rewriteDurationMs")) msg["rewriteDurationMs"] = o["rewriteDurationMs"].toInt();
        if (o.contains("rewriteThinking")) msg["rewriteThinking"] = o["rewriteThinking"].toString();
        if (o.contains("searchDurationMs")) msg["searchDurationMs"] = o["searchDurationMs"].toInt();
        if (o.contains("ragLinks")) {
            QVariantList links;
            for (const QJsonValue &lv : o["ragLinks"].toArray()) {
                QJsonObject lm = lv.toObject();
                QVariantMap linkMap;
                linkMap["text"] = lm["text"].toString();
                linkMap["url"] = lm["url"].toString();
                if (lm.contains("snippet")) linkMap["snippet"] = lm["snippet"].toString();
                if (lm.contains("index")) linkMap["index"] = lm["index"].toInt();
                links.append(linkMap);
            }
            msg["ragLinks"] = links;
        }
        if (o.contains("blocks")) {
            QVariantList blocks;
            for (const QJsonValue &b : o["blocks"].toArray()) {
                QJsonObject bo = b.toObject();
                QVariantMap bm;
                bm["type"] = bo["type"].toString();
                bm["content"] = bo["content"].toString();
            if (bo.contains("toolName")) {
                bm["toolName"] = bo["toolName"].toString();
                bm["result"] = bo["result"].toString();
                bm["args"] = bo["args"].toObject().toVariantMap();
                if (bo.contains("durationSec")) bm["durationSec"] = bo["durationSec"].toDouble();
            } else if (bo["type"].toString() == QLatin1String("thinking") && bo.contains("durationSec")) {
                bm["durationSec"] = bo["durationSec"].toDouble();
            }
            blocks.append(bm);
        }
        msg["blocks"] = blocks;
        }
        if (o.contains("editHistory")) {
            QVariantList hist;
            for (const QJsonValue &h : o["editHistory"].toArray()) {
                QJsonObject ho = h.toObject();
                QVariantMap hm;
                hm["content"] = ho["content"].toString();
                hm["parentIndex"] = ho["parentIndex"].toInt();
                hist.append(hm);
            }
            msg["editHistory"] = hist;
            msg["currentEditIndex"] = o["currentEditIndex"].toInt(0);
            if (o.contains("branchTails")) {
                QVariantList tails;
                for (const QJsonValue &t : o["branchTails"].toArray()) {
                    QVariantList msgList;
                    for (const QJsonValue &em : t.toArray()) {
                        msgList.append(jsonToMessage(em.toObject()));
                    }
                    tails.append(msgList);
                }
                msg["branchTails"] = tails;
            }
        }
        m_messagesModel.appendOne(msg);
    }
    applyLlmForCurrentChatMode();
}

QVariantList MainView::buildApiMessages() const {
    return m_messagesModel.toVariantList();
}

QString MainView::fetchRagContext(const QString &userQuery) const {
    if (!m_settings->chatOnline() || !m_toolRegistry || userQuery.isEmpty())
        return QString();
    QString raw = m_toolRegistry->execute(QStringLiteral("web_search"),
        QVariantMap{{QStringLiteral("query"), userQuery}});
    QJsonObject obj = QJsonDocument::fromJson(raw.toUtf8()).object();
    if (!obj["success"].toBool())
        return QString();
    QStringList parts;
    QString abs = obj["abstract"].toString();
    if (!abs.isEmpty() && abs != QStringLiteral("(无摘要)"))
        parts << QStringLiteral("摘要：") + abs;
    for (const QJsonValue &v : obj["related"].toArray()) {
        QString s = v.toString().trimmed();
        if (!s.isEmpty()) parts << s;
    }
    return parts.isEmpty() ? QString() : parts.join(QStringLiteral("\n"));
}

// ── 根据对话内容自动命名会话（调用 LLM 生成标题）───────────────────────────────────
void MainView::autoNameCurrentSession() {
    if (m_currentSession.isEmpty())
        return;

    // 只有默认名字时才自动命名，避免覆盖用户手动修改
    if (!(m_sessionName.startsWith(QStringLiteral("新标题("))
          || m_sessionName == QStringLiteral("新标题")
          || m_sessionName == QStringLiteral("新对话"))) {
        return;
    }

    const QVariantList list = m_messagesModel.toVariantList();
    bool hasUser = false;
    for (const QVariant &v : list) {
        if (v.toMap().value("role").toString() == QLatin1String("user")) {
            hasUser = true;
            break;
        }
    }
    if (!hasUser)
        return;

    requestSessionTitle();
}

void MainView::requestSessionTitle() {
    if (m_titleReply || m_currentSession.isEmpty()) return;

    // 参考 ChatOllama：仅用首条用户消息+首条AI回复，缩短 payload 加速返回
    QString userMsg, aiMsg;
    const QVariantList list = m_messagesModel.toVariantList();
    for (const QVariant &v : list) {
        const QVariantMap m = v.toMap();
        QString role = m.value("role").toString();
        QString content = m.value("content").toString().trimmed();
        if (content.isEmpty()) continue;
        if (role == QLatin1String("user") && userMsg.isEmpty()) {
            userMsg = content.length() > 80 ? content.left(80) + QStringLiteral("…") : content;
        } else if (role == QLatin1String("ai") && aiMsg.isEmpty()) {
            aiMsg = content.length() > 80 ? content.left(80) + QStringLiteral("…") : content;
        }
        if (!userMsg.isEmpty() && !aiMsg.isEmpty()) break;
    }
    if (userMsg.isEmpty()) return;

    QString dialog = QStringLiteral("[用户] %1\n").arg(userMsg);
    if (!aiMsg.isEmpty())
        dialog += QStringLiteral("[助手] %1\n").arg(aiMsg);

    const QString sessionIdForTitle = m_currentSession;  // 捕获，避免用户切换会话后命名错乱

    QUrl apiUrl(m_settings->apiUrl());
    QString path = apiUrl.path();
    if (!path.contains(QStringLiteral("/v1"))) {
        if (!path.endsWith(QLatin1Char('/'))) path.append(QLatin1Char('/'));
        path.append(QStringLiteral("v1"));
    }
    if (!path.contains(QStringLiteral("/chat/completions"))) {
        if (!path.endsWith(QLatin1Char('/'))) path.append(QLatin1Char('/'));
        path.append(QStringLiteral("chat/completions"));
    }
    apiUrl.setPath(path);

    QJsonArray msgs;
    msgs.append(QJsonObject{
        {"role", "system"},
        {"content", "你是对话标题生成器。根据以下对话内容，生成一个简短有力的中文标题概括主题。"
                    "要求：最多20个字，不要引号、句号，只输出标题本身。"}
    });
    msgs.append(QJsonObject{
        {"role", "user"},
        {"content", dialog}
    });

    QJsonObject body;
    body["model"] = m_settings->effectiveModelForChatMode(QStringLiteral("ask"));
    body["stream"] = false;
    body["temperature"] = 0.3;
    body["max_tokens"] = 32;
    body["messages"] = msgs;

    QNetworkRequest req(apiUrl);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization",
                     ("Bearer " + m_settings->apiKey()).toUtf8());

    m_titleReply = m_nam->post(req, QJsonDocument(body).toJson());
    connect(m_titleReply, &QNetworkReply::finished, this, [this, sessionIdForTitle]() {
        if (!m_titleReply) return;
        QNetworkReply *reply = m_titleReply;
        m_titleReply = nullptr;

        if (reply->error() != QNetworkReply::NoError
            && reply->error() != QNetworkReply::OperationCanceledError) {
            reply->deleteLater();
            return;
        }

        if (!reply->isOpen()) {
            reply->deleteLater();
            return;
        }

        QByteArray data = reply->readAll();
        reply->deleteLater();

        QJsonObject root = QJsonDocument::fromJson(data).object();
        QJsonArray choices = root["choices"].toArray();
        if (choices.isEmpty()) return;

        QString title = choices.first().toObject()["message"].toObject()["content"].toString().trimmed();
        title = title.remove(QStringLiteral("\"")).remove(QStringLiteral("「")).remove(QStringLiteral("」"));
        if (title.isEmpty()) return;
        if (title.length() > 20)
            title = title.left(20) + QStringLiteral("…");

        // 始终更新该会话的节点名称及会话文件
        m_history->renameNode(sessionIdForTitle, title);
        m_history->updateSessionNameInFile(sessionIdForTitle, title);
        // 若用户仍在查看该会话，才更新当前显示的 sessionName
        if (m_currentSession == sessionIdForTitle) {
            m_sessionName = title;
            emit sessionNameChanged();
            saveCurrentSession();
        }
    });
}

