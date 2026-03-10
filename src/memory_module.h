#ifndef MEMORY_MODULE_H
#define MEMORY_MODULE_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

/**
 * Agent 记忆模块（自进化支持）
 * 短期：滚动窗口对话历史（内存）
 * 长期 L2：SQLite facts 表 - 用户偏好/环境事实
 * 长期 L3：SQLite sops 表 - 学到的任务 SOP（自生长）
 * 教训层：SQLite lessons 表 - 从事件中提炼的教训（OpenClaw 风格）
 */
class MemoryModule : public QObject {
    Q_OBJECT

    Q_PROPERTY(QVariantList shortTermMessages READ shortTermMessages NOTIFY shortTermMessagesChanged)
    Q_PROPERTY(QVariantList longTermFacts READ longTermFacts NOTIFY longTermFactsChanged)

public:
    explicit MemoryModule(QObject *parent = nullptr);
    ~MemoryModule();

    // 短期记忆（最近 N 轮对话）
    QVariantList shortTermMessages() const { return m_shortTerm; }
    void appendShortTerm(const QVariantMap &msg);
    void clearShortTerm();
    void setShortTermWindow(int count) { m_windowSize = qMax(1, count); }

    // 长期记忆 L2（facts）
    QVariantList longTermFacts() const { return m_longTerm; }
    Q_INVOKABLE void addFact(const QString &key, const QString &value);
    Q_INVOKABLE void removeFact(const QString &key);
    Q_INVOKABLE void clearLongTerm();
    Q_INVOKABLE QString getFact(const QString &key) const;

    // 长期记忆 L3（SOP - 自进化）
    Q_PROPERTY(QVariantList sopsList READ sopsList NOTIFY sopsChanged)
    Q_INVOKABLE bool addSop(const QString &name, const QString &content, const QString &summary = QString());
    Q_INVOKABLE bool removeSop(const QString &name);
    Q_INVOKABLE QString getSop(const QString &name) const;
    Q_INVOKABLE QVariantList listSops(const QString &keyword = QString()) const;
    Q_INVOKABLE void incrementSopUsage(const QString &name);
    Q_INVOKABLE void refreshSops();

    // 教训层（Events → Lessons，OpenClaw 风格）
    Q_INVOKABLE bool addLesson(const QString &eventSummary, const QString &lesson, double confidence = 0.8);
    Q_INVOKABLE QVariantList getLessons(int limit = 10) const;

    // 刷新长期记忆（从 DB 加载）
    Q_INVOKABLE void refreshLongTerm();
    QVariantList sopsList() const { return m_sopsList; }

    // 构建注入到 prompt 的上下文
    QString buildContextForPrompt() const;

signals:
    void shortTermMessagesChanged();
    void longTermFactsChanged();
    void sopsChanged();

private:
    void trimShortTerm();
    void initDb();

    QVariantList m_shortTerm;
    QVariantList m_longTerm;
    QVariantList m_sopsList;
    int m_windowSize = 20;

    struct Db;
    Db *m_db = nullptr;
};

#endif // MEMORY_MODULE_H
