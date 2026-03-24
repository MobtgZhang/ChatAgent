#ifndef LLM_BACKEND_H
#define LLM_BACKEND_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QMap>
#include <utility>

/**
 * LLM 通信后端
 * 支持流式/非流式，支持 function calling（工具调用）
 */
class LLMBackend : public QObject {
    Q_OBJECT

public:
    explicit LLMBackend(QObject *parent = nullptr);

    // 配置（可从 Settings 注入）
    void setApiKey(const QString &key);
    void setApiUrl(const QString &url);
    void setModel(const QString &model);
    void setChatRequestParams(double temperature, int maxTokens);

    // 工具 schema（OpenAI 格式）
    void setTools(const QVariantList &toolsSchema);

    // 流式调用：消息历史 + 工具 schema
    Q_INVOKABLE void chatStream(const QVariantList &messages,
                                const QString &systemPrompt = QString());

    // 非流式单次补全（用于 Query Rewrite 等）：仅返回全文，无 tools
    Q_INVOKABLE void chatComplete(const QVariantList &messages,
                                  const QString &systemPrompt = QString());

    // 停止当前请求
    Q_INVOKABLE void abort();

signals:
    void chunkReceived(const QString &contentChunk, const QString &reasoningChunk, bool isThinking);
    void finished(const QString &fullContent);
    void completeReceived(const QString &fullContent, const QString &reasoningContent = QString());  // chatComplete 成功时发射
    void toolCallRequested(const QString &toolName, const QVariantMap &args);
    void errorOccurred(const QString &msg);

private slots:
    void onReadyRead();
    void onFinished();
    void onCompleteFinished();

private:
    void parseStreamLine(const QByteArray &line);
    void emitPendingToolCalls();
    void clearPendingToolCalls();
    QUrl buildCompletionsUrl() const;

    QString m_apiKey;
    QString m_apiUrl;
    QString m_model;
    double m_chatTemperature = 0.7;
    int m_chatMaxTokens = 4096;
    QVariantList m_toolsSchema;

    QNetworkAccessManager *m_nam;
    QPointer<QNetworkReply> m_reply;
    QPointer<QNetworkReply> m_completeReply;  // chatComplete 专用，与 m_reply 互斥

    // 流式 tool_calls 累积：index -> (name, arguments)
    QMap<int, std::pair<QString, QString>> m_pendingToolCalls;
};

#endif // LLM_BACKEND_H
