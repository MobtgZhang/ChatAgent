#include "mainview.h"
#include "settings.h"
#include "history.h"
#include "agent_core.h"
#include "llm_backend.h"
#include "memory_module.h"
#include "tool_registry.h"
#include "tools/file_tool.h"
#include "tools/shell_tool.h"
#include "tools/websearch_tool.h"
#include "tools/keyboard_tool.h"
#include "tools/ocr_tool.h"
#include "tools/window_tool.h"
#include "tools/clipboard_tool.h"
#include "tools/wait_tool.h"
#include "tools/image_match_tool.h"

#include <QGuiApplication>
#include <QClipboard>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QDebug>
#include <QDateTime>

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
void MainView::newSession(const QString &name) {
    if (m_activeReply) m_activeReply->abort();

    QString sessionName = name.isEmpty()
            ? QStringLiteral("新标题(%1)").arg(m_settings->modelName())
            : name;
    QString id = m_history->createSession(sessionName);

    m_currentSession = id;
    m_sessionName    = sessionName;
    m_messagesModel.clearAll();

    // 添加 system 欢迎消息
    QVariantMap welcome;
    welcome["role"]      = "ai";
    welcome["content"]   = QString("你好！我是 **%1**，有什么可以帮助你的吗？")
                           .arg(m_settings->modelName());
    welcome["thinking"]  = "";
    welcome["isThinking"]= false;
    welcome["id"]        = "welcome";
    m_messagesModel.appendOne(welcome);
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
    // 清空 = 新建对话，旧对话保留在历史中（不删除）；删除对话用历史列表右键删除
    newSession(QString());
}

void MainView::setChatMode(const QString &mode) {
    QString m = mode;
    if (m != "chat" && m != "agent" && m != "planning")
        m = "chat";
    if (m_chatMode == m) return;
    m_chatMode = m;
    emit chatModeChanged();
}

void MainView::setupAgent() {
    m_agentMemory = new MemoryModule(this);

    LLMBackend *llm = new LLMBackend(this);
    llm->setApiKey(m_settings->apiKey());
    llm->setApiUrl(m_settings->apiUrl());
    llm->setModel(m_settings->modelName());
    connect(m_settings, &Settings::apiKeyChanged, this, [this, llm]() { llm->setApiKey(m_settings->apiKey()); });
    connect(m_settings, &Settings::apiUrlChanged, this, [this, llm]() { llm->setApiUrl(m_settings->apiUrl()); });
    connect(m_settings, &Settings::modelNameChanged, this, [this, llm]() { llm->setModel(m_settings->modelName()); });

    ToolRegistry *reg = new ToolRegistry(this);
    reg->registerTool(new FileTool(reg));
    reg->registerTool(new ShellTool(reg));
    reg->registerTool(new WebSearchTool(reg));
    reg->registerTool(new KeyboardTool(reg));
    reg->registerTool(new OcrTool(reg));
    reg->registerTool(new WindowTool(reg));
    reg->registerTool(new ClipboardTool(reg));
    reg->registerTool(new WaitTool(reg));
    reg->registerTool(new ImageMatchTool(reg));

    m_agentCore = new AgentCore(this);
    m_agentCore->setLLM(llm);
    m_agentCore->setTools(reg);
    m_agentCore->setMemory(m_agentMemory);
    m_agentCore->setSystemPromptBase(m_settings->systemPrompt());
    m_agentCore->setMaxToolRounds(m_settings->maxToolRounds());
    connect(m_settings, &Settings::systemPromptChanged, this, [this]() { m_agentCore->setSystemPromptBase(m_settings->systemPrompt()); });
    connect(m_settings, &Settings::maxToolRoundsChanged, this, [this]() { m_agentCore->setMaxToolRounds(m_settings->maxToolRounds()); });

    connect(m_agentCore, &AgentCore::chunkReceived, this, [this](const QString &content, const QString &reasoning, bool isThinking) {
        updateLastAiMessage(reasoning, content, isThinking);
    });
    connect(m_agentCore, &AgentCore::finished, this, [this]() {
        setIsStreaming(false);
        autoNameCurrentSession();
        saveCurrentSession();
        m_history->touchSession(m_currentSession);
    });
    connect(m_agentCore, &AgentCore::errorOccurred, this, &MainView::errorOccurred);
}

void MainView::renameSession(const QString &name) {
    if (name.trimmed().isEmpty()) return;
    m_sessionName = name.trimmed();
    m_history->renameNode(m_currentSession, m_sessionName);
    emit sessionNameChanged();
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

    msg["content"] = content.trimmed();
    m_messagesModel.replaceAtRow(index, msg);

    if (role == QLatin1String("user")) {
        // 用户消息：截断该条之后，重新发起请求
        m_messagesModel.truncateTo(index + 1);
        if (m_chatMode == "chat") {
            startApiCall(m_messagesModel.toVariantList());
        } else {
            startAgentCall(content.trimmed());
        }
    } else {
        // AI 消息：仅保存修改，不重新生成
        saveCurrentSession();
    }
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
            startApiCall(m_messagesModel.toVariantList());
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

    if (m_chatMode == "chat") {
        startApiCall(m_messagesModel.toVariantList());
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
    aiMsg["id"]        = QString::number(QDateTime::currentMSecsSinceEpoch());
    m_messagesModel.appendOne(aiMsg);
    setIsStreaming(true);

    QString mode = (m_chatMode == "planning") ? "planning" : "agent";
    m_agentCore->setMode(mode);

    QVariantList msgs;
    const QVariantList full = m_messagesModel.toVariantList();
    for (int i = 0; i < full.size() - 1; ++i) {
        msgs.append(full[i]);
    }
    m_agentCore->run(msgs, userInput);
}

void MainView::stopGeneration() {
    if (m_agentCore && (m_chatMode != "chat"))
        m_agentCore->stop();
    if (m_activeReply) {
        m_activeReply->abort();
        m_activeReply = nullptr;
    }
    setIsStreaming(false);

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

// ── 核心：发起 API 请求 ───────────────────────────────────────────────────────
void MainView::startApiCall(const QVariantList & /*history*/) {
    // 添加 AI 占位消息
    QVariantMap aiMsg;
    aiMsg["role"]      = "ai";
    aiMsg["content"]   = "";
    aiMsg["thinking"]  = "";
    aiMsg["isThinking"]= true;
    aiMsg["id"]        = QString::number(QDateTime::currentMSecsSinceEpoch());
    m_messagesModel.appendOne(aiMsg);
    setIsStreaming(true);

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
    body["model"]  = m_settings->modelName();
    body["stream"] = true;
    body["temperature"] = m_settings->temperature();
    body["top_p"]       = m_settings->topP();
    body["max_tokens"]  = m_settings->maxTokens();
    // 始终请求思考过程，兼容多种 API：
    // - DeepSeek: thinking.type = enabled
    // - Qwen/通义: enable_thinking = true
    body["thinking"]        = QJsonObject{{"type", "enabled"}};
    body["enable_thinking"]  = true;

    // 构建 messages 数组（system + 历史）
    QJsonArray msgs;
    if (!m_settings->systemPrompt().trimmed().isEmpty())
        msgs.append(QJsonObject{{"role","system"},{"content",m_settings->systemPrompt()}});

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
                autoNameCurrentSession();
                saveCurrentSession();
                m_history->touchSession(m_currentSession);
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
            updateLastAiMessage("",
                "\n\n> **[错误]** " + reply->errorString(), false);
            emit errorOccurred(reply->errorString());
        }
        setIsStreaming(false);
        saveCurrentSession();
        reply->deleteLater();
        if (m_activeReply == reply) m_activeReply = nullptr;
    });
}

// ── 更新最后一条 AI 消息 ──────────────────────────────────────────────────────
void MainView::updateLastAiMessage(const QString &reasoningChunk,
                                   const QString &contentChunk,
                                   bool isThinking) {
    m_messagesModel.updateLastAiMessageAppend(reasoningChunk, contentChunk, isThinking);
}

// ── 持久化 ────────────────────────────────────────────────────────────────────
void MainView::saveCurrentSession() {
    if (m_currentSession.isEmpty()) return;

    QJsonArray arr;
    const QVariantList list = m_messagesModel.toVariantList();
    for (const QVariant &v : list) {
        const QVariantMap m = v.toMap();
        QJsonObject o;
        o["role"]       = m["role"].toString();
        o["content"]    = m["content"].toString();
        o["thinking"]   = m["thinking"].toString();
        o["isThinking"] = false;   // 持久化时总是已完成
        o["id"]         = m["id"].toString();
        arr.append(o);
    }

    QJsonObject root;
    root["id"]       = m_currentSession;
    root["name"]     = m_sessionName;
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

    m_messagesModel.clearAll();
    for (const QJsonValue &v : root["messages"].toArray()) {
        QJsonObject o = v.toObject();
        QVariantMap msg;
        // 兼容旧数据：将 OpenAI 风格的 assistant 角色归一化为 ai
        {
            const QString role = o["role"].toString();
            msg["role"] = (role == QLatin1String("assistant")) ? QStringLiteral("ai") : role;
        }
        msg["content"]    = o["content"].toString();
        msg["thinking"]   = o["thinking"].toString();
        msg["isThinking"] = false;
        msg["id"]         = o["id"].toString();
        m_messagesModel.appendOne(msg);
    }
}

QVariantList MainView::buildApiMessages() const {
    return m_messagesModel.toVariantList();
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
    body["model"] = m_settings->modelName();
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

