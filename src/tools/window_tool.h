#ifndef WINDOW_TOOL_H
#define WINDOW_TOOL_H

#include "base_tool.h"

/**
 * 窗口管理工具
 * 列出窗口、激活/切换窗口、获取窗口信息等
 * Linux: 通过 wmctrl 和 xdotool 实现
 */
class WindowTool : public BaseTool {
    Q_OBJECT

public:
    explicit WindowTool(QObject *parent = nullptr);

    QString name() const override { return QStringLiteral("window"); }
    QString description() const override;
    QVariantMap parametersSchema() const override;
    QString execute(const QVariantMap &args) override;

private:
    QString listWindows();
    QString activateWindow(const QString &pattern);
    QString getActiveWindow();
};

#endif // WINDOW_TOOL_H
