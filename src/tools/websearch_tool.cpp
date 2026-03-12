#include "websearch_tool.h"
#include "settings.h"
#include "web_search_service.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

WebSearchTool::WebSearchTool(Settings *settings, QObject *parent) : BaseTool(parent), m_settings(settings) {
    m_nam = new QNetworkAccessManager(this);
}

QString WebSearchTool::description() const {
    return QStringLiteral("在网络上搜索信息。输入搜索关键词，返回相关网页摘要和链接。会抓取网页正文供 LLM 综合回答。");
}

QVariantMap WebSearchTool::parametersSchema() const {
    return QVariantMap{
        { "type", "object" },
        { "properties", QVariantMap{
            { "query", QVariantMap{
                { "type", "string" },
                { "description", "简短的搜索关键词（1-10个词，不要包含对话背景或冗长描述，如：\"Python 安装教程\"）" }
            }}
        }},
        { "required", QVariantList{"query"} }
    };
}

QString WebSearchTool::execute(const QVariantMap &args) {
    QString query = args.value("query").toString().trimmed();
    if (query.isEmpty()) {
        QString r = QStringLiteral("{\"error\":\"搜索关键词为空\",\"success\":false}");
        emit executionFinished(name(), r);
        return r;
    }

    QString engine = m_settings ? m_settings->searchEngine().trimmed().toLower() : QStringLiteral("duckduckgo");
    if (engine != QLatin1String("duckduckgo") && engine != QLatin1String("bing")
        && engine != QLatin1String("brave") && engine != QLatin1String("google")
        && engine != QLatin1String("tencent")) {
        engine = QStringLiteral("duckduckgo");
    }
    QString proxyMode = m_settings ? m_settings->proxyMode().trimmed().toLower() : QStringLiteral("system");
    QString proxyUrl = m_settings ? m_settings->proxyUrl().trimmed() : QString();
    if (proxyMode != QLatin1String("system") && proxyMode != QLatin1String("manual"))
        proxyMode = QStringLiteral("off");
    QString apiKey = m_settings ? m_settings->webSearchApiKey().trimmed() : QString();
    QString tcId = m_settings ? m_settings->tencentSecretId().trimmed() : QString();
    QString tcKey = m_settings ? m_settings->tencentSecretKey().trimmed() : QString();

    // 搜索 + 抓取网页正文，供 LLM 综合回答；限制 3 条结果防止上下文过大
    QVector<SearchResult> results = WebSearchService::searchWithContent(
        query, engine, proxyMode, proxyUrl, m_nam, 3, apiKey, tcId, tcKey, QString());

    QJsonObject result;
    if (engine == QLatin1String("duckduckgo")) {
        result["searchUrl"] = QStringLiteral("https://html.duckduckgo.com/html/?q=%1").arg(QString::fromUtf8(QUrl::toPercentEncoding(query)));
    } else if (engine == QLatin1String("google")) {
        result["searchUrl"] = QStringLiteral("https://www.google.com/search?q=%1").arg(QString::fromUtf8(QUrl::toPercentEncoding(query)));
    } else if (engine == QLatin1String("bing")) {
        result["searchUrl"] = QStringLiteral("https://www.bing.com/search?q=%1").arg(QString::fromUtf8(QUrl::toPercentEncoding(query)));
    }
    result["success"] = true;

    QJsonArray related;
    QStringList contextParts;
    for (int i = 0; i < results.size(); ++i) {
        const SearchResult &r = results.at(i);
        QJsonObject item;
        item["title"] = r.title;
        item["url"] = r.url;
        item["snippet"] = r.snippet;
        related.append(item);
        QString block = QStringLiteral("[%1] %2\n%3")
            .arg(i + 1).arg(r.title).arg(r.snippet.isEmpty() ? r.title : r.snippet);
        contextParts << block;
    }
    result["related"] = related;
    // 限制总 context 大小，防止 LLM 请求体过大导致 API 返回 500
    QString ctx = contextParts.join(QStringLiteral("\n\n"));
    if (ctx.length() > 2500)
        ctx = ctx.left(2500) + QStringLiteral("…");
    result["context"] = ctx;
    if (results.isEmpty()) {
        result["abstract"] = result.contains("searchUrl")
            ? QStringLiteral("未解析到结果，请访问上方 searchUrl 手动搜索。")
            : QStringLiteral("未获取到搜索结果。");
    } else {
        QStringList titles;
        for (const SearchResult &r : results)
            titles << r.title;
        result["abstract"] = titles.join(QStringLiteral(" | "));
    }

    m_pendingResult = QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact));
    emit executionFinished(name(), m_pendingResult);
    return m_pendingResult;
}
