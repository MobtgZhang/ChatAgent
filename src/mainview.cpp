#include "mainview.h"
#include "settings.h"
#include "history.h"

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
    if (m_activeReply) m_activeReply->abort();
    m_messagesModel.clearAll();

    QVariantMap welcome;
    welcome["role"]      = "ai";
    welcome["content"]   = "对话已清空。有什么可以帮助你的吗？";
    welcome["thinking"]  = "";
    welcome["isThinking"]= false;
    welcome["id"]        = "welcome";
    m_messagesModel.appendOne(welcome);
    saveCurrentSession();
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

    startApiCall(m_messagesModel.toVariantList());
}

// ── 停止生成 ──────────────────────────────────────────────────────────────────
void MainView::stopGeneration() {
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

// ── 根据对话内容自动命名会话 ─────────────────────────────────────────────────────
void MainView::autoNameCurrentSession() {
    if (m_currentSession.isEmpty())
        return;

    // 只有默认名字时才自动命名，避免覆盖用户手动修改
    if (!(m_sessionName.startsWith(QStringLiteral("新标题("))
          || m_sessionName == QStringLiteral("新标题")
          || m_sessionName == QStringLiteral("新对话"))) {
        return;
    }

    // 找第一条用户消息作为标题
    QString title;
    const QVariantList list = m_messagesModel.toVariantList();
    for (const QVariant &v : list) {
        const QVariantMap m = v.toMap();
        if (m.value("role").toString() == QLatin1String("user")) {
            title = m.value("content").toString().trimmed();
            if (!title.isEmpty())
                break;
        }
    }

    if (title.isEmpty())
        return;

    if (title.length() > 40)
        title = title.left(40) + QStringLiteral("…");

    m_sessionName = title;
    m_history->renameNode(m_currentSession, m_sessionName);
    emit sessionNameChanged();
}

