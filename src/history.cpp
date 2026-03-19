#include "history.h"
#include "historylistmodel.h"
#include "settings.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <QSet>
#include <algorithm>

// ── 构造 ──────────────────────────────────────────────────────────────────────
History::History(QObject *parent) : QObject(parent), m_flatModel(new HistoryListModel(this)) {
    QDir().mkpath(dataDir());
    loadIndex();
    rebuildFlat();
}

History::History(Settings *settings, QObject *parent)
    : QObject(parent), m_settings(settings), m_flatModel(new HistoryListModel(this)) {
    migrateOldData();
    QDir().mkpath(dataDir());
    loadIndex();
    rebuildFlat();
}

// ── 路径（使用用户设置的缓存目录存储历史记录和配置）────────────────────────────
QString History::baseDir() const {
    QString root = m_settings
        ? m_settings->effectiveDataDirectory()
        : QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return root + "/history";
}

QString History::dataDir() const {
    return baseDir() + "/sessions";
}

QString History::indexPath() const {
    return baseDir() + "/index.json";
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
        node.order     = o["order"].toInt(0);
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
        o["order"]     = n.order;
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
    m_flatModel->setFlatNodes(m_flatNodes);
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
                  // 先按 order，再文件夹优先，再按 updatedAt 倒序
                  if (a.order != b.order)
                      return a.order < b.order;
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

// ── 辅助：为新节点分配 order（放在同层最前）────────────────────────────────────
static void assignOrderToNewNode(QList<HistoryNode> &nodes, int newNodeIdx,
                                const QString &parentId) {
    for (int i = 0; i < nodes.size(); ++i) {
        if (i != newNodeIdx && nodes[i].parentId == parentId)
            nodes[i].order++;  // 同层兄弟后移
    }
    nodes[newNodeIdx].order = 0;  // 新节点放最前
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
    m_nodes.prepend(node);
    assignOrderToNewNode(m_nodes, 0, parentId);
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
    assignOrderToNewNode(m_nodes, 0, parentId);
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

// ── 更新会话文件中的 name 字段（用于异步标题生成后同步到文件）────────────────────
void History::updateSessionNameInFile(const QString &id, const QString &name) {
    if (id.isEmpty() || name.trimmed().isEmpty()) return;
    QFile f(sessionFilePath(id));
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    f.close();
    root["name"] = name.trimmed();
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

// ── 展开/折叠文件夹 ───────────────────────────────────────────────────────────
void History::toggleExpand(const QString &id) {
    int idx = findIndex(id);
    if (idx < 0 || m_nodes[idx].type != "folder") return;
    m_nodes[idx].expanded = !m_nodes[idx].expanded;
    saveIndex();
    rebuildFlat();
}

// ── 同层拖拽排序（movedId 与 targetId 必须为同一父节点下的兄弟）────────────
void History::reorderNode(const QString &movedId, const QString &targetId) {
    int movedIdx = findIndex(movedId);
    int targetIdx = findIndex(targetId);
    if (movedIdx < 0 || targetIdx < 0) return;
    if (m_nodes[movedIdx].parentId != m_nodes[targetIdx].parentId) return;

    const QString parentId = m_nodes[movedIdx].parentId;

    // 收集同层兄弟按当前顺序
    QList<HistoryNode> siblings;
    for (const HistoryNode &n : m_nodes)
        if (n.parentId == parentId)
            siblings.append(n);

    std::sort(siblings.begin(), siblings.end(),
              [](const HistoryNode &a, const HistoryNode &b) { return a.order < b.order; });

    // 找到 movedId 和 targetId 在 siblings 中的位置
    int fromPos = -1, toPos = -1;
    for (int i = 0; i < siblings.size(); ++i) {
        if (siblings[i].id == movedId) fromPos = i;
        if (siblings[i].id == targetId) toPos = i;
    }
    if (fromPos < 0 || toPos < 0 || fromPos == toPos) return;

    // 从 siblings 中移除 moved，插入到 toPos
    HistoryNode movedNode = siblings.takeAt(fromPos);
    int insertPos = toPos;
    if (fromPos < toPos) insertPos--;
    siblings.insert(insertPos, movedNode);

    // 重新分配 order
    for (int i = 0; i < siblings.size(); ++i) {
        int idx = findIndex(siblings[i].id);
        if (idx >= 0)
            m_nodes[idx].order = i;
    }
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
    // 移到新父节点后，放在同层最后
    int maxOrder = -1;
    for (const HistoryNode &n : m_nodes) {
        if (n.parentId == newParentId && n.id != id)
            maxOrder = qMax(maxOrder, n.order);
    }
    m_nodes[idx].order = maxOrder + 1;
    saveIndex();
    rebuildFlat();
}

// ── 获取“添加到文件夹”的选项列表（空=根级，名称为"(空)"；其余为文件夹）────────
QVariantList History::getFolderOptions(const QString &movingNodeId) const {
    QVariantList out;
    out.append(QVariantMap{{"id", QString()}, {"name", QStringLiteral("(空)")}});

    // 收集不能作为目标的后代 id（若 movingNodeId 是文件夹）
    QSet<QString> excludeIds;
    excludeIds.insert(movingNodeId);
    int idx = findIndex(movingNodeId);
    if (idx >= 0 && m_nodes[idx].type == "folder") {
        for (const HistoryNode &n : m_nodes) {
            QString pid = n.parentId;
            while (!pid.isEmpty()) {
                if (pid == movingNodeId) {
                    excludeIds.insert(n.id);
                    break;
                }
                int i = findIndex(pid);
                if (i < 0) break;
                pid = m_nodes[i].parentId;
            }
        }
    }

    for (const HistoryNode &n : m_nodes) {
        if (n.type != "folder" || excludeIds.contains(n.id))
            continue;
        out.append(QVariantMap{{"id", n.id}, {"name", n.name}});
    }
    return out;
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

// ── 从旧路径迁移数据 ──────────────────────────────────────────────────────────
void History::migrateOldData() {
    QString root = m_settings
        ? m_settings->effectiveDataDirectory()
        : QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    QString oldIndex = root + "/history_index.json";
    QString oldSessions = root + "/sessions";
    QString newBase = baseDir();
    QString newIndex = indexPath();
    QString newSessions = dataDir();

    if (!QFile::exists(oldIndex))
        return;
    if (QFile::exists(newIndex))
        return;

    QDir().mkpath(newBase);
    QDir().mkpath(newSessions);

    QFile::copy(oldIndex, newIndex);

    QDir oldDir(oldSessions);
    if (oldDir.exists()) {
        const QStringList files = oldDir.entryList(QDir::Files);
        for (const QString &f : files)
            QFile::copy(oldSessions + "/" + f, newSessions + "/" + f);
    }
}

