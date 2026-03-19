#include "historylistmodel.h"

HistoryListModel::HistoryListModel(QObject *parent)
    : QAbstractListModel(parent) {}

int HistoryListModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return m_nodes.size();
}

QVariant HistoryListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_nodes.size())
        return QVariant();

    const QVariantMap &node = m_nodes[index.row()].toMap();

    switch (role) {
    case IdRole:          return node.value("id");
    case TypeRole:        return node.value("type");
    case NameRole:        return node.value("name");
    case DepthRole:       return node.value("depth");
    case ExpandedRole:    return node.value("expanded");
    case HasChildrenRole: return node.value("hasChildren");
    case UpdatedAtRole:   return node.value("updatedAt");
    case ParentIdRole:    return node.value("parentId");
    default:              return QVariant();
    }
}

QHash<int, QByteArray> HistoryListModel::roleNames() const {
    return {
        { IdRole,          "nodeId" },
        { TypeRole,        "nodeType" },
        { NameRole,        "nodeName" },
        { DepthRole,       "nodeDepth" },
        { ExpandedRole,    "nodeExpanded" },
        { HasChildrenRole, "nodeHasChildren" },
        { UpdatedAtRole,   "nodeUpdatedAt" },
        { ParentIdRole,    "nodeParentId" },
    };
}

void HistoryListModel::setFlatNodes(const QVariantList &nodes) {
    if (m_nodes == nodes) return;

    const int oldCount = m_nodes.size();
    beginResetModel();
    m_nodes = nodes;
    endResetModel();
    if (m_nodes.size() != oldCount)
        emit countChanged();
}
