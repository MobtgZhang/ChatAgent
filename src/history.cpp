#include "history.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <algorithm>

// ── 构造 ──────────────────────────────────────────────────────────────────────
History::History(QObject *parent) : QObject(parent) {
    QDir().mkpath(dataDir());
    loadIndex();
    rebuildFlat();
}

// ── 路径 ──────────────────────────────────────────────────────────────────────
QString History::dataDir() const {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + "/sessions";
}

QString History::indexPath() const {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + "/history_index.json";
}

QString History::sessionFilePath(const QString &id) const {
    return dataDir() + "/" + id + ".json";
}

// ── ID 生成 ───────────────────────────────────────────────────────────────────
QString History::generateId() const {
    return QUuid::createUuid().toString(QUuid::WithoutBraces).left(12);
}

// ── 索引查找 ──────────────────────────────────────────────────────────────────
int History::findIndex(const QString &id) const {
    for (int i = 0; i < m_nodes.size(); ++i)
        if (m_nodes[i].id == id) return i;
    return -1;
}

// ── 加载索引 ──────────────────────────────────────────────────────────────────
void History::loadIndex() {
    QFile f(indexPath());
    if (!f.open(QIODevice::ReadOnly)) return;

    QJsonArray arr = QJsonDocument::fromJson(f.readAll()).array();
    m_nodes.clear();
    for (const QJsonValue &v : arr) {
        QJsonObject o = v.toObject();
        HistoryNode node;
        node.id        = o["id"].toString();
        node.type      = o["type"].toString();
        node.name      = o["name"].toString();
        node.parentId  = o["parentId"].toString();
        node.expanded  = o["expanded"].toBool(true);
        node.createdAt = o["createdAt"].toVariant().toLongLong();
        node.updatedAt = o["updatedAt"].toVariant().toLongLong();
        if (!node.id.isEmpty() && !node.type.isEmpty())
            m_nodes.append(node);
    }
}

// ── 保存索引 ──────────────────────────────────────────────────────────────────
void History::saveIndex() {
    QJsonArray arr;
    for (const HistoryNode &n : m_nodes) {
        QJsonObject o;
        o["id"]        = n.id;
        o["type"]      = n.type;
        o["name"]      = n.name;
        o["parentId"]  = n.parentId;
        o["expanded"]  = n.expanded;
        o["createdAt"] = n.createdAt;
        o["updatedAt"] = n.updatedAt;
        arr.append(o);
    }
    QFile f(indexPath());
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

// ── 重建扁平列表（DFS 遍历，折叠文件夹时隐藏子节点）────────────────────────
void History::rebuildFlat() {
    m_flatNodes.clear();
    appendChildren(QString(), 0, m_flatNodes);
    emit flatNodesChanged();
}

void History::appendChildren(const QString &parentId, int depth,
                              QVariantList &out) const {
    // 收集该父节点的直接子节点，按 updatedAt 倒序排列
    QList<HistoryNode> children;
    for (const HistoryNode &n : m_nodes)
        if (n.parentId == parentId)
            children.append(n);

    std::sort(children.begin(), children.end(),
              [](const HistoryNode &a, const HistoryNode &b) {
                  // 文件夹优先，再按 updatedAt 倒序
                  if (a.type != b.type)
                      return a.type == "folder";
                  return a.updatedAt > b.updatedAt;
              });

    for (const HistoryNode &n : children) {
        // 统计是否有子节点
        bool hasChildren = std::any_of(m_nodes.begin(), m_nodes.end(),
                                       [&](const HistoryNode &c){ return c.parentId == n.id; });
        QVariantMap item;
        item["id"]          = n.id;
        item["type"]        = n.type;
        item["name"]        = n.name;
        item["depth"]       = depth;
        item["expanded"]    = n.expanded;
        item["hasChildren"] = hasChildren;
        item["updatedAt"]   = n.updatedAt;
        item["parentId"]    = n.parentId;
        out.append(item);

        // 只有展开的文件夹才继续递归子节点
        if (n.type == "folder" && n.expanded)
            appendChildren(n.id, depth + 1, out);
    }
}

// ── 创建会话 ──────────────────────────────────────────────────────────────────
QString History::createSession(const QString &name, const QString &parentId) {
    HistoryNode node;
    node.id        = generateId();
    node.type      = "session";
    node.name      = name.isEmpty() ? "新对话" : name;
    node.parentId  = parentId;
    node.createdAt = QDateTime::currentMSecsSinceEpoch();
    node.updatedAt = node.createdAt;
    m_nodes.prepend(node);   // 最新在前
    saveIndex();
    rebuildFlat();
    return node.id;
}

// ── 创建文件夹 ────────────────────────────────────────────────────────────────
QString History::createFolder(const QString &name, const QString &parentId) {
    HistoryNode node;
    node.id        = generateId();
    node.type      = "folder";
    node.name      = name.isEmpty() ? "新文件夹" : name;
    node.parentId  = parentId;
    node.createdAt = QDateTime::currentMSecsSinceEpoch();
    node.updatedAt = node.createdAt;
    m_nodes.prepend(node);
    saveIndex();
    rebuildFlat();
    return node.id;
}

// ── 删除节点（递归删除子节点和会话文件）─────────────────────────────────────
void History::deleteNode(const QString &id) {
    int idx = findIndex(id);
    if (idx < 0) return;

    HistoryNode node = m_nodes[idx];

    // 递归删除子节点
    if (node.type == "folder") {
        QStringList childIds;
        for (const HistoryNode &n : m_nodes)
            if (n.parentId == id) childIds.append(n.id);
        for (const QString &cid : childIds)
            deleteNode(cid);
    }

    // 删除会话文件
    if (node.type == "session")
        QFile::remove(sessionFilePath(id));

    m_nodes.removeAt(findIndex(id));   // 重新查找，因为递归可能改变了下标
    saveIndex();
    rebuildFlat();
}

// ── 重命名 ────────────────────────────────────────────────────────────────────
void History::renameNode(const QString &id, const QString &name) {
    int idx = findIndex(id);
    if (idx < 0 || name.trimmed().isEmpty()) return;
    m_nodes[idx].name      = name.trimmed();
    m_nodes[idx].updatedAt = QDateTime::currentMSecsSinceEpoch();
    saveIndex();
    rebuildFlat();
}

// ── 展开/折叠文件夹 ───────────────────────────────────────────────────────────
void History::toggleExpand(const QString &id) {
    int idx = findIndex(id);
    if (idx < 0 || m_nodes[idx].type != "folder") return;
    m_nodes[idx].expanded = !m_nodes[idx].expanded;
    saveIndex();
    rebuildFlat();
}

// ── 移动节点 ──────────────────────────────────────────────────────────────────
void History::moveNode(const QString &id, const QString &newParentId) {
    int idx = findIndex(id);
    if (idx < 0) return;
    // 防止移动到自身或后代
    if (id == newParentId) return;

    // 检查 newParentId 是否是 id 的后代（向上追溯父链）
    QString curParent = newParentId;
    while (!curParent.isEmpty()) {
        if (curParent == id)
            return;
        const int pIdx = findIndex(curParent);
        if (pIdx < 0)
            break;
        curParent = m_nodes[pIdx].parentId;
    }
    m_nodes[idx].parentId  = newParentId;
    m_nodes[idx].updatedAt = QDateTime::currentMSecsSinceEpoch();
    saveIndex();
    rebuildFlat();
}

// ── 更新会话时间戳 ────────────────────────────────────────────────────────────
void History::touchSession(const QString &id) {
    int idx = findIndex(id);
    if (idx < 0) return;
    m_nodes[idx].updatedAt = QDateTime::currentMSecsSinceEpoch();
    saveIndex();
    rebuildFlat();
}

// ── 获取第一个会话 ID ─────────────────────────────────────────────────────────
QString History::firstSessionId() const {
    for (const HistoryNode &n : m_nodes)
        if (n.type == "session") return n.id;
    return QString();
}

