#include "messagemodel.h"

#include <QDateTime>
#include <QMap>
#include <functional>

// 从 flat 树结构计算深度优先顺序，用于 (X/Y) 导航
static QList<int> depthFirstOrder(const QVariantList &nodes) {
    QList<int> result;
    if (nodes.isEmpty()) return result;

    QMap<int, QList<int>> parentToChildren;
    for (int i = 0; i < nodes.size(); ++i) {
        int p = nodes.at(i).toMap().value("parentIndex", -1).toInt();
        parentToChildren[p].append(i);
    }

    std::function<void(int)> dfs = [&](int parentIdx) {
        for (int c : parentToChildren.value(parentIdx)) {
            result.append(c);
            dfs(c);
        }
    };
    dfs(-1);
    return result;
}

MessageModel::MessageModel(QObject *parent)
    : QAbstractListModel(parent) {}

int MessageModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return m_items.size();
}

QVariant MessageModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid()) return {};
    const int row = index.row();
    if (row < 0 || row >= m_items.size()) return {};
    const QVariantMap &m = m_items[row];

    switch (role) {
    case RoleRole:       return m.value("role");
    case ContentRole:    return m.value("content");
    case ThinkingRole:   return m.value("thinking");
    case IsThinkingRole: return m.value("isThinking");
    case IdRole:         return m.value("id");
    case BlocksRole:     return m.value("blocks");
    case RagSearchStatusRole: return m.value("ragSearchStatus");
    case RagLinksRole: return m.value("ragLinks");
    case RewriteDurationMsRole: return m.value("rewriteDurationMs");
    case RewriteThinkingRole: return m.value("rewriteThinking");
    case SearchDurationMsRole: return m.value("searchDurationMs");
    case EditHistoryRole: return m.value("editHistory");
    case CurrentEditIndexRole: return m.value("currentEditIndex");
    case EditVersionRole: {
        QVariantList hist = m.value("editHistory").toList();
        if (hist.isEmpty()) return 1;
        QList<int> dfs = depthFirstOrder(hist);
        int flat = m.value("currentEditIndex", 0).toInt();
        int pos = dfs.indexOf(flat);
        return (pos >= 0 ? pos : 0) + 1;
    }
    case EditTotalRole: {
        QVariantList hist = m.value("editHistory").toList();
        if (hist.isEmpty()) return 1;
        return depthFirstOrder(hist).size();
    }
    default:             return {};
    }
}

QHash<int, QByteArray> MessageModel::roleNames() const {
    return {
        { RoleRole,       "role" },
        { ContentRole,    "content" },
        { ThinkingRole,   "thinking" },
        { IsThinkingRole, "isThinking" },
        { IdRole,         "id" },
        { BlocksRole,     "blocks" },
        { RagSearchStatusRole, "ragSearchStatus" },
        { RagLinksRole, "ragLinks" },
        { RewriteDurationMsRole, "rewriteDurationMs" },
        { RewriteThinkingRole, "rewriteThinking" },
        { SearchDurationMsRole, "searchDurationMs" },
        { EditHistoryRole, "editHistory" },
        { CurrentEditIndexRole, "currentEditIndex" },
        { EditVersionRole, "editVersion" },
        { EditTotalRole, "editTotal" },
    };
}

QVariantMap MessageModel::at(int row) const {
    if (row < 0 || row >= m_items.size()) return {};
    return m_items[row];
}

QVariantList MessageModel::toVariantList() const {
    QVariantList out;
    out.reserve(m_items.size());
    for (const auto &m : m_items) out.append(m);
    return out;
}

void MessageModel::clearAll() {
    beginResetModel();
    m_items.clear();
    endResetModel();
}

void MessageModel::appendOne(const QVariantMap &msg) {
    const int row = m_items.size();
    beginInsertRows(QModelIndex(), row, row);
    m_items.push_back(msg);
    endInsertRows();
}

void MessageModel::removeAtRow(int row) {
    if (row < 0 || row >= m_items.size()) return;
    beginRemoveRows(QModelIndex(), row, row);
    m_items.removeAt(row);
    endRemoveRows();
}

void MessageModel::truncateTo(int newSize) {
    if (newSize < 0) newSize = 0;
    if (newSize >= m_items.size()) return;

    beginRemoveRows(QModelIndex(), newSize, m_items.size() - 1);
    while (m_items.size() > newSize) m_items.removeLast();
    endRemoveRows();
}

void MessageModel::replaceAtRow(int row, const QVariantMap &msg) {
    if (row < 0 || row >= m_items.size()) return;
    m_items[row] = msg;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {RoleRole, ContentRole, ThinkingRole, IsThinkingRole, IdRole, BlocksRole, RagSearchStatusRole, RagLinksRole, EditHistoryRole, CurrentEditIndexRole, EditVersionRole, EditTotalRole});
}

void MessageModel::updateLastAiMessageAgent(const QString &reasoningChunk,
                                            const QString &contentChunk,
                                            bool isThinking) {
    if (m_items.isEmpty()) return;
    const int row = m_items.size() - 1;
    QVariantMap last = m_items[row];
    if (last.value("role").toString() != QStringLiteral("ai")) return;

    QVariantList blocks = last.value("blocks").toList();
    const qint64 nowMsec = QDateTime::currentMSecsSinceEpoch();

    auto finalizeThinkingBlock = [&blocks, nowMsec]() {
        if (blocks.isEmpty()) return;
        QVariantMap b = blocks.last().toMap();
        if (b.value("type").toString() != QStringLiteral("thinking")) return;
        qint64 start = b.value("startMsec", nowMsec).toLongLong();
        b["durationSec"] = (nowMsec - start) / 1000.0;
        b.remove("startMsec");
        blocks.last() = b;
    };

    auto appendToLastBlock = [&blocks, nowMsec, &finalizeThinkingBlock](const QString &type, const QString &chunk) {
        if (chunk.isEmpty()) return;
        if (!blocks.isEmpty()) {
            QVariantMap b = blocks.last().toMap();
            if (b.value("type").toString() == type) {
                b["content"] = b.value("content").toString() + chunk;
                blocks.last() = b;
                return;
            }
            if (type == QStringLiteral("reply")) finalizeThinkingBlock();
        }
        QVariantMap nb;
        nb["type"] = type;
        nb["content"] = chunk;
        if (type == QStringLiteral("thinking"))
            nb["startMsec"] = nowMsec;
        blocks.append(nb);
    };

    if (!reasoningChunk.isEmpty()) {
        if (!blocks.isEmpty()) {
            QVariantMap b = blocks.last().toMap();
            QString t = b.value("type").toString();
            if (t == QStringLiteral("thinking")) {
                if (!b.contains("startMsec")) b["startMsec"] = nowMsec;
                b["content"] = b.value("content").toString() + reasoningChunk;
                blocks.last() = b;
            } else {
                appendToLastBlock(QStringLiteral("thinking"), reasoningChunk);
            }
        } else {
            appendToLastBlock(QStringLiteral("thinking"), reasoningChunk);
        }
    }
    if (!contentChunk.isEmpty()) {
        if (!blocks.isEmpty()) {
            QVariantMap b = blocks.last().toMap();
            QString t = b.value("type").toString();
            if (t == QStringLiteral("reply")) {
                b["content"] = b.value("content").toString() + contentChunk;
                blocks.last() = b;
            } else {
                appendToLastBlock(QStringLiteral("reply"), contentChunk);
            }
        } else {
            appendToLastBlock(QStringLiteral("reply"), contentChunk);
        }
    }

    if (!contentChunk.isEmpty()
        || (!isThinking && reasoningChunk.isEmpty() && contentChunk.isEmpty())) {
        last["isThinking"] = false;
        if (!isThinking) finalizeThinkingBlock();  // LLM 结束且最后为思考块时写入时长
    } else {
        last["isThinking"] = isThinking;
    }
    last["blocks"] = blocks;
    last["thinking"] = last.value("thinking").toString() + reasoningChunk;
    last["content"] = last.value("content").toString() + contentChunk;

    m_items[row] = last;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {ContentRole, ThinkingRole, IsThinkingRole, BlocksRole});
}

void MessageModel::appendToolBlockToLastAiMessage(const QString &toolName,
                                                  const QVariantMap &args,
                                                  const QString &result,
                                                  double durationSec) {
    if (m_items.isEmpty()) return;
    const int row = m_items.size() - 1;
    QVariantMap last = m_items[row];
    if (last.value("role").toString() != QStringLiteral("ai")) return;

    QVariantList blocks = last.value("blocks").toList();
    const qint64 nowMsec = QDateTime::currentMSecsSinceEpoch();
    if (!blocks.isEmpty()) {
        QVariantMap b = blocks.last().toMap();
        if (b.value("type").toString() == QStringLiteral("thinking")) {
            qint64 start = b.value("startMsec", nowMsec).toLongLong();
            b["durationSec"] = (nowMsec - start) / 1000.0;
            b.remove("startMsec");
            blocks.last() = b;
        }
    }
    QVariantMap tb;
    tb["type"] = QStringLiteral("tool");
    tb["toolName"] = toolName;
    tb["args"] = args;
    tb["result"] = result;
    tb["durationSec"] = durationSec;
    blocks.append(tb);
    last["blocks"] = blocks;
    last["isThinking"] = true;

    m_items[row] = last;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {BlocksRole, IsThinkingRole});
}

void MessageModel::updateLastAiMessageAppend(const QString &reasoningChunk,
                                            const QString &contentChunk,
                                            bool isThinking) {
    if (m_items.isEmpty()) return;
    const int row = m_items.size() - 1;
    QVariantMap last = m_items[row];
    if (last.value("role").toString() != QStringLiteral("ai")) return;

    if (!reasoningChunk.isEmpty())
        last["thinking"] = last.value("thinking").toString() + reasoningChunk;
    if (!contentChunk.isEmpty())
        last["content"]  = last.value("content").toString()  + contentChunk;

    if (!contentChunk.isEmpty()
        || (!isThinking && reasoningChunk.isEmpty() && contentChunk.isEmpty()))
        last["isThinking"] = false;
    else
        last["isThinking"] = isThinking;

    m_items[row] = last;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {ContentRole, ThinkingRole, IsThinkingRole});
}

void MessageModel::updateLastAiMessageRagSearchStatus(const QString &status) {
    if (m_items.isEmpty()) return;
    const int row = m_items.size() - 1;
    QVariantMap last = m_items[row];
    if (last.value("role").toString() != QStringLiteral("ai")) return;
    last["ragSearchStatus"] = status;
    m_items[row] = last;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {RagSearchStatusRole});
}

void MessageModel::updateLastAiMessageRagLinks(const QVariantList &links) {
    if (m_items.isEmpty()) return;
    const int row = m_items.size() - 1;
    QVariantMap last = m_items[row];
    if (last.value("role").toString() != QStringLiteral("ai")) return;
    last["ragLinks"] = links;
    m_items[row] = last;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {RagLinksRole});
}

void MessageModel::updateLastAiMessageRagMeta(int rewriteDurationMs, const QString &rewriteThinking, int searchDurationMs) {
    if (m_items.isEmpty()) return;
    const int row = m_items.size() - 1;
    QVariantMap last = m_items[row];
    if (last.value("role").toString() != QStringLiteral("ai")) return;
    last["rewriteDurationMs"] = rewriteDurationMs;
    last["rewriteThinking"] = rewriteThinking;
    last["searchDurationMs"] = searchDurationMs;
    m_items[row] = last;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {RewriteDurationMsRole, RewriteThinkingRole, SearchDurationMsRole});
}

void MessageModel::appendEditHistoryNode(int row, const QString &newContent, int parentNodeIndex) {
    if (row < 0 || row >= m_items.size()) return;
    QVariantMap msg = m_items[row];
    if (msg.value("role").toString() != QStringLiteral("user")) return;

    // 保存当前分支 tail（该用户消息之后的所有消息），用于之后切换版本时恢复对应历史分支
    QVariantList tailMsgs;
    for (int i = row + 1; i < m_items.size(); ++i)
        tailMsgs.append(m_items.at(i));

    QVariantList history = msg.value("editHistory").toList();
    QVariantList branchTails = msg.value("branchTails").toList();
    int currentFlatIdx = msg.value("currentEditIndex", 0).toInt();

    if (history.isEmpty()) {
        // 首次建立历史：当前 content 作为根
        QVariantMap root;
        root["content"] = msg.value("content").toString();
        root["parentIndex"] = -1;
        history.append(root);

        // 新内容作为根的直接子节点
        QVariantMap newNode;
        newNode["content"] = newContent.trimmed();
        newNode["parentIndex"] = 0;
        history.append(newNode);

        // 根分支 tail = 当前 AI 回复等；新分支 tail 为空
        branchTails.append(tailMsgs);
        branchTails.append(QVariantList());
    } else {
        // 后续编辑：在当前版本或指定父节点之下新增子节点
        QList<int> dfsOrder = depthFirstOrder(history);

        // 将当前分支的 tail 保存到对应 DFS 位置
        int currentDfs = dfsOrder.indexOf(currentFlatIdx);
        if (currentDfs >= 0) {
            while (branchTails.size() < dfsOrder.size())
                branchTails.append(QVariantList());
            branchTails[currentDfs] = tailMsgs;
        }

        int parentFlatIndex = currentFlatIdx;
        if (parentNodeIndex >= 0 && parentNodeIndex < dfsOrder.size())
            parentFlatIndex = dfsOrder.at(parentNodeIndex);

        QVariantMap newNode;
        newNode["content"] = newContent.trimmed();
        newNode["parentIndex"] = parentFlatIndex;
        history.append(newNode);
        // 为新建版本追加空 tail，占位以便后续保存/恢复
        branchTails.append(QVariantList());
    }
    msg["editHistory"] = history;
    msg["branchTails"] = branchTails;
    msg["currentEditIndex"] = history.size() - 1;
    msg["content"] = newContent.trimmed();
    m_items[row] = msg;

    // 截断：移除当前 tail，新分支的 tail 将由后续 AI 回复填充
    truncateTo(row + 1);

    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {ContentRole, EditHistoryRole, CurrentEditIndexRole, EditVersionRole, EditTotalRole});
}

void MessageModel::persistCurrentBranchTails() {
    // branchTails 在 saveCurrentSession 中内联计算并序列化，此处保留空实现以兼容 API
}

void MessageModel::setUserMessageEditIndex(int row, int dfsPosition) {
    if (row < 0 || row >= m_items.size()) return;
    QVariantMap msg = m_items[row];
    if (msg.value("role").toString() != QStringLiteral("user")) return;

    QVariantList history = msg.value("editHistory").toList();
    QVariantList branchTails = msg.value("branchTails").toList();
    QList<int> dfsOrder = depthFirstOrder(history);
    if (dfsPosition < 0 || dfsPosition >= dfsOrder.size()) return;

    const int flatIdx = dfsOrder.at(dfsPosition);
    msg["currentEditIndex"] = flatIdx;
    msg["content"] = history.at(flatIdx).toMap().value("content").toString();
    m_items[row] = msg;

    // 根据 branchTails 恢复对应历史分支的后续消息
    QVariantList selectedTail;
    if (dfsPosition >= 0 && dfsPosition < branchTails.size())
        selectedTail = branchTails.at(dfsPosition).toList();

    // 先移除当前 tail
    truncateTo(row + 1);

    // 再插入选中的 tail（若存在）
    if (!selectedTail.isEmpty()) {
        const int start = m_items.size();
        const int end = start + selectedTail.size() - 1;
        beginInsertRows(QModelIndex(), start, end);
        for (const QVariant &v : selectedTail) {
            m_items.append(v.toMap());
        }
        endInsertRows();
    }

    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {ContentRole, CurrentEditIndexRole, EditVersionRole, EditTotalRole});
}

