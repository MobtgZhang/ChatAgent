#ifndef MAINVIEW_H
#define MAINVIEW_H

#include <QObject>
#include <QVariantList>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QAbstractItemModel>

#include "messagemodel.h"

class Settings;
class History;

class MainView : public QObject {
    Q_OBJECT

    Q_PROPERTY(QAbstractItemModel* messagesModel READ messagesModel CONSTANT)
    Q_PROPERTY(QString      currentSession  READ currentSession  NOTIFY currentSessionChanged)
    Q_PROPERTY(QString      sessionName     READ sessionName     NOTIFY sessionNameChanged)
    Q_PROPERTY(bool         isStreaming     READ isStreaming     NOTIFY isStreamingChanged)

public:
    explicit MainView(Settings *settings, History *history,
                      QObject *parent = nullptr);

    QAbstractItemModel* messagesModel() { return &m_messagesModel; }
    QString      currentSession() const { return m_currentSession; }
    QString      sessionName()    const { return m_sessionName; }
    bool         isStreaming()    const { return m_isStreaming; }

    // ── 发送与控制 ────────────────────────────────────────────────────────────
    Q_INVOKABLE void sendMessage(const QString &text);
    Q_INVOKABLE void stopGeneration();

    // ── 消息编辑 ──────────────────────────────────────────────────────────────
    Q_INVOKABLE void editMessage(int index, const QString &content);
    Q_INVOKABLE void deleteMessage(int index);
    Q_INVOKABLE void resendFrom(int index);          // 从某条消息重新生成

    // ── 会话管理 ──────────────────────────────────────────────────────────────
    Q_INVOKABLE void newSession(const QString &name = QString());
    Q_INVOKABLE void loadSession(const QString &id);
    Q_INVOKABLE void clearMessages();
    Q_INVOKABLE void renameSession(const QString &name);

signals:
    void currentSessionChanged();
    void sessionNameChanged();
    void isStreamingChanged();
    void errorOccurred(const QString &msg);

private:
    Settings *m_settings;
    History  *m_history;

    MessageModel              m_messagesModel;
    QString                   m_currentSession;
    QString                   m_sessionName;
    bool                      m_isStreaming = false;
    QPointer<QNetworkReply>   m_activeReply;
    QNetworkAccessManager    *m_nam;

    void   setIsStreaming(bool v);
    void   appendMessage(const QVariantMap &msg);
    void   updateLastAiMessage(const QString &reasoningChunk,
                               const QString &contentChunk,
                               bool isThinking);

    void   saveCurrentSession();
    void   loadSessionFile(const QString &id);
    void   startApiCall(const QVariantList &history);

    void   autoNameCurrentSession();   // 根据对话内容自动为会话命名

    QVariantList buildApiMessages() const;
};

#endif // MAINVIEW_H

