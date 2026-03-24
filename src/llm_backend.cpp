#include "llm_backend.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QDebug>

LLMBackend::LLMBackend(QObject *parent) : QObject(parent) {
    m_nam = new QNetworkAccessManager(this);
}

void LLMBackend::setApiKey(const QString &key) { m_apiKey = key; }
void LLMBackend::setApiUrl(const QString &url) { m_apiUrl = url; }
void LLMBackend::setModel(const QString &model) { m_model = model; }
void LLMBackend::setChatRequestParams(double temperature, int maxTokens) {
    m_chatTemperature = qBound(0.0, temperature, 2.0);
    m_chatMaxTokens = qBound(256, maxTokens, 32768);
}
void LLMBackend::setTools(const QVariantList &toolsSchema) { m_toolsSchema = toolsSchema; }

QUrl LLMBackend::buildCompletionsUrl() const {
    QUrl url(m_apiUrl);
    QString path = url.path();
    const QString v1 = QStringLiteral("/v1");
    if (!path.contains(v1)) {
        if (!path.endsWith('/')) path += '/';
        path += "v1";
    }
    const QString comp = QStringLiteral("/chat/completions");
    if (!path.contains(comp)) {
        if (!path.endsWith('/')) path += '/';
        path += "chat/completions";
    }
    url.setPath(path);
    return url;
}

void LLMBackend::chatStream(const QVariantList &messages, const QString &systemPrompt) {
    abort();
    clearPendingToolCalls();

    QJsonObject body;
    body["model"] = m_model;
    body["stream"] = true;
    body["temperature"] = m_chatTemperature;
    body["max_tokens"] = m_chatMaxTokens;
    // 始终请求思考过程，兼容多种 API：
    // - DeepSeek: thinking.type = enabled
    // - Qwen/通义: enable_thinking = true
    body["thinking"]       = QJsonObject{{"type", "enabled"}};
    body["enable_thinking"] = true;

    QJsonArray msgs;
    if (!systemPrompt.isEmpty()) {
        msgs.append(QJsonObject{{"role", "system"}, {"content", systemPrompt}});
    }
    for (const QVariant &v : messages) {
        QVariantMap m = v.toMap();
        QString role = m["role"].toString();
        if (role == "ai") role = "assistant";
        QString content = m["content"].toString();

        QJsonObject obj;
        obj["role"] = role;
        if (m.contains("name"))
            obj["name"] = m["name"].toString();

        // 支持 tool_calls 格式
        if (m.contains("tool_calls")) {
            obj["content"] = content;
            QJsonArray tc;
            for (const QVariant &v : m["tool_calls"].toList())
                tc.append(QJsonValue::fromVariant(v));
            if (!tc.isEmpty())
                obj["tool_calls"] = tc;
        } else {
            obj["content"] = content;
        }
        msgs.append(obj);
    }
    body["messages"] = msgs;

    if (!m_toolsSchema.isEmpty()) {
        QJsonArray toolsArr;
        for (const QVariant &v : m_toolsSchema)
            toolsArr.append(QJsonValue::fromVariant(v));
        body["tools"] = toolsArr;
        body["tool_choice"] = "auto";
    }

    QNetworkRequest req(buildCompletionsUrl());
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());

    m_reply = m_nam->post(req, QJsonDocument(body).toJson());
    connect(m_reply, &QNetworkReply::readyRead, this, &LLMBackend::onReadyRead);
    connect(m_reply, &QNetworkReply::finished, this, &LLMBackend::onFinished);
}

void LLMBackend::abort() {
    if (m_reply) {
        m_reply->abort();
        m_reply = nullptr;
    }
    if (m_completeReply) {
        m_completeReply->abort();
        m_completeReply = nullptr;
    }
}

void LLMBackend::chatComplete(const QVariantList &messages, const QString &systemPrompt) {
    if (m_completeReply) {
        m_completeReply->abort();
        m_completeReply = nullptr;
    }

    QJsonObject body;
    body["model"] = m_model;
    body["stream"] = false;
    body["temperature"] = 0.3;
    body["max_tokens"] = 256;
    body["thinking"] = QJsonObject{{"type", "enabled"}};
    body["enable_thinking"] = true;

    QJsonArray msgs;
    if (!systemPrompt.isEmpty())
        msgs.append(QJsonObject{{"role", "system"}, {"content", systemPrompt}});
    for (const QVariant &v : messages) {
        QVariantMap m = v.toMap();
        QString role = m["role"].toString();
        if (role == "ai") role = "assistant";
        msgs.append(QJsonObject{{"role", role}, {"content", m["content"].toString()}});
    }
    body["messages"] = msgs;

    QNetworkRequest req(buildCompletionsUrl());
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());

    m_completeReply = m_nam->post(req, QJsonDocument(body).toJson());
    connect(m_completeReply, &QNetworkReply::finished, this, &LLMBackend::onCompleteFinished);
}

void LLMBackend::onReadyRead() {
    if (!m_reply) return;
    while (m_reply->canReadLine()) {
        QByteArray line = m_reply->readLine().trimmed();
        parseStreamLine(line);
    }
}

void LLMBackend::clearPendingToolCalls() {
    m_pendingToolCalls.clear();
}

void LLMBackend::emitPendingToolCalls() {
    // 先复制到局部，避免 emit 触发 chatStream -> clearPendingToolCalls 导致迭代器失效
    QList<std::pair<QString, QVariantMap>> toEmit;
    for (auto it = m_pendingToolCalls.constBegin(); it != m_pendingToolCalls.constEnd(); ++it) {
        const QString &name = it.value().first;
        const QString &argsStr = it.value().second;
        if (name.isEmpty() || argsStr.isEmpty()) continue;
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(argsStr.toUtf8(), &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) continue;
        const QVariantMap vm = doc.object().toVariantMap();
        toEmit.append(std::pair<QString, QVariantMap>(name, vm));
    }
    m_pendingToolCalls.clear();
    for (const auto &p : toEmit)
        emit toolCallRequested(p.first, p.second);
}

void LLMBackend::parseStreamLine(const QByteArray &line) {
    if (!line.startsWith("data: ")) return;
    QByteArray data = line.mid(6);
    if (data == "[DONE]") {
        const bool hadToolCalls = !m_pendingToolCalls.isEmpty();
        emitPendingToolCalls();
        // 若有 tool 调用，AgentCore 会继续 ReAct，此时不应 emit finished
        if (!hadToolCalls)
            emit finished("");
        return;
    }

    QJsonObject root = QJsonDocument::fromJson(data).object();
    QJsonArray choices = root["choices"].toArray();
    if (choices.isEmpty()) return;

    QJsonObject choice = choices.first().toObject();
    QJsonObject delta = choice["delta"].toObject();

    QString reasoning = delta.value("reasoning_content").toString();
    QString content = delta.value("content").toString();

    // 累积流式 tool_calls（arguments 分块到达，需在 [DONE] 时统一解析发射）
    if (delta.contains("tool_calls")) {
        QJsonArray tc = delta["tool_calls"].toArray();
        for (const QJsonValue &v : tc) {
            QJsonObject tco = v.toObject();
            int idx = tco["index"].toInt(0);
            QJsonObject fn = tco["function"].toObject();
            QString nameDelta = fn["name"].toString();
            QString argsDelta = fn["arguments"].toString();
            if (!m_pendingToolCalls.contains(idx))
                m_pendingToolCalls[idx] = std::make_pair(QString(), QString());
            auto &p = m_pendingToolCalls[idx];
            if (!nameDelta.isEmpty()) p.first += nameDelta;  // name 也可能分块
            if (!argsDelta.isEmpty()) p.second += argsDelta;
        }
    }

    bool isThinking = content.isEmpty() && !reasoning.isEmpty();
    emit chunkReceived(content, reasoning, isThinking);
}

void LLMBackend::onFinished() {
    if (!m_reply) return;
    if (m_reply->error() != QNetworkReply::NoError
        && m_reply->error() != QNetworkReply::OperationCanceledError) {
        emit errorOccurred(m_reply->errorString());
    }
    m_reply->deleteLater();
    m_reply = nullptr;
}

void LLMBackend::onCompleteFinished() {
    if (!m_completeReply) return;
    QNetworkReply *reply = m_completeReply;
    m_completeReply = nullptr;

    if (reply->error() != QNetworkReply::NoError
        && reply->error() != QNetworkReply::OperationCanceledError) {
        emit errorOccurred(reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonObject root = QJsonDocument::fromJson(data).object();
    QJsonArray choices = root["choices"].toArray();
    if (choices.isEmpty()) {
        emit errorOccurred(QStringLiteral("Empty completion response"));
        return;
    }
    QJsonObject msg = choices.first().toObject()["message"].toObject();
    QString content = msg["content"].toString().trimmed();
    QString reasoning = msg["reasoning_content"].toString().trimmed();
    emit completeReceived(content, reasoning);
}
