#ifndef WEBSEARCH_TOOL_H
#define WEBSEARCH_TOOL_H

#include "base_tool.h"
#include <QNetworkAccessManager>

class Settings;

/**
 * 网络搜索工具（支持百度、Google、Bing、DuckDuckGo）
 * 用于 Agent 获取实时信息
 */
class WebSearchTool : public BaseTool {
    Q_OBJECT

public:
    explicit WebSearchTool(Settings *settings, QObject *parent = nullptr);

    QString name() const override { return QStringLiteral("web_search"); }
    QString description() const override;
    QVariantMap parametersSchema() const override;
    QString execute(const QVariantMap &args) override;

private:
    QString m_pendingResult;
    QNetworkAccessManager *m_nam;
    Settings *m_settings = nullptr;
};

#endif // WEBSEARCH_TOOL_H
