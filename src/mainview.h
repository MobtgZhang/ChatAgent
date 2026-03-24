#ifndef MAINVIEW_H
#define MAINVIEW_H

#include <QObject>
#include <QVariantList>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QAbstractItemModel>
#include <QVector>
#include <QPair>

#include "messagemodel.h"
#include "web_search_service.h"

class Settings;
class History;
class AgentCore;
class LLMBackend;
class MemoryModule;
class ToolRegistry;
class SkillManager;

class MainView : public QObject {
    Q_OBJECT

    Q_PROPERTY(QAbstractItemModel* messagesModel READ messagesModel CONSTANT)
    Q_PROPERTY(QString      currentSession  READ currentSession  NOTIFY currentSessionChanged)
    Q_PROPERTY(QString      sessionName     READ sessionName     NOTIFY sessionNameChanged)
    Q_PROPERTY(bool         isStreaming     READ isStreaming     NOTIFY isStreamingChanged)
    Q_PROPERTY(QString      chatMode        READ chatMode        WRITE setChatMode NOTIFY chatModeChanged)

public:
    explicit MainView(Settings *settings, History *history,
                      QObject *parent = nullptr);

    QAbstractItemModel* messagesModel() { return &m_messagesModel; }
    QString      currentSession() const { return m_currentSession; }
    QString      sessionName()    const { return m_sessionName; }
    bool         isStreaming()    const { return m_isStreaming; }
    QString      chatMode()       const { return m_chatMode; }
    void        setChatMode(const QString &mode);

    // Agent 记忆模块（供 QML 右侧面板使用）
    MemoryModule* agentMemory() const { return m_agentMemory; }

    // 连接技能管理器（main.cpp 中 SkillManager 创建后调用）
    void setSkillManager(SkillManager *sm);

    // ── 发送与控制 ────────────────────────────────────────────────────────────
    Q_INVOKABLE void sendMessage(const QString &text);
    Q_INVOKABLE void stopGeneration();

    // ── 消息编辑 ──────────────────────────────────────────────────────────────
    Q_INVOKABLE void editMessage(int index, const QString &content);
    Q_INVOKABLE void editAndRegenerate(int index, const QString &content);  // 编辑后重新生成
    Q_INVOKABLE void setUserMessageVersion(int index, int dfsPosition);     // 切换用户消息历史版本 (X/Y)
    Q_INVOKABLE void deleteMessage(int index);
    Q_INVOKABLE void resendFrom(int index);          // 从某条消息重新生成
    Q_INVOKABLE void copyToClipboard(const QString &text);

    // ── 会话管理 ──────────────────────────────────────────────────────────────
    Q_INVOKABLE void newSession(const QString &name = QString(), bool addWelcome = false);
    Q_INVOKABLE void loadSession(const QString &id);
    Q_INVOKABLE void clearMessages();
    Q_INVOKABLE void renameSession(const QString &name);

    // 导出当前对话为 Markdown
    Q_INVOKABLE bool exportCurrentChat(const QString &filePath);

    // 提供给 Web 前端一次性获取当前会话的所有消息，用于 JS 渲染整块对话
    Q_INVOKABLE QVariantList getMessages() const { return m_messagesModel.toVariantList(); }

signals:
    void currentSessionChanged();
    void sessionNameChanged();
    void isStreamingChanged();
    void chatModeChanged();
    void errorOccurred(const QString &msg);

private:
    Settings *m_settings;
    History  *m_history;

    MessageModel              m_messagesModel;
    QString                   m_currentSession;
    QString                   m_sessionName;
    QString                   m_chatMode = QStringLiteral("ask");
    bool                      m_isStreaming = false;
    QPointer<QNetworkReply>   m_activeReply;
    QPointer<QNetworkReply>   m_titleReply;
    QNetworkAccessManager    *m_nam;

    AgentCore    *m_agentCore = nullptr;
    LLMBackend   *m_llmBackend = nullptr;
    MemoryModule *m_agentMemory = nullptr;
    ToolRegistry *m_toolRegistry = nullptr;
    SkillManager *m_skillManager = nullptr;

    void   setIsStreaming(bool v);
    void   appendMessage(const QVariantMap &msg);
    void   updateLastAiMessage(const QString &reasoningChunk,
                               const QString &contentChunk,
                               bool isThinking);

    void   saveCurrentSession();
    void   loadSessionFile(const QString &id);
    void   applyLlmForCurrentChatMode();
    void   configureAgentForChatMode();
    void   startApiCall(const QVariantList &history, const QString &ragContext = QString());
    void   startAgentCall(const QString &userInput);
    void   startRagFetchAndApiCall(const QString &userQuery);
    void   onQueryRewriteDone(const QString &userQuery, const QString &rewrittenQuery,
                             int rewriteDurationMs, const QString &rewriteThinking);
    void   onRagSearchDone(const QString &userQuery,
                          const QVector<SearchResult> &results,
                          int rewriteDurationMs, const QString &rewriteThinking, int searchDurationMs);
    void   doStartApiCall(const QString &ragContext);
    QString fetchRagContext(const QString &userQuery) const;

    void   autoNameCurrentSession();   // 对话结束后调用 LLM 生成标题
    void   requestSessionTitle();      // 发起标题生成请求（异步）

    QVariantList buildApiMessages() const;
    void   setupAgent();

    // 联网 Query 重写思考内容的前端“流式”展示控制（客户端逐段写入 rewriteThinking）
    QString m_rewriteFullThinking;
    int     m_rewriteStreamPos = 0;
    bool    m_rewriteStreaming = false;
    void   startRewriteThinkingStream(int rewriteDurationMs, const QString &fullThinking);
    void   appendRewriteThinkingChunk(int rewriteDurationMs);
};

#endif // MAINVIEW_H

