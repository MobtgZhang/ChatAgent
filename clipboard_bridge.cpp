#include "clipboard_bridge.h"
#include <QGuiApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>

ClipboardBridge::ClipboardBridge(QObject *parent) : QObject(parent) {}

void ClipboardBridge::copyToClipboard(const QString &text) {
    if (text.isEmpty()) return;
    QClipboard *cb = QGuiApplication::clipboard();
    if (cb) cb->setText(text);
}

void ClipboardBridge::openUrl(const QString &url) {
    if (url.isEmpty()) return;
    QDesktopServices::openUrl(QUrl(url));
}
