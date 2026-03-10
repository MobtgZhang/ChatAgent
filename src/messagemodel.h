#ifndef MESSAGEMODEL_H
#define MESSAGEMODEL_H

#include <QAbstractListModel>
#include <QVector>
#include <QVariantMap>

class MessageModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        RoleRole = Qt::UserRole + 1,
        ContentRole,
        ThinkingRole,
        IsThinkingRole,
        IdRole,
        BlocksRole,   // agent/planning 模式：分块 [thinking,reply,tool,...]
        RagSearchStatusRole,  // chat 模式联网时："" | "searching" | "done"
        RagLinksRole,        // 搜索得到的链接列表 [{text, url, snippet, index}]，可点击打开
        RewriteDurationMsRole,   // 查询重写耗时（毫秒）
        RewriteThinkingRole,     // 重写思考过程（LLM reasoning 或改写结果）
        SearchDurationMsRole,    // 搜索耗时（毫秒）
        EditHistoryRole,     // 用户消息编辑历史树 [{content, parentIndex}, ...]，parentIndex -1 为根
        CurrentEditIndexRole,// 当前节点的 flat 索引
        EditVersionRole,     // (X/Y) 中的 X，深度优先序中的 1-based 位置
        EditTotalRole,       // (X/Y) 中的 Y，总版本数
    };
    Q_ENUM(Role)

    explicit MessageModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int size() const { return m_items.size(); }
    bool isEmpty() const { return m_items.isEmpty(); }

    QVariantMap at(int row) const;
    QVariantList toVariantList() const;

    void clearAll();
    void appendOne(const QVariantMap &msg);
    void removeAtRow(int row);
    void truncateTo(int newSize);
    void replaceAtRow(int row, const QVariantMap &msg);

    void updateLastAiMessageAppend(const QString &reasoningChunk,
                                   const QString &contentChunk,
                                   bool isThinking);

    // agent/planning 模式：分块更新（思考→回复→工具 分开）
    void updateLastAiMessageAgent(const QString &reasoningChunk,
                                  const QString &contentChunk,
                                  bool isThinking);
    void appendToolBlockToLastAiMessage(const QString &toolName,
                                       const QVariantMap &args,
                                       const QString &result,
                                       double durationSec = 0.0);

    void updateLastAiMessageRagSearchStatus(const QString &status);
    void updateLastAiMessageRagLinks(const QVariantList &links);
    void updateLastAiMessageRagMeta(int rewriteDurationMs, const QString &rewriteThinking, int searchDurationMs);

    // 用户消息编辑历史：追加新版本、切换版本；每个版本有独立 branchTail（后续对话）
    void appendEditHistoryNode(int row, const QString &newContent, int parentNodeIndex);
    void setUserMessageEditIndex(int row, int dfsPosition);
    void persistCurrentBranchTails();  // 保存前将当前 tail 写回 branchTails

private:
    QVector<QVariantMap> m_items;
};

#endif // MESSAGEMODEL_H

