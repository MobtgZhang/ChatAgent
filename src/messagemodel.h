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

private:
    QVector<QVariantMap> m_items;
};

#endif // MESSAGEMODEL_H

