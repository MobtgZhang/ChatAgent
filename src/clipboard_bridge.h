#ifndef CLIPBOARD_BRIDGE_H
#define CLIPBOARD_BRIDGE_H

#include <QObject>

// 供 WebEngineView 内 HTML 通过 WebChannel 调用，实现剪贴板复制
class ClipboardBridge : public QObject {
    Q_OBJECT
public:
    explicit ClipboardBridge(QObject *parent = nullptr);
    Q_INVOKABLE void copyToClipboard(const QString &text);
};

#endif // CLIPBOARD_BRIDGE_H
