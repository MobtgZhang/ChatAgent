#ifndef KEYBOARD_TOOL_H
#define KEYBOARD_TOOL_H

#include "base_tool.h"

/**
 * 键盘输入工具
 * 模拟键盘输入文本、按键、组合键等（用于 UI 自动化）
 * Linux: 通过 xdotool 实现
 */
class KeyboardTool : public BaseTool {
    Q_OBJECT

public:
    explicit KeyboardTool(QObject *parent = nullptr);

    QString name() const override { return QStringLiteral("keyboard"); }
    QString description() const override;
    QVariantMap parametersSchema() const override;
    QString execute(const QVariantMap &args) override;

private:
    QString typeText(const QString &text);
    QString keyPress(const QString &key);
    QString keyCombo(const QString &keys);
};

#endif // KEYBOARD_TOOL_H
