#include "messagemodel.h"

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
    emit dataChanged(idx, idx, {RoleRole, ContentRole, ThinkingRole, IsThinkingRole, IdRole});
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

