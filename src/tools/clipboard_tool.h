#ifndef CLIPBOARD_TOOL_H
#define CLIPBOARD_TOOL_H

#include "base_tool.h"

/**
 * 剪贴板工具
 * 读取或写入系统剪贴板
 */
class ClipboardTool : public BaseTool {
    Q_OBJECT

public:
    explicit ClipboardTool(QObject *parent = nullptr);

    QString name() const override { return QStringLiteral("clipboard"); }
    QString description() const override;
    QVariantMap parametersSchema() const override;
    QString execute(const QVariantMap &args) override;

private:
    QString readClipboard();
    QString writeClipboard(const QString &content);
};

#endif // CLIPBOARD_TOOL_H
