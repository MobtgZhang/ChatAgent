#ifndef PYTHON_TOOL_H
#define PYTHON_TOOL_H

#include "base_tool.h"

class Settings;

/**
 * Python 执行工具
 * 支持创建独立虚拟环境、pip 安装依赖、执行代码片段和脚本文件。
 * 无需外部依赖——仅需系统已安装 python3。
 *
 * action:
 *   run_code    — 执行 Python 代码字符串（自动写入临时文件后运行）
 *   run_script  — 执行指定路径的 .py 脚本
 *   pip_install — 安装一个或多个 PyPI 包
 *   pip_list    — 列出已安装包
 *   create_venv — 显式创建/重置虚拟环境
 */
class PythonTool : public BaseTool {
    Q_OBJECT

public:
    explicit PythonTool(Settings *settings, QObject *parent = nullptr);
    explicit PythonTool(QObject *parent = nullptr);

    QString name()             const override { return QStringLiteral("python"); }
    QString description()      const override;
    QVariantMap parametersSchema() const override;
    QString execute(const QVariantMap &args) override;

private:
    Settings *m_settings = nullptr;

    // 虚拟环境根目录（数据目录/agent_venv）
    QString venvRoot() const;

    // venv 内 python / pip 可执行路径
    QString venvPython() const;
    QString venvPip()    const;

    // 系统 python3 路径（用于创建 venv）
    QString systemPython() const;

    // 确保 venv 存在，首次调用时自动创建
    bool ensureVenv(QString &errOut);

    // 具体动作
    QString doRunCode   (const QString &code,   int timeoutSec);
    QString doRunScript (const QString &path,   int timeoutSec);
    QString doPipInstall(const QStringList &pkgs, int timeoutSec);
    QString doPipList   ();
    QString doCreateVenv();

    // 通用：启动进程并等待，返回 JSON 结果
    QString runProcess(const QString &program,
                       const QStringList &args,
                       int timeoutMs,
                       const QString &workDir = QString());
};

#endif // PYTHON_TOOL_H
