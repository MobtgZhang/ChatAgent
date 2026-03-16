#ifndef WAIT_TOOL_H
#define WAIT_TOOL_H

#include "base_tool.h"

class QTimer;
class QEventLoop;

/**
 * 等待/延迟工具
 * 阻塞等待指定秒数，用于 UI 操作间等待界面加载
 */
class WaitTool : public BaseTool {
    Q_OBJECT

public:
    explicit WaitTool(QObject *parent = nullptr);

    QString name() const override { return QStringLiteral("wait"); }
    QString description() const override;
    QVariantMap parametersSchema() const override;
    QString execute(const QVariantMap &args) override;
};

#endif // WAIT_TOOL_H
