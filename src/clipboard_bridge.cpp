#include "clipboard_bridge.h"
#include <QGuiApplication>
#include <QClipboard>

ClipboardBridge::ClipboardBridge(QObject *parent) : QObject(parent) {}

void ClipboardBridge::copyToClipboard(const QString &text) {
    if (text.isEmpty()) return;
    QClipboard *cb = QGuiApplication::clipboard();
    if (cb) cb->setText(text);
}
