#ifndef HISTORYLISTMODEL_H
#define HISTORYLISTMODEL_H

#include <QAbstractListModel>
#include <QVariantList>

// 用于 QML ListView 的历史树扁平列表模型，确保正确触发视图更新
class HistoryListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        TypeRole,
        NameRole,
        DepthRole,
        ExpandedRole,
        HasChildrenRole,
        UpdatedAtRole,
        ParentIdRole,
    };
    Q_ENUM(Role)

    explicit HistoryListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setFlatNodes(const QVariantList &nodes);

private:
    QVariantList m_nodes;
};

#endif // HISTORYLISTMODEL_H
