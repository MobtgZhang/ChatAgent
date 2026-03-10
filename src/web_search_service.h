#ifndef WEB_SEARCH_SERVICE_H
#define WEB_SEARCH_SERVICE_H

#include <QString>
#include <QVector>
#include <QPair>
#include <QVariant>
#include <QNetworkAccessManager>

class Settings;

/** 单条搜索结果：标题、URL、抓取的正文摘要（可为空） */
struct SearchResult {
    QString title;
    QString url;
    QString snippet;  // 抓取的网页正文摘要，用于 LLM 与展示
};

/**
 * 共享联网搜索服务
 * 支持：DuckDuckGo、Baidu、Bing Search、Brave Search、Google、Tencent Search
 * DuckDuckGo/Google 不可达时自动回退到百度（国内场景）
 * 供 WebSearchTool 和 MainView RAG 统一使用
 */
class WebSearchService {
public:
    /**
     * 执行搜索，返回 (标题, URL) 列表（兼容旧接口）
     */
    static QVector<QPair<QString, QString>> search(
        const QString &query,
        Settings *settings,
        QNetworkAccessManager *nam = nullptr);

    /**
     * 线程安全重载：直接传入引擎和代理模式/URL
     * apiKey: Bing/Brave/Google(Serper) 需要
     * tencentSecretId/Key: Tencent 需要
     */
    static QVector<QPair<QString, QString>> search(
        const QString &query,
        const QString &engine,
        const QString &proxyMode,
        const QString &proxyUrl,
        QNetworkAccessManager *nam = nullptr,
        const QString &apiKey = QString(),
        const QString &tencentSecretId = QString(),
        const QString &tencentSecretKey = QString());

    /**
     * 联网 RAG 专用：搜索并抓取网页正文
     * 参考 MindSearch：conversationContext 可选，传入对话历史以提升多轮问答的搜索质量
     */
    static QVector<SearchResult> searchWithContent(
        const QString &query,
        const QString &engine,
        const QString &proxyMode,
        const QString &proxyUrl,
        QNetworkAccessManager *nam = nullptr,
        int maxFetch = 5,
        const QString &apiKey = QString(),
        const QString &tencentSecretId = QString(),
        const QString &tencentSecretKey = QString(),
        const QString &conversationContext = QString());

    /**
     * 根据对话上下文与当前问题，构建带上下文的搜索 query（MindSearch 风格）
     */
    static QString buildContextualSearchQuery(
        const QString &conversationContext,
        const QString &currentQuery);

    /**
     * 从消息列表构建对话上下文字符串（主问题 + 历史问答），供 buildContextualSearchQuery 使用
     * excludeLast 若为 true，则排除最后一条消息（视为当前问题）
     */
    static QString buildConversationContextFromMessages(
        const QVariantList &messages,
        bool excludeLast = true);

    /**
     * 抓取单个 URL 的正文，提取纯文本（供 RAG 使用）
     * proxyMode: off | system | manual
     */
    static QString fetchPageContent(
        const QString &url,
        const QString &proxyMode,
        const QString &proxyUrl,
        QNetworkAccessManager *nam = nullptr,
        int maxChars = 2000);

    /**
     * CLI 调试：代理检测 + 请求测试 + 搜索，返回多行调试报告
     */
    static QString runSearchDebug(
        const QString &query = QStringLiteral("test"),
        const QString &proxyMode = QStringLiteral("system"),
        const QString &proxyUrl = QString());
};

#endif // WEB_SEARCH_SERVICE_H
