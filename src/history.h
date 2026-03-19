#ifndef HISTORY_H
#define HISTORY_H

#include <QObject>

class Settings;
#include <QVariantList>
#include <QString>
#include <QList>
#include <QDateTime>
#include <QAbstractListModel>

#include "historylistmodel.h"

// ── 树节点结构体 ──────────────────────────────────────────────────────────────
struct HistoryNode {
    QString  id;
    QString  type;      // "folder" | "session"
    QString  name;
    QString  parentId;  // 空字符串 = 根节点
    bool     expanded = true;
    int      order = 0; // 同层兄弟节点的排序（用于拖拽调整）
    qint64   createdAt = 0;
    qint64   updatedAt = 0;
};

// ── 历史管理器 ────────────────────────────────────────────────────────────────
class History : public QObject {
    Q_OBJECT

    // 供 QML ListView 使用的模型，确保点击加载和视图更新正确工作
    Q_PROPERTY(QAbstractListModel* flatModel READ flatModel CONSTANT)

public:
    explicit History(QObject *parent = nullptr);
    explicit History(Settings *settings, QObject *parent = nullptr);

    QAbstractListModel* flatModel() const { return m_flatModel; }

    // ── 节点操作 ──────────────────────────────────────────────────────────────
    Q_INVOKABLE QString createSession(const QString &name,
                                      const QString &parentId = QString());
    Q_INVOKABLE QString createFolder(const QString &name,
                                     const QString &parentId = QString());
    Q_INVOKABLE void    deleteNode(const QString &id);
    Q_INVOKABLE void    renameNode(const QString &id, const QString &name);
    Q_INVOKABLE void    toggleExpand(const QString &id);
    Q_INVOKABLE void    moveNode(const QString &id, const QString &newParentId);
    Q_INVOKABLE void    reorderNode(const QString &movedId, const QString &targetId);
    Q_INVOKABLE QVariantList getFolderOptions(const QString &movingNodeId) const;

    // ── 会话文件操作 ──────────────────────────────────────────────────────────
    Q_INVOKABLE QString sessionFilePath(const QString &id) const;
    Q_INVOKABLE void    touchSession(const QString &id); // 更新 updatedAt
    Q_INVOKABLE void    updateSessionNameInFile(const QString &id, const QString &name);

    // ── 工具 ──────────────────────────────────────────────────────────────────
    Q_INVOKABLE QString firstSessionId() const;          // 最近的 session id

signals:
    void flatNodesChanged();

private:
    Settings           *m_settings = nullptr;
    QList<HistoryNode>  m_nodes;
    QVariantList        m_flatNodes;
    HistoryListModel   *m_flatModel;

    QString baseDir()     const;  // 缓存目录根（用户设置或默认 AppDataLocation）
    QString dataDir()     const;
    QString indexPath()   const;

    void loadIndex();
    void saveIndex();
    void rebuildFlat();          // 重建扁平列表
    void migrateOldData();       // 从旧路径迁移数据

    // DFS 辅助：递归添加子节点到 flat list
    void appendChildren(const QString &parentId, int depth,
                        QVariantList &out) const;

    QString generateId() const;
    int     findIndex(const QString &id) const;  // 在 m_nodes 中查找
};

#endif // HISTORY_H

