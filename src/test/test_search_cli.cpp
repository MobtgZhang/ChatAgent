/**
 * DuckDuckGo / 网络搜索调试 CLI
 * 用法: ./test_search_cli [query] [--proxy=off|system|manual] [--proxy-url=URL]
 * 示例: ./test_search_cli "hello"
 *       ./test_search_cli "hello" --proxy=manual --proxy-url=http://127.0.0.1:7890
 */
#include "web_search_service.h"
#include <QCoreApplication>
#include <QStringList>
#include <iostream>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    QStringList args = app.arguments();
    QString query     = QStringLiteral("test");
    QString proxyMode = QStringLiteral("system");
    QString proxyUrl;

    for (int i = 1; i < args.size(); ++i) {
        QString a = args[i];
        if (a == QLatin1String("-h") || a == QLatin1String("--help")) {
            std::cout << "用法: " << qPrintable(args[0]) << " [搜索词] [选项]\n"
                      << "选项:\n"
                      << "  --proxy=off|system|manual  代理模式 (默认 system)\n"
                      << "  --proxy-url=URL            手动代理时填写，如 http://127.0.0.1:7890\n"
                      << "  -h, --help                 显示此帮助\n"
                      << "示例: " << qPrintable(args[0]) << " hello\n"
                      << "      " << qPrintable(args[0])
                      << " hello --proxy=manual --proxy-url=http://127.0.0.1:7890\n";
            return 0;
        }
        if (a.startsWith(QLatin1String("--proxy="))) {
            proxyMode = a.mid(8).trimmed().toLower();
            if (proxyMode != QLatin1String("off") && proxyMode != QLatin1String("system")
                    && proxyMode != QLatin1String("manual"))
                proxyMode = QStringLiteral("system");
        } else if (a.startsWith(QLatin1String("--proxy-url="))) {
            proxyUrl = a.mid(12).trimmed();
        } else if (!a.startsWith(QLatin1Char('-'))) {
            query = a;
        }
    }

    std::cout << "搜索词: " << qPrintable(query) << "\n"
              << "代理模式: " << qPrintable(proxyMode)
              << (proxyUrl.isEmpty() ? "" : ("  URL: " + proxyMode.toStdString()))
              << "\n运行调试，请稍候...\n"
              << std::endl;

    QString report = WebSearchService::runSearchDebug(query, proxyMode, proxyUrl);
    std::cout << report.toUtf8().constData() << std::endl;
    return 0;
}
