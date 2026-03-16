#ifndef SHELL_TOOL_H
#define SHELL_TOOL_H

#include "base_tool.h"

/**
 * Shell 命令执行工具（安全沙箱）
 * 默认仅允许白名单命令，可配置
 * 支持跨平台：Windows (cmd.exe + PowerShell)、Linux、macOS
 */
class ShellTool : public BaseTool {
    Q_OBJECT

public:
    explicit ShellTool(QObject *parent = nullptr);

    QString name() const override { return QStringLiteral("shell"); }
    QString description() const override;
    QVariantMap parametersSchema() const override;
    QString execute(const QVariantMap &args) override;

    // Shell 命令白名单
    void setAllowedPrefixes(const QStringList &prefixes);
    QStringList allowedPrefixes() const;

    // PowerShell 命令白名单（仅 Windows 有效）
    void setPowerShellAllowedPrefixes(const QStringList &prefixes);
    QStringList powershellAllowedPrefixes() const;

private:
    bool isAllowed(const QString &cmd) const;
    QString runCommand(const QString &cmd);
    QStringList buildAllowedPrefixes();

    // 跨平台共有命令
    QStringList m_commonPrefixes;
    // Windows 独有命令
    QStringList m_windowsPrefixes;
    // Linux/macOS 独有命令
    QStringList m_linuxPrefixes;
    // 最终白名单（根据操作系统组合）
    QStringList m_allowedPrefixes;
    // PowerShell 命令白名单
    QStringList m_powershellAllowedPrefixes;
};

#endif // SHELL_TOOL_H
