#include "web_search_service.h"
#include "settings.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QUrl>
#include <QUrlQuery>
#include <QEventLoop>
#include <QRegularExpression>
#include <QSet>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QScopedPointer>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QDateTime>
#include <QTimeZone>
#include <QProcessEnvironment>
#include <QProcess>
#include <QTimer>
#include <QFile>
#include <QStandardPaths>
#include <QDir>

static void applyProxyFromUrl(QNetworkAccessManager *nam, const QString &proxyUrlStr) {
    if (!nam || proxyUrlStr.trimmed().isEmpty()) {
        if (nam) nam->setProxy(QNetworkProxy::NoProxy);
        return;
    }
    QString proxyStr = proxyUrlStr.trimmed();
    QUrl proxyUrl(proxyStr);
    if (!proxyUrl.isValid() || proxyUrl.host().isEmpty()) {
        nam->setProxy(QNetworkProxy::NoProxy);
        return;
    }
    QNetworkProxy proxy;
    QString scheme = proxyUrl.scheme().toLower();
    if (scheme == QLatin1String("socks5")) {
        proxy.setType(QNetworkProxy::Socks5Proxy);
    } else {
        proxy.setType(QNetworkProxy::HttpProxy);
    }
    proxy.setHostName(proxyUrl.host());
    proxy.setPort(proxyUrl.port(scheme == QLatin1String("socks5") ? 1080 : 8080));
    if (!proxyUrl.userName().isEmpty())
        proxy.setUser(proxyUrl.userName());
    if (!proxyUrl.password().isEmpty())
        proxy.setPassword(proxyUrl.password());
    nam->setProxy(proxy);
}

/** 从环境变量获取代理 URL（Linux 常见：VPN 或终端 export 的 http_proxy/https_proxy） */
static QString proxyUrlFromEnvironment() {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QStringList vars = { QStringLiteral("https_proxy"), QStringLiteral("HTTPS_PROXY"),
                        QStringLiteral("http_proxy"),  QStringLiteral("HTTP_PROXY"),
                        QStringLiteral("all_proxy"),   QStringLiteral("ALL_PROXY") };
    for (const QString &key : vars) {
        QString val = env.value(key).trimmed();
        if (!val.isEmpty()) return val;
    }
    return QString();
}

/** 从 GNOME gsettings 读取系统代理（Qt 无 libproxy 时 Linux 上 systemProxyForQuery 无效，需手动读取） */
static QString proxyUrlFromGnomeGsettings() {
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(QStringLiteral("gsettings"), { QStringLiteral("get"), QStringLiteral("org.gnome.system.proxy"), QStringLiteral("mode") });
    if (!proc.waitForFinished(2000) || proc.exitCode() != 0)
        return QString();
    QString mode = QString::fromUtf8(proc.readAllStandardOutput()).trimmed().remove(QLatin1Char('\''));
    if (mode != QLatin1String("manual"))
        return QString();

    auto gget = [](const QString &schema, const QString &key) -> QString {
        QProcess p;
        p.setProcessChannelMode(QProcess::MergedChannels);
        p.start(QStringLiteral("gsettings"), { QStringLiteral("get"), schema, key });
        if (!p.waitForFinished(1500) || p.exitCode() != 0) return QString();
        return QString::fromUtf8(p.readAllStandardOutput()).trimmed().remove(QLatin1Char('\''));
    };

    // 优先 HTTPS/HTTP，其次 SOCKS（Clash/V2Ray 等常用）
    QString host = gget(QStringLiteral("org.gnome.system.proxy.https"), QStringLiteral("host"));
    int port = gget(QStringLiteral("org.gnome.system.proxy.https"), QStringLiteral("port")).toInt();
    if (host.isEmpty() || port <= 0) {
        host = gget(QStringLiteral("org.gnome.system.proxy.http"), QStringLiteral("host"));
        port = gget(QStringLiteral("org.gnome.system.proxy.http"), QStringLiteral("port")).toInt();
    }
    if (host.isEmpty() || port <= 0) {
        host = gget(QStringLiteral("org.gnome.system.proxy.socks"), QStringLiteral("host"));
        port = gget(QStringLiteral("org.gnome.system.proxy.socks"), QStringLiteral("port")).toInt();
        if (!host.isEmpty() && port > 0)
            return QStringLiteral("socks5://%1:%2").arg(host).arg(port);
    }
    if (!host.isEmpty() && port > 0)
        return QStringLiteral("http://%1:%2").arg(host).arg(port);
    return QString();
}

/** 从 KDE kioslaverc 读取代理（Clash/V2Ray 在 KDE 下常用） */
static QString proxyUrlFromKdeKioslaverc() {
    QStringList configPaths = {
        QDir::homePath() + QStringLiteral("/.config/kioslaverc"),
        QDir::homePath() + QStringLiteral("/.kde/share/config/kioslaverc"),
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/kioslaverc")
    };
    QRegularExpression reSection(QLatin1String("^\\[([^\\]]+)\\]$"));
    QRegularExpression reKey(QLatin1String("^(ProxyType|httpProxy|httpsProxy|socksProxy)\\s*=\\s*(.+)$"));
    for (const QString &path : configPaths) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        bool inProxySection = false;
        int proxyType = 0;
        QString httpProxy, httpsProxy, socksProxy;
        while (!f.atEnd()) {
            QString s = QString::fromUtf8(f.readLine()).trimmed();
            auto mSec = reSection.match(s);
            if (mSec.hasMatch()) {
                inProxySection = (mSec.captured(1) == QLatin1String("Proxy Settings"));
                continue;
            }
            if (!inProxySection) continue;
            auto m = reKey.match(s);
            if (m.hasMatch()) {
                QString key = m.captured(1);
                QString val = m.captured(2).trimmed();
                if (key == QLatin1String("ProxyType"))
                    proxyType = val.toInt();
                else if (key == QLatin1String("httpProxy"))
                    httpProxy = val;
                else if (key == QLatin1String("httpsProxy"))
                    httpsProxy = val;
                else if (key == QLatin1String("socksProxy"))
                    socksProxy = val;
            }
        }
        f.close();
        if (proxyType != 1 && proxyType != 2)
            continue;
        for (const QString &raw : { httpsProxy, httpProxy, socksProxy }) {
            if (raw.isEmpty()) continue;
            if (raw.contains(QLatin1String("://")))
                return raw;
            QStringList parts = raw.split(QLatin1Char(' '));
            if (parts.size() >= 2) {
                QString host = parts[0].trimmed();
                int port = parts[1].trimmed().toInt();
                if (!host.isEmpty() && port > 0) {
                    if (!socksProxy.isEmpty() && raw == socksProxy)
                        return QStringLiteral("socks5://%1:%2").arg(host).arg(port);
                    return QStringLiteral("http://%1:%2").arg(host).arg(port);
                }
            }
        }
    }
    return QString();
}

/** 常见 VPN 端口回退（系统代理检测全失败时）：Clash 默认 7890 最常见 */
static QString proxyUrlFromCommonVpnPorts() {
    return QStringLiteral("http://127.0.0.1:7890");
}

/** 根据代理模式应用代理：off=不使用, system=系统代理, manual=手动 URL
 *  系统模式下：Qt 检测 -> 环境变量 -> GNOME gsettings -> KDE kioslaverc -> 常见 VPN 端口回退 */
static void applyProxyWithMode(QNetworkAccessManager *nam, const QString &proxyMode, const QString &proxyUrl) {
    if (!nam) return;
    QString mode = proxyMode.trimmed().toLower();
    if (mode == QLatin1String("off")) {
        nam->setProxy(QNetworkProxy::NoProxy);
        return;
    }
    if (mode == QLatin1String("system")) {
        QList<QNetworkProxy> proxies = QNetworkProxyFactory::systemProxyForQuery(QNetworkProxyQuery(QUrl("https://html.duckduckgo.com")));
        if (!proxies.isEmpty() && proxies.first().type() != QNetworkProxy::NoProxy) {
            nam->setProxy(proxies.first());
            return;
        }
        /* Linux 下 Qt 无 libproxy 时无法检测，回退到环境变量 */
        QString envProxy = proxyUrlFromEnvironment();
        if (!envProxy.isEmpty()) {
            applyProxyFromUrl(nam, envProxy);
            return;
        }
        /* 再回退到 GNOME gsettings（系统设置中的代理） */
        QString gsProxy = proxyUrlFromGnomeGsettings();
        if (!gsProxy.isEmpty()) {
            applyProxyFromUrl(nam, gsProxy);
            return;
        }
        /* KDE kioslaverc（Clash/V2Ray 在 KDE 下常用） */
        QString kdeProxy = proxyUrlFromKdeKioslaverc();
        if (!kdeProxy.isEmpty()) {
            applyProxyFromUrl(nam, kdeProxy);
            return;
        }
        /* 最后回退：Clash 默认端口 7890（VPN 开启但系统检测不到时的常见情况） */
        applyProxyFromUrl(nam, proxyUrlFromCommonVpnPorts());
        return;
    }
    if (mode == QLatin1String("manual")) {
        applyProxyFromUrl(nam, proxyUrl);
        return;
    }
    nam->setProxy(QNetworkProxy::NoProxy);
}

static void applyProxy(QNetworkAccessManager *nam, Settings *settings) {
    if (!settings)
        nam->setProxy(QNetworkProxy::NoProxy);
    else
        applyProxyWithMode(nam, settings->proxyMode(), settings->proxyUrl());
}

/** 网络请求超时（毫秒），避免无代理/墙内时一直卡在「搜索中」 */
static const int kNetworkTimeoutMs = 20000;

/** 带超时的 GET：超时或失败返回空 QByteArray */
static QByteArray getWithTimeout(QNetworkAccessManager *nam, const QNetworkRequest &req, int timeoutMs) {
    QNetworkReply *reply = nam->get(req);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QMetaObject::Connection connFinished = QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, [&]() { reply->abort(); loop.quit(); });
    timer.start(timeoutMs);
    loop.exec();
    timer.stop();
    QObject::disconnect(connFinished);
    QByteArray data = (reply->error() == QNetworkReply::NoError) ? reply->readAll() : QByteArray();
    reply->deleteLater();
    return data;
}

/** 带超时的 POST：超时或失败返回空 QByteArray */
static QByteArray postWithTimeout(QNetworkAccessManager *nam, const QNetworkRequest &req, const QByteArray &body, int timeoutMs) {
    QNetworkReply *reply = nam->post(req, body);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QMetaObject::Connection connFinished = QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, [&]() { reply->abort(); loop.quit(); });
    timer.start(timeoutMs);
    loop.exec();
    timer.stop();
    QObject::disconnect(connFinished);
    QByteArray data = (reply->error() == QNetworkReply::NoError) ? reply->readAll() : QByteArray();
    reply->deleteLater();
    return data;
}

/** 精简搜索 query：只保留核心关键词，限制长度，提升搜索引擎效果
 * 参考 MindSearch：每个搜索应为单一问题（非复合问题） */
static QString extractFocusedQuery(const QString &raw) {
    QString q = raw.trimmed();
    if (q.isEmpty()) return q;
    // 若较长且含句号，优先取最后一句（针对「前面背景。当前问题。」的场景）
    const QChar cjkPeriod(0x3002);  // 。
    if (q.length() > 50 && q.contains(cjkPeriod)) {
        int idx = q.lastIndexOf(cjkPeriod);
        if (idx >= 0 && idx < q.length() - 1)
            q = q.mid(idx + 1).trimmed();
    }
    // 去除开头常见填充词（MindSearch 风格：聚焦核心问题）
    static const char *prefixes[] = {
        "请", "帮我", "你能", "请问", "我想", "能不能", "可以", "能否",
        "麻烦", "请问一下", "帮我查", "帮我搜索", "帮我找", "告诉我", "解释一下"
    };
    for (const char *p : prefixes) {
        QString pref = QString::fromUtf8(p);
        if (q.startsWith(pref)) {
            q = q.mid(pref.length()).trimmed();
            break;
        }
    }
    // 限制长度：搜索引擎对过长 query 效果差
    const int maxLen = 80;
    if (q.length() > maxLen)
        q = q.left(maxLen).trimmed();
    return q.isEmpty() ? raw.trimmed().left(maxLen) : q;
}

/** 参考 MindSearch：将复合问题拆分为多个单一子问题，用于并行多查询搜索
 * 返回 1～3 个子查询，无关联的可并行搜索 */
static QStringList extractSubQueries(const QString &focused) {
    QString q = focused.trimmed();
    if (q.isEmpty()) return {};
    QStringList out;
    // 按分号、问号分割多问题（如 "A是什么？B有什么特点？"）
    QStringList parts = q.split(QRegularExpression(QLatin1String("[；;?？]")), Qt::SkipEmptyParts);
    for (QString &p : parts) {
        p = p.trimmed();
        if (p.length() >= 3) out.append(extractFocusedQuery(p));
    }
    if (out.isEmpty()) return {focused};
    if (out.size() > 3) out = out.mid(0, 3);  // 最多 3 个子查询
    return out;
}

static QVector<QPair<QString, QString>> parseBaiduHtml(const QByteArray &html) {
    QVector<QPair<QString, QString>> out;
    QString htmlStr = QString::fromUtf8(html);

    // 百度多种结构：h3 内链接、通用标题链接
    QList<QRegularExpression> patterns;
    patterns << QRegularExpression(QLatin1String("<h3[^>]*>\\s*<a[^>]+href=\"([^\"]+)\"[^>]*>([^<]+)</a>"))
             << QRegularExpression(QLatin1String("href=\"(https?://[^\"]+)\"[^>]*>([^<]{5,200})</a>"));

    QSet<QString> seenUrls;
    const int maxResults = 8;

    for (const QRegularExpression &re : patterns) {
        auto it = re.globalMatch(htmlStr);
        while (it.hasNext() && out.size() < maxResults) {
            auto m = it.next();
            QString url = m.captured(1).trimmed();
            QString title = m.captured(2).trimmed();
            title = title.remove(QRegularExpression(QLatin1String("<[^>]+>"))).trimmed();
            // 过滤站内、无效
            if (url.isEmpty() || title.isEmpty() || title.length() < 2) continue;
            if (url.contains(QLatin1String("/s?wd="))) continue;  // 站内搜索链
            if (url.startsWith(QLatin1String("javascript:")) || url.startsWith(QLatin1String("#")))
                continue;
            if (!url.startsWith(QLatin1String("http"))) continue;
            if (seenUrls.contains(url)) continue;
            seenUrls.insert(url);
            out.append({title, url});
        }
        if (out.size() >= maxResults) break;
    }
    return out;
}

static QVector<QPair<QString, QString>> parseGoogleHtml(const QByteArray &html) {
    QVector<QPair<QString, QString>> out;
    QString htmlStr = QString::fromUtf8(html);
    QRegularExpression re(QLatin1String("<h3[^>]*>\\s*<a[^>]+href=\"([^\"]+)\"[^>]*>([^<]+)</a>"));
    auto it = re.globalMatch(htmlStr);
    QSet<QString> seen;
    while (it.hasNext() && out.size() < 8) {
        auto m = it.next();
        QString url = m.captured(1).trimmed();
        QString title = m.captured(2).trimmed();
        title = title.remove(QRegularExpression(QLatin1String("<[^>]+>"))).trimmed();
        if (url.startsWith(QLatin1String("/search")) || url.startsWith(QLatin1String("javascript:"))) continue;
        if (!url.startsWith(QLatin1String("http"))) continue;
        if (title.isEmpty() || seen.contains(url)) continue;
        seen.insert(url);
        out.append({title, url});
    }
    return out;
}

static QVector<QPair<QString, QString>> parseBingHtml(const QByteArray &html) {
    QVector<QPair<QString, QString>> out;
    QString htmlStr = QString::fromUtf8(html);
    // Bing: <h2><a href="...">title</a></h2>
    QRegularExpression re(QLatin1String("<h2[^>]*>\\s*<a[^>]+href=\"([^\"]+)\"[^>]*>([^<]+)</a>"));
    auto it = re.globalMatch(htmlStr);
    QSet<QString> seen;
    while (it.hasNext() && out.size() < 8) {
        auto m = it.next();
        QString url = m.captured(1).trimmed();
        QString title = m.captured(2).trimmed();
        title = title.remove(QRegularExpression(QLatin1String("<[^>]+>"))).trimmed();
        if (!url.startsWith(QLatin1String("http")) || title.isEmpty() || seen.contains(url)) continue;
        seen.insert(url);
        out.append({title, url});
    }
    return out;
}

/** 从 DuckDuckGo 重定向 URL 中解析真实目标地址 (uddg 参数) */
static QString decodeDuckDuckGoUrl(const QString &redirectUrl) {
    QString qstr = redirectUrl;
    qstr.replace(QLatin1String("&amp;"), QLatin1String("&"));
    QUrl u(qstr);
    QUrlQuery q(u.query());
    QString uddg = q.queryItemValue(QStringLiteral("uddg"));
    if (uddg.isEmpty()) return redirectUrl;
    return QUrl::fromPercentEncoding(uddg.toUtf8());
}

/** DuckDuckGo HTML 搜索：使用 html.duckduckgo.com 获取真实网页结果
 * 实际返回为标准 HTML，结果链接为 <a class="result__a" href="...">；兼容旧版 Markdown 格式。 */
static QVector<QPair<QString, QString>> parseDuckDuckGoHtml(const QByteArray &html) {
    QVector<QPair<QString, QString>> out;
    QString htmlStr = QString::fromUtf8(html);
    QSet<QString> seenUrls;
    const int maxResults = 10;

    // 1) 优先解析真实 HTML：result__a 链接，href 可为直接 URL 或 /l/?uddg=... 跳转
    QRegularExpression htmlLinkRe(QLatin1String(
        "<a[^>]*class=\"[^\"]*result__a[^\"]*\"[^>]*href=\"([^\"]+)\"[^>]*>([^<]*)</a>"));
    auto htmlIt = htmlLinkRe.globalMatch(htmlStr);
    while (htmlIt.hasNext() && out.size() < maxResults) {
        auto m = htmlIt.next();
        QString href = m.captured(1).trimmed();
        QString title = m.captured(2).trimmed();
        title = title.remove(QRegularExpression(QLatin1String("<[^>]+>"))).trimmed();
        if (title.isEmpty() || title.length() < 2) continue;
        QString realUrl;
        if (href.contains(QLatin1String("uddg="))) {
            QString fullHref = href.startsWith(QLatin1String("http")) ? href : QStringLiteral("https://duckduckgo.com") + (href.startsWith(QLatin1Char('/')) ? href : QLatin1String("/") + href);
            realUrl = decodeDuckDuckGoUrl(fullHref);
        } else if (href.startsWith(QLatin1String("http://")) || href.startsWith(QLatin1String("https://"))) {
            realUrl = href;  // DuckDuckGo 现已直接返回目标 URL
        } else
            continue;
        if (realUrl.isEmpty() || !realUrl.startsWith(QLatin1String("http"))) continue;
        if (seenUrls.contains(realUrl)) continue;
        seenUrls.insert(realUrl);
        if (title.length() > 200) title = title.left(200) + QStringLiteral("…");
        out.append({title, realUrl});
    }

    // 2) 备用：任意 <a href="...duckduckgo.com/l/?...uddg=..."> 或 href="/l/?uddg=..."
    if (out.size() < 3) {
        QRegularExpression anyDdgRe(QLatin1String(
            "<a[^>]+href=\"(https://duckduckgo\\.com/l/\\?[^\"]+|/l/\\?[^\"]+)\"[^>]*>([^<]*)</a>"));
        auto anyIt = anyDdgRe.globalMatch(htmlStr);
        while (anyIt.hasNext() && out.size() < maxResults) {
            auto m = anyIt.next();
            QString href = m.captured(1).trimmed();
            QString title = m.captured(2).trimmed();
            title = title.remove(QRegularExpression(QLatin1String("<[^>]+>"))).trimmed();
            if (title.length() < 2 || !href.contains(QLatin1String("uddg="))) continue;
            QString fullHref = href.startsWith(QLatin1String("http")) ? href : QStringLiteral("https://duckduckgo.com") + (href.startsWith(QLatin1Char('/')) ? href : QLatin1String("/") + href);
            QString realUrl = decodeDuckDuckGoUrl(fullHref);
            if (realUrl.isEmpty() || !realUrl.startsWith(QLatin1String("http")) || seenUrls.contains(realUrl)) continue;
            seenUrls.insert(realUrl);
            if (title.length() > 200) title = title.left(200) + QStringLiteral("…");
            out.append({title, realUrl});
        }
    }

    // 3) 兼容旧版 Markdown 结构：## Title \n [text](https://duckduckgo.com/l/?...)
    if (out.size() < 3) {
        QRegularExpression linkRe(QLatin1String(
            "\\[([^\\]]+)\\]\\(https://duckduckgo\\.com/l/\\?([^)]+)\\)"));
        QRegularExpression sectionRe(QLatin1String("##\\s+([^\n]+)"));
        QStringList sections = htmlStr.split(QLatin1String("## "));
        for (int i = 1; i < sections.size() && out.size() < maxResults; ++i) {
            const QString &block = sections.at(i);
            QString title = sectionRe.match(block).captured(1).trimmed();
            if (title.isEmpty()) title = block.left(block.indexOf(QLatin1Char('\n'))).trimmed();
            QString realUrl;
            QString linkText1, linkText2;
            int linkIdx = 0;
            auto linkIt = linkRe.globalMatch(block);
            while (linkIt.hasNext() && linkIdx < 2) {
                auto m = linkIt.next();
                QString fullUrl = QStringLiteral("https://duckduckgo.com/l/?") + m.captured(2);
                realUrl = decodeDuckDuckGoUrl(fullUrl);
                if (realUrl.isEmpty() || !realUrl.startsWith(QLatin1String("http"))) continue;
                if (linkIdx == 0) linkText1 = m.captured(1).trimmed(); else linkText2 = m.captured(1).trimmed();
                ++linkIdx;
            }
            if (realUrl.isEmpty() || seenUrls.contains(realUrl)) continue;
            seenUrls.insert(realUrl);
            if (title.isEmpty()) title = (linkText2.length() > linkText1.length()) ? linkText2 : linkText1;
            if (title.length() > 200) title = title.left(200) + QStringLiteral("…");
            out.append({title, realUrl});
        }
        auto fallbackIt = linkRe.globalMatch(htmlStr);
        while (fallbackIt.hasNext() && out.size() < maxResults) {
            auto m = fallbackIt.next();
            QString fullUrl = QStringLiteral("https://duckduckgo.com/l/?") + m.captured(2);
            QString realUrl = decodeDuckDuckGoUrl(fullUrl);
            if (realUrl.isEmpty() || !realUrl.startsWith(QLatin1String("http")) || seenUrls.contains(realUrl)) continue;
            seenUrls.insert(realUrl);
            QString text = m.captured(1).trimmed();
            if (text.length() < 3) continue;
            out.append({text, realUrl});
        }
    }
    return out;
}

/** 百度搜索：国内可直接访问，无需代理 */
static QVector<QPair<QString, QString>> searchBaidu(
    const QString &query, QNetworkAccessManager *nam)
{
    QVector<QPair<QString, QString>> out;
    QUrl url(QStringLiteral("https://www.baidu.com/s"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("wd"), query);
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
        QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"));
    req.setRawHeader("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QByteArray data = getWithTimeout(nam, req, kNetworkTimeoutMs);
    if (data.isEmpty()) return out;
    return parseBaiduHtml(data);
}

static QVector<QPair<QString, QString>> searchDuckDuckGo(
    const QString &query, QNetworkAccessManager *nam)
{
    QVector<QPair<QString, QString>> out;
    QUrl url(QStringLiteral("https://html.duckduckgo.com/html/"));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
        QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"));
    req.setRawHeader("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    // 优先 POST（与浏览器表单一致），失败时再试 GET
    QByteArray body = QByteArray("q=") + QUrl::toPercentEncoding(query);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("application/x-www-form-urlencoded"));
    QByteArray data = postWithTimeout(nam, req, body, kNetworkTimeoutMs);
    if (data.isEmpty()) {
        QUrl getUrl(QStringLiteral("https://html.duckduckgo.com/html/"));
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("q"), query);
        getUrl.setQuery(q);
        QNetworkRequest getReq(getUrl);
        getReq.setHeader(QNetworkRequest::UserAgentHeader, req.header(QNetworkRequest::UserAgentHeader));
        getReq.setRawHeader("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
        getReq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        data = getWithTimeout(nam, getReq, kNetworkTimeoutMs);
    }
    if (data.isEmpty()) return out;
    return parseDuckDuckGoHtml(data);
}

/** Brave Search API：结构化 JSON，需 API Key；参考 MindSearch 的 BraveSearch */
static QVector<SearchResult> searchBraveApiWithSnippet(
    const QString &query, const QString &apiKey, QNetworkAccessManager *nam)
{
    QVector<SearchResult> out;
    if (apiKey.trimmed().isEmpty()) return out;

    QUrl url(QStringLiteral("https://api.search.brave.com/res/v1/web/search"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("q"), query);
    q.addQueryItem(QStringLiteral("count"), QStringLiteral("10"));
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
        QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"));
    req.setRawHeader("X-Subscription-Token", apiKey.trimmed().toUtf8());
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QByteArray data = getWithTimeout(nam, req, kNetworkTimeoutMs);
    if (data.isEmpty()) return out;

    QJsonObject root = QJsonDocument::fromJson(data).object();
    QJsonObject web = root["web"].toObject();
    QJsonArray results = web["results"].toArray();
    for (const QJsonValue &v : results) {
        QJsonObject r = v.toObject();
        SearchResult sr;
        sr.title = r["title"].toString().trimmed();
        sr.url = r["url"].toString().trimmed();
        sr.snippet = r["description"].toString().trimmed();
        if (sr.snippet.length() > 2000) sr.snippet = sr.snippet.left(2000) + QStringLiteral("…");
        if (!sr.title.isEmpty() && !sr.url.isEmpty())
            out.append(sr);
    }
    return out;
}

/** Bing Web Search API v7：需 API Key，返回 JSON；无 Key 时用 HTML 抓取
 * 返回 SearchResult 以便直接使用 API 返回的 snippet */
static QVector<SearchResult> searchBingApiWithSnippet(
    const QString &query, const QString &apiKey, QNetworkAccessManager *nam)
{
    QVector<SearchResult> out;
    if (apiKey.trimmed().isEmpty()) return out;

    QUrl url(QStringLiteral("https://api.bing.microsoft.com/v7.0/search"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("q"), query);
    q.addQueryItem(QStringLiteral("mkt"), QStringLiteral("zh-CN"));
    q.addQueryItem(QStringLiteral("count"), QStringLiteral("10"));
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
        QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"));
    req.setRawHeader("Ocp-Apim-Subscription-Key", apiKey.trimmed().toUtf8());
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QByteArray data = getWithTimeout(nam, req, kNetworkTimeoutMs);
    if (data.isEmpty()) return out;

    QJsonObject root = QJsonDocument::fromJson(data).object();
    QJsonObject webPages = root["webPages"].toObject();
    QJsonArray values = webPages["value"].toArray();
    for (const QJsonValue &v : values) {
        QJsonObject r = v.toObject();
        SearchResult sr;
        sr.title = r["name"].toString().trimmed();
        sr.url = r["url"].toString().trimmed();
        sr.snippet = r["snippet"].toString().trimmed();
        if (sr.snippet.length() > 2000) sr.snippet = sr.snippet.left(2000) + QStringLiteral("…");
        if (!sr.title.isEmpty() && !sr.url.isEmpty())
            out.append(sr);
    }
    return out;
}

/** 腾讯联网搜索 SearchPro：需 SecretId + SecretKey，TC3-HMAC-SHA256 签名 */
static QByteArray hmacSha256(const QByteArray &key, const QByteArray &data) {
    QMessageAuthenticationCode hmac(QCryptographicHash::Sha256, key);
    hmac.addData(data);
    return hmac.result();
}
static QString sha256Hex(const QByteArray &data) {
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()).toLower();
}
/** 将 SearchResult 转为 (title,url) 对 */
static QVector<QPair<QString, QString>> searchResultsToPairs(const QVector<SearchResult> &vec) {
    QVector<QPair<QString, QString>> out;
    for (const SearchResult &sr : vec)
        out.append({sr.title, sr.url});
    return out;
}

static QVector<SearchResult> searchTencentApiWithSnippet(
    const QString &query, const QString &secretId, const QString &secretKey,
    QNetworkAccessManager *nam)
{
    QVector<SearchResult> out;
    if (secretId.trimmed().isEmpty() || secretKey.trimmed().isEmpty()) return out;

    const QString host = QStringLiteral("wsa.tencentcloudapi.com");
    const QString service = QStringLiteral("wsa");
    const QString action = QStringLiteral("SearchPro");
    const QString version = QStringLiteral("2025-05-08");
    qint64 ts = QDateTime::currentSecsSinceEpoch();
    QString date = QDateTime::fromSecsSinceEpoch(ts, QTimeZone::utc()).toString(Qt::ISODate).left(10);  // YYYY-MM-DD

    QJsonObject payload;
    payload["Query"] = query;
    QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    // 1. CanonicalRequest（content-type 需与请求头一致）
    QString ct = QStringLiteral("application/json; charset=utf-8");
    QString canonicalHeaders = QStringLiteral("content-type:%1\nhost:%2\nx-tc-action:%3\n")
        .arg(ct).arg(host).arg(action.toLower());
    QString signedHeaders = QStringLiteral("content-type;host;x-tc-action");
    QString hashedPayload = sha256Hex(body);
    QString canonicalRequest = QStringLiteral("POST\n/\n\n%1\n%2\n%3")
        .arg(canonicalHeaders).arg(signedHeaders).arg(hashedPayload);

    // 2. StringToSign
    QString credentialScope = QStringLiteral("%1/%2/tc3_request").arg(date).arg(service);
    QString hashedCanonical = sha256Hex(canonicalRequest.toUtf8());
    QString stringToSign = QStringLiteral("TC3-HMAC-SHA256\n%1\n%2\n%3")
        .arg(ts).arg(credentialScope).arg(hashedCanonical);

    // 3. SigningKey
    QByteArray kSecret = QByteArray("TC3") + secretKey.trimmed().toUtf8();
    QByteArray kDate = hmacSha256(kSecret, date.toUtf8());
    QByteArray kService = hmacSha256(kDate, service.toUtf8());
    QByteArray kSigning = hmacSha256(kService, QByteArray("tc3_request"));

    // 4. Signature
    QByteArray sig = hmacSha256(kSigning, stringToSign.toUtf8());
    QString signature = QString::fromLatin1(sig.toHex()).toLower();

    // 5. Authorization
    QString auth = QStringLiteral("TC3-HMAC-SHA256 Credential=%1/%2, SignedHeaders=%3, Signature=%4")
        .arg(secretId.trimmed()).arg(credentialScope).arg(signedHeaders).arg(signature);

    QNetworkRequest req(QUrl(QStringLiteral("https://") + host));
    req.setHeader(QNetworkRequest::ContentTypeHeader, ct);
    req.setRawHeader("Host", host.toUtf8());
    req.setRawHeader("X-TC-Action", action.toUtf8());
    req.setRawHeader("X-TC-Version", version.toUtf8());
    req.setRawHeader("X-TC-Timestamp", QByteArray::number(ts));
    req.setRawHeader("Authorization", auth.toUtf8());
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QByteArray data = postWithTimeout(nam, req, body, kNetworkTimeoutMs);
    if (data.isEmpty()) return out;

    QJsonObject root = QJsonDocument::fromJson(data).object();
    QJsonObject resp = root["Response"].toObject();
    QJsonArray pages = resp["Pages"].toArray();
    for (const QJsonValue &v : pages) {
        QJsonObject p = QJsonDocument::fromJson(v.toString().toUtf8()).object();
        SearchResult sr;
        sr.title = p["title"].toString().trimmed();
        sr.url = p["url"].toString().trimmed();
        sr.snippet = p["passage"].toString().trimmed();
        if (sr.title.isEmpty()) sr.title = sr.snippet.left(100);
        if (sr.snippet.length() > 2000) sr.snippet = sr.snippet.left(2000) + QStringLiteral("…");
        if (!sr.url.isEmpty())
            out.append(sr);
    }
    return out;
}

/** Serper API：Google 搜索结果，需 API Key；返回 SearchResult 含 snippet */
static QVector<SearchResult> searchSerperApiWithSnippet(
    const QString &query, const QString &apiKey, QNetworkAccessManager *nam)
{
    QVector<SearchResult> out;
    if (apiKey.trimmed().isEmpty()) return out;

    QNetworkRequest req(QUrl(QStringLiteral("https://google.serper.dev/search")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("application/json"));
    req.setRawHeader("X-API-KEY", apiKey.trimmed().toUtf8());
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QJsonObject body;
    body["q"] = query;
    QByteArray bodyData = QJsonDocument(body).toJson(QJsonDocument::Compact);

    QByteArray data = postWithTimeout(nam, req, bodyData, kNetworkTimeoutMs);
    if (data.isEmpty()) return out;

    QJsonObject root = QJsonDocument::fromJson(data).object();
    QJsonArray organic = root["organic"].toArray();
    if (organic.isEmpty()) organic = root["organic_results"].toArray();
    for (const QJsonValue &v : organic) {
        QJsonObject r = v.toObject();
        SearchResult sr;
        sr.title = r["title"].toString().trimmed();
        sr.url = r["link"].toString().trimmed();
        sr.snippet = r["snippet"].toString().trimmed();
        if (sr.snippet.length() > 2000) sr.snippet = sr.snippet.left(2000) + QStringLiteral("…");
        if (!sr.title.isEmpty() && !sr.url.isEmpty())
            out.append(sr);
    }
    return out;
}

/** 兼容：Serper 返回 (title,url) 对 */
static QVector<QPair<QString, QString>> searchSerperApi(
    const QString &query, const QString &apiKey, QNetworkAccessManager *nam)
{
    return searchResultsToPairs(searchSerperApiWithSnippet(query, apiKey, nam));
}

/** HTML 抓取：Google、Bing、Baidu（无 API Key 时回退用） */
static QVector<QPair<QString, QString>> searchHtmlEngine(
    const QString &query, const QString &engine,
    QNetworkAccessManager *nam)
{
    if (engine == QLatin1String("baidu"))
        return searchBaidu(query, nam);

    QUrl url;
    if (engine == QLatin1String("google")) {
        url = QUrl(QStringLiteral("https://www.google.com/search"));
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("q"), query);
        url.setQuery(q);
    } else {  // bing
        url = QUrl(QStringLiteral("https://www.bing.com/search"));
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("q"), query);
        url.setQuery(q);
    }

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
        QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"));
    req.setRawHeader("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QByteArray html = getWithTimeout(nam, req, kNetworkTimeoutMs);
    QVector<QPair<QString, QString>> out;
    if (html.isEmpty()) return out;
    out = (engine == QLatin1String("bing")) ? parseBingHtml(html) : parseGoogleHtml(html);

    return out;
}

QVector<QPair<QString, QString>> WebSearchService::search(
    const QString &query,
    Settings *settings,
    QNetworkAccessManager *nam)
{
    QVector<QPair<QString, QString>> result;
    QString focused = extractFocusedQuery(query);
    if (focused.isEmpty()) return result;

    QNetworkAccessManager *useNam = nam;
    QScopedPointer<QNetworkAccessManager> tempNam;
    if (!useNam) {
        tempNam.reset(new QNetworkAccessManager());
        useNam = tempNam.data();
    }
    applyProxy(useNam, settings);

    QString engine = settings && !settings->searchEngine().trimmed().isEmpty()
        ? settings->searchEngine().trimmed().toLower()
        : QStringLiteral("duckduckgo");
    if (engine != QLatin1String("duckduckgo") && engine != QLatin1String("bing")
        && engine != QLatin1String("brave") && engine != QLatin1String("google")
        && engine != QLatin1String("tencent") && engine != QLatin1String("baidu")) {
        engine = QStringLiteral("duckduckgo");
    }

    QString apiKey = settings ? settings->webSearchApiKey().trimmed() : QString();
    QString tcId = settings ? settings->tencentSecretId().trimmed() : QString();
    QString tcKey = settings ? settings->tencentSecretKey().trimmed() : QString();

    if (engine == QLatin1String("baidu")) {
        result = searchBaidu(focused, useNam);
    } else if (engine == QLatin1String("duckduckgo")) {
        result = searchDuckDuckGo(focused, useNam);
        if (result.isEmpty()) result = searchBaidu(focused, useNam);  // 国内不可达时回退到百度
    } else if (engine == QLatin1String("bing")) {
        result = !apiKey.isEmpty() ? searchResultsToPairs(searchBingApiWithSnippet(focused, apiKey, useNam)) : searchHtmlEngine(focused, QLatin1String("bing"), useNam);
    } else if (engine == QLatin1String("brave") && !apiKey.isEmpty()) {
        result = searchResultsToPairs(searchBraveApiWithSnippet(focused, apiKey, useNam));
    } else if (engine == QLatin1String("brave")) {
        result = searchDuckDuckGo(focused, useNam);
    } else if (engine == QLatin1String("google")) {
        if (!apiKey.isEmpty()) {
            result = searchSerperApi(focused, apiKey, useNam);
        } else {
            result = searchHtmlEngine(focused, QLatin1String("google"), useNam);
            if (result.isEmpty()) result = searchBaidu(focused, useNam);  // 国内 Google 不可达时回退到百度
        }
    } else if (engine == QLatin1String("tencent") && !tcId.isEmpty() && !tcKey.isEmpty()) {
        result = searchResultsToPairs(searchTencentApiWithSnippet(focused, tcId, tcKey, useNam));
    } else if (engine == QLatin1String("tencent")) {
        result = searchDuckDuckGo(focused, useNam);
    } else {
        result = searchDuckDuckGo(focused, useNam);
    }
    return result;
}

QVector<QPair<QString, QString>> WebSearchService::search(
    const QString &query,
    const QString &engine,
    const QString &proxyMode,
    const QString &proxyUrl,
    QNetworkAccessManager *nam,
    const QString &apiKey,
    const QString &tencentSecretId,
    const QString &tencentSecretKey)
{
    QVector<QPair<QString, QString>> result;
    QString focused = extractFocusedQuery(query);
    if (focused.isEmpty()) return result;

    QNetworkAccessManager *useNam = nam;
    QScopedPointer<QNetworkAccessManager> tempNam;
    if (!useNam) {
        tempNam.reset(new QNetworkAccessManager());
        useNam = tempNam.data();
    }
    applyProxyWithMode(useNam, proxyMode, proxyUrl);

    QString e = engine.trimmed().toLower();
    if (e != QLatin1String("duckduckgo") && e != QLatin1String("bing")
        && e != QLatin1String("brave") && e != QLatin1String("google")
        && e != QLatin1String("tencent") && e != QLatin1String("baidu")) {
        e = QStringLiteral("duckduckgo");
    }

    QString key = apiKey.trimmed();
    QString tcId = tencentSecretId.trimmed();
    QString tcKey = tencentSecretKey.trimmed();

    if (e == QLatin1String("baidu")) {
        result = searchBaidu(focused, useNam);
    } else if (e == QLatin1String("duckduckgo")) {
        result = searchDuckDuckGo(focused, useNam);
        if (result.isEmpty()) result = searchBaidu(focused, useNam);
    } else if (e == QLatin1String("bing")) {
        result = !key.isEmpty() ? searchResultsToPairs(searchBingApiWithSnippet(focused, key, useNam)) : searchHtmlEngine(focused, QLatin1String("bing"), useNam);
    } else if (e == QLatin1String("brave") && !key.isEmpty()) {
        result = searchResultsToPairs(searchBraveApiWithSnippet(focused, key, useNam));
    } else if (e == QLatin1String("brave")) {
        result = searchDuckDuckGo(focused, useNam);
        if (result.isEmpty()) result = searchBaidu(focused, useNam);
    } else if (e == QLatin1String("google")) {
        if (!key.isEmpty()) {
            result = searchSerperApi(focused, key, useNam);
        } else {
            result = searchHtmlEngine(focused, QLatin1String("google"), useNam);
            if (result.isEmpty()) result = searchBaidu(focused, useNam);
        }
    } else if (e == QLatin1String("tencent") && !tcId.isEmpty() && !tcKey.isEmpty()) {
        result = searchResultsToPairs(searchTencentApiWithSnippet(focused, tcId, tcKey, useNam));
    } else if (e == QLatin1String("tencent")) {
        result = searchDuckDuckGo(focused, useNam);
    } else {
        result = searchDuckDuckGo(focused, useNam);
    }
    return result;
}

// ── 从 HTML 提取纯文本 ─────────────────────────────────────────────────────
static QString extractTextFromHtml(const QByteArray &html) {
    QString htmlStr;
    // 尝试 UTF-8，失败则用 Latin1
    if (html.contains("charset=\"utf-8\"") || html.contains("charset='utf-8'")
        || html.contains("charset=utf-8") || html.contains("charset=UTF-8"))
        htmlStr = QString::fromUtf8(html);
    else
        htmlStr = QString::fromUtf8(html);
    if (htmlStr.isEmpty()) htmlStr = QString::fromLatin1(html);

    // 移除 script、style
    htmlStr.remove(QRegularExpression(QLatin1String("<script[^>]*>[\\s\\S]*?</script>"), QRegularExpression::CaseInsensitiveOption));
    htmlStr.remove(QRegularExpression(QLatin1String("<style[^>]*>[\\s\\S]*?</style>"), QRegularExpression::CaseInsensitiveOption));
    htmlStr.remove(QRegularExpression(QLatin1String("<noscript[^>]*>[\\s\\S]*?</noscript>"), QRegularExpression::CaseInsensitiveOption));
    // 标签替换为空格
    htmlStr.replace(QRegularExpression(QLatin1String("<[^>]+>")), QChar(' '));
    // HTML 实体
    htmlStr.replace(QLatin1String("&nbsp;"), QChar(' '));
    htmlStr.replace(QLatin1String("&amp;"), QLatin1String("&"));
    htmlStr.replace(QLatin1String("&lt;"), QLatin1String("<"));
    htmlStr.replace(QLatin1String("&gt;"), QLatin1String(">"));
    htmlStr.replace(QLatin1String("&quot;"), QChar('"'));
    htmlStr.replace(QLatin1String("&apos;"), QChar('\''));
    // 合并空白
    htmlStr = htmlStr.simplified();
    htmlStr.replace(QRegularExpression(QLatin1String("\\s+")), QChar(' '));
    return htmlStr.trimmed();
}

QString WebSearchService::fetchPageContent(const QString &url,
                                          const QString &proxyMode,
                                          const QString &proxyUrl,
                                          QNetworkAccessManager *nam,
                                          int maxChars) {
    if (url.trimmed().isEmpty()) return QString();

    QNetworkAccessManager *useNam = nam;
    QScopedPointer<QNetworkAccessManager> tempNam;
    if (!useNam) {
        tempNam.reset(new QNetworkAccessManager());
        useNam = tempNam.data();
    }
    applyProxyWithMode(useNam, proxyMode, proxyUrl);

    QUrl qurl(url);
    if (!qurl.isValid() || !qurl.scheme().startsWith(QLatin1String("http")))
        return QString();

    QNetworkRequest req(qurl);
    req.setHeader(QNetworkRequest::UserAgentHeader,
        QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"));
    req.setRawHeader("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QByteArray data = getWithTimeout(useNam, req, kNetworkTimeoutMs);
    if (data.isEmpty()) return QString();

    QString text = extractTextFromHtml(data);
    if (text.length() > maxChars)
        text = text.left(maxChars) + QStringLiteral("…");
    return text;
}

/** 对单个子查询执行搜索（供多查询合并使用） */
static QVector<SearchResult> searchOneQuery(
    const QString &subQuery,
    const QString &e,
    const QString &proxyMode,
    const QString &proxyUrl,
    QNetworkAccessManager *useNam,
    const QString &key,
    const QString &tcId,
    const QString &tcKey,
    int perQueryLimit)
{
    QVector<SearchResult> out;
    if (subQuery.trimmed().isEmpty()) return out;
    bool hasApiSnippet = false;
    if (e == QLatin1String("bing") && !key.isEmpty()) {
        out = searchBingApiWithSnippet(subQuery, key, useNam);
        hasApiSnippet = true;
    } else if (e == QLatin1String("brave") && !key.isEmpty()) {
        out = searchBraveApiWithSnippet(subQuery, key, useNam);
        hasApiSnippet = true;
    } else if (e == QLatin1String("google") && !key.isEmpty()) {
        out = searchSerperApiWithSnippet(subQuery, key, useNam);
        hasApiSnippet = true;
    } else if (e == QLatin1String("tencent") && !tcId.isEmpty() && !tcKey.isEmpty()) {
        out = searchTencentApiWithSnippet(subQuery, tcId, tcKey, useNam);
        hasApiSnippet = true;
    } else {
        QVector<QPair<QString, QString>> raw;
        if (e == QLatin1String("baidu")) {
            raw = searchBaidu(subQuery, useNam);
        } else if (e == QLatin1String("duckduckgo")) {
            raw = searchDuckDuckGo(subQuery, useNam);
            if (raw.isEmpty()) raw = searchBaidu(subQuery, useNam);
        } else {
            QString htmlEngine = (e == QLatin1String("bing")) ? QLatin1String("bing") : QLatin1String("google");
            raw = searchHtmlEngine(subQuery, htmlEngine, useNam);
            if (raw.isEmpty() && htmlEngine == QLatin1String("google")) raw = searchBaidu(subQuery, useNam);
        }
        for (const auto &p : raw) {
            SearchResult sr;
            sr.title = p.first;
            sr.url = p.second;
            out.append(sr);
        }
    }
    if (out.size() > perQueryLimit) out.resize(perQueryLimit);
    if (!hasApiSnippet) {
        for (int i = 0; i < out.size(); ++i) {
            if (out[i].snippet.isEmpty()) {
                out[i].snippet = WebSearchService::fetchPageContent(out[i].url, proxyMode, proxyUrl, useNam, 2000);
                if (out[i].snippet.isEmpty()) out[i].snippet = out[i].title;
            }
        }
    }
    return out;
}

/** 内部：搜索并返回带 snippet 的结果；API 引擎直接使用 API snippet，否则抓取网页
 * 参考 MindSearch：多子查询时合并去重，提升复合问题的覆盖度 */
static QVector<SearchResult> searchWithSnippetsInternal(
    const QString &query,
    const QString &engine,
    const QString &proxyMode,
    const QString &proxyUrl,
    QNetworkAccessManager *nam,
    int maxFetch,
    const QString &apiKey,
    const QString &tencentSecretId,
    const QString &tencentSecretKey)
{
    QVector<SearchResult> out;
    QString focused = extractFocusedQuery(query);
    if (focused.isEmpty()) return out;

    QNetworkAccessManager *useNam = nam;
    QScopedPointer<QNetworkAccessManager> tempNam;
    if (!useNam) {
        tempNam.reset(new QNetworkAccessManager());
        useNam = tempNam.data();
    }
    applyProxyWithMode(useNam, proxyMode, proxyUrl);

    QString e = engine.trimmed().toLower();
    if (e != QLatin1String("duckduckgo") && e != QLatin1String("bing")
        && e != QLatin1String("brave") && e != QLatin1String("google")
        && e != QLatin1String("tencent") && e != QLatin1String("baidu")) {
        e = QStringLiteral("duckduckgo");
    }
    QString key = apiKey.trimmed();
    QString tcId = tencentSecretId.trimmed();
    QString tcKey = tencentSecretKey.trimmed();

    QStringList subQueries = extractSubQueries(focused);
    const int perQuery = (subQueries.size() > 1) ? qMax(3, (maxFetch + subQueries.size() - 1) / subQueries.size()) : maxFetch;

    for (const QString &sq : subQueries) {
        QVector<SearchResult> part = searchOneQuery(sq, e, proxyMode, proxyUrl, useNam, key, tcId, tcKey, perQuery);
        for (const SearchResult &r : part) {
            bool dup = false;
            for (const SearchResult &o : out) {
                if (o.url == r.url) { dup = true; break; }
            }
            if (!dup) out.append(r);
            if (out.size() >= maxFetch) break;
        }
        if (out.size() >= maxFetch) break;
    }
    if (out.size() > maxFetch) out.resize(maxFetch);
    return out;
}

QVector<SearchResult> WebSearchService::searchWithContent(
    const QString &query,
    const QString &engine,
    const QString &proxyMode,
    const QString &proxyUrl,
    QNetworkAccessManager *nam,
    int maxFetch,
    const QString &apiKey,
    const QString &tencentSecretId,
    const QString &tencentSecretKey,
    const QString &conversationContext) {
    QString effectiveQuery = query;
    if (!conversationContext.isEmpty()) {
        effectiveQuery = buildContextualSearchQuery(conversationContext, query);
    }
    return searchWithSnippetsInternal(effectiveQuery, engine, proxyMode, proxyUrl, nam, maxFetch, apiKey, tencentSecretId, tencentSecretKey);
}

/** 参考 MindSearch searcher_input_template + searcher_context_template：
 * 将主问题、历史对话与当前问题合并，使搜索引擎能理解指代（如「它」「这个」） */
QString WebSearchService::buildContextualSearchQuery(
    const QString &conversationContext,
    const QString &currentQuery) {
    if (conversationContext.trimmed().isEmpty())
        return currentQuery.trimmed();
    QString cur = currentQuery.trimmed();
    if (cur.isEmpty()) return QString();

    // 从上下文中提取主问题/主题（主问题：xxx 或 历史问题：xxx）
    QString topic;
    int topicIdx = conversationContext.indexOf(QStringLiteral("主问题："));
    if (topicIdx >= 0) {
        int end = conversationContext.indexOf(QChar(0x0a), topicIdx + 4);
        topic = (end >= 0) ? conversationContext.mid(topicIdx + 4, end - topicIdx - 4).trimmed()
                           : conversationContext.mid(topicIdx + 4).trimmed();
    }
    if (topic.isEmpty()) {
        int histIdx = conversationContext.indexOf(QStringLiteral("历史问题："));
        if (histIdx >= 0) {
            int end = conversationContext.indexOf(QStringLiteral("回答："), histIdx);
            if (end < 0) end = conversationContext.indexOf(QChar(0x0a), histIdx + 4);
            topic = (end >= 0) ? conversationContext.mid(histIdx + 5, end - histIdx - 5).trimmed()
                              : conversationContext.mid(histIdx + 5).trimmed();
        }
    }
    if (topic.isEmpty()) {
        // 回退：取上下文首段非空行（通常即主问题）
        QStringList lines = conversationContext.split(QChar(0x0a), Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            QString l = line.trimmed();
            if (l.length() >= 2 && l.length() <= 80 && !l.startsWith(QStringLiteral("回答：")))
                { topic = l; break; }
        }
    }
    if (topic.length() > 50) topic = topic.left(50).trimmed();

    // 判断当前问题是否包含指代词，需要上下文补全
    static const QStringList pronouns = {
        QStringLiteral("它"), QStringLiteral("这个"), QStringLiteral("那个"), QStringLiteral("其"),
        QStringLiteral("这"), QStringLiteral("那"), QStringLiteral("it"), QStringLiteral("this"),
        QStringLiteral("that"), QStringLiteral("they"), QStringLiteral("them")
    };
    bool needsContext = cur.length() < 10;
    if (!needsContext) {
        for (const QString &p : pronouns) {
            if (cur.contains(p)) { needsContext = true; break; }
        }
    }
    if (needsContext && !topic.isEmpty()) {
        // 将「主问题 当前问题」合并，搜索引擎可据此理解指代
        return (topic + QStringLiteral(" ") + cur).trimmed();
    }
    // MindSearch 风格：多轮对话时附带主问题，提升搜索相关性
    if (!topic.isEmpty() && topic != cur && cur.length() < 30) {
        return (topic + QStringLiteral(" ") + cur).trimmed();
    }
    return cur;
}

QString WebSearchService::buildConversationContextFromMessages(
    const QVariantList &messages,
    bool excludeLast) {
    int n = messages.size();
    if (n <= 1) return QString();
    int end = excludeLast ? n - 1 : n;
    if (end <= 0) return QString();

    QString firstUser;
    QStringList historyParts;
    for (int i = 0; i < end; ++i) {
        QVariantMap m = messages[i].toMap();
        QString role = m[QStringLiteral("role")].toString();
        QString content = m[QStringLiteral("content")].toString().trimmed();
        if (content.isEmpty()) continue;
        if (role == QLatin1String("user")) {
            if (firstUser.isEmpty()) firstUser = content;
            historyParts << QStringLiteral("历史问题：") + content;
        } else if (role == QLatin1String("ai") || role == QLatin1String("assistant")) {
            if (!historyParts.isEmpty())
                historyParts.back().append(QStringLiteral("\n回答：") + content);
        }
    }
    if (firstUser.isEmpty() && historyParts.isEmpty()) return QString();
    QString out = QStringLiteral("主问题：") + firstUser;
    if (!historyParts.isEmpty())
        out += QChar(0x0a) + historyParts.join(QChar(0x0a));
    return out;
}

/** CLI 调试实现：逐步检测代理、测试请求、执行搜索，返回调试报告 */
QString WebSearchService::runSearchDebug(const QString &query, const QString &proxyMode, const QString &proxyUrl) {
    QStringList lines;
    auto ln = [&lines](const QString &s) { lines << s; };
    ln(QStringLiteral("=== DuckDuckGo 搜索调试 ==="));
    ln(QStringLiteral("查询: %1").arg(query.isEmpty() ? QStringLiteral("test") : query));
    ln(QStringLiteral("代理模式: %1").arg(proxyMode.isEmpty() ? QStringLiteral("system") : proxyMode));
    if (!proxyUrl.isEmpty())
        ln(QStringLiteral("手动代理: %1").arg(proxyUrl));
    ln(QStringLiteral(""));

    QNetworkAccessManager nam;
    QString effectiveProxy;
    QString mode = proxyMode.trimmed().toLower();
    if (mode.isEmpty()) mode = QStringLiteral("system");

    if (mode == QLatin1String("off")) {
        nam.setProxy(QNetworkProxy::NoProxy);
        effectiveProxy = QStringLiteral("(无代理)");
        ln(QStringLiteral("[代理] 关闭代理"));
    } else if (mode == QLatin1String("manual")) {
        applyProxyFromUrl(&nam, proxyUrl);
        effectiveProxy = proxyUrl.isEmpty() ? QStringLiteral("(未填写)") : proxyUrl;
        ln(QStringLiteral("[代理] 手动: %1").arg(effectiveProxy));
    } else {
        ln(QStringLiteral("[代理检测] 系统模式"));
        QList<QNetworkProxy> qtProxies = QNetworkProxyFactory::systemProxyForQuery(QNetworkProxyQuery(QUrl("https://html.duckduckgo.com")));
        if (!qtProxies.isEmpty() && qtProxies.first().type() != QNetworkProxy::NoProxy) {
            nam.setProxy(qtProxies.first());
            effectiveProxy = QStringLiteral("%1:%2").arg(qtProxies.first().hostName()).arg(qtProxies.first().port());
            ln(QStringLiteral("  -> Qt 检测: %1").arg(effectiveProxy));
        } else {
            ln(QStringLiteral("  -> Qt: 未检测到"));
            QString envP = proxyUrlFromEnvironment();
            if (!envP.isEmpty()) {
                applyProxyFromUrl(&nam, envP);
                effectiveProxy = envP;
                ln(QStringLiteral("  -> 环境变量: %1").arg(envP));
            } else {
                ln(QStringLiteral("  -> 环境变量: 无"));
                QString gs = proxyUrlFromGnomeGsettings();
                if (!gs.isEmpty()) {
                    applyProxyFromUrl(&nam, gs);
                    effectiveProxy = gs;
                    ln(QStringLiteral("  -> GNOME gsettings: %1").arg(gs));
                } else {
                    ln(QStringLiteral("  -> GNOME gsettings: 无"));
                    QString kde = proxyUrlFromKdeKioslaverc();
                    if (!kde.isEmpty()) {
                        applyProxyFromUrl(&nam, kde);
                        effectiveProxy = kde;
                        ln(QStringLiteral("  -> KDE kioslaverc: %1").arg(kde));
                    } else {
                        ln(QStringLiteral("  -> KDE kioslaverc: 无"));
                        QString fallback = proxyUrlFromCommonVpnPorts();
                        applyProxyFromUrl(&nam, fallback);
                        effectiveProxy = fallback;
                        ln(QStringLiteral("  -> 回退(Clash 7890): %1").arg(fallback));
                    }
                }
            }
        }
    }
    ln(QStringLiteral("  实际使用: %1").arg(effectiveProxy));
    ln(QStringLiteral(""));

    /* 测试原始 GET 请求 */
    ln(QStringLiteral("[步骤1] 测试 GET https://html.duckduckgo.com"));
    QUrl testUrl(QStringLiteral("https://html.duckduckgo.com/"));
    QNetworkRequest req(testUrl);
    req.setHeader(QNetworkRequest::UserAgentHeader,
        QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"));
    req.setRawHeader("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = nam.get(req);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, [&]() { reply->abort(); loop.quit(); });
    timer.start(kNetworkTimeoutMs);
    loop.exec();
    timer.stop();
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QNetworkReply::NetworkError err = reply->error();
    QByteArray body = reply->readAll();
    QString errStr = reply->errorString();
    reply->deleteLater();

    ln(QStringLiteral("  HTTP 状态: %1").arg(statusCode > 0 ? QString::number(statusCode) : QStringLiteral("(无)")));
    ln(QStringLiteral("  错误码: %1 (%2)").arg(static_cast<int>(err)).arg(errStr));
    ln(QStringLiteral("  响应大小: %1 字节").arg(body.size()));
    if (body.size() > 0 && body.size() < 2000)
        ln(QStringLiteral("  响应预览: %1").arg(QString::fromUtf8(body).left(500)));
    else if (body.size() >= 2000)
        ln(QStringLiteral("  响应预览: %1...").arg(QString::fromUtf8(body.left(300))));
    ln(QStringLiteral(""));

    /* 测试 POST 搜索请求 */
    ln(QStringLiteral("[步骤1b] 测试 POST https://html.duckduckgo.com/html/ (实际搜索)"));
    QString q = query.trimmed().isEmpty() ? QStringLiteral("test") : query.trimmed();
    QUrl postUrl(QStringLiteral("https://html.duckduckgo.com/html/"));
    QNetworkRequest postReq(postUrl);
    postReq.setHeader(QNetworkRequest::UserAgentHeader,
        QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"));
    postReq.setRawHeader("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
    postReq.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("application/x-www-form-urlencoded"));
    postReq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QByteArray postBody = QByteArray("q=") + QUrl::toPercentEncoding(q);
    QNetworkReply *postReply = nam.post(postReq, postBody);
    QEventLoop postLoop;
    QTimer postTimer;
    postTimer.setSingleShot(true);
    QObject::connect(postReply, &QNetworkReply::finished, &postLoop, &QEventLoop::quit);
    QObject::connect(&postTimer, &QTimer::timeout, &postLoop, [&]() { postReply->abort(); postLoop.quit(); });
    postTimer.start(kNetworkTimeoutMs);
    postLoop.exec();
    postTimer.stop();
    int postStatus = postReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray postData = postReply->readAll();
    QString postErr = postReply->errorString();
    postReply->deleteLater();
    ln(QStringLiteral("  HTTP 状态: %1").arg(postStatus > 0 ? QString::number(postStatus) : QStringLiteral("(无)")));
    ln(QStringLiteral("  错误: %1").arg(postErr));
    ln(QStringLiteral("  响应大小: %1 字节").arg(postData.size()));
    if (postData.contains("result__a") || postData.contains("result__body"))
        ln(QStringLiteral("  包含 result__a/result__body: 是 (预期结构)"));
    else if (postData.contains("duckduckgo.com/l/"))
        ln(QStringLiteral("  包含 duckduckgo.com/l/: 是"));
    /* 抽取第一个 result__a 附近的 HTML 用于诊断解析 */
    int idx = postData.indexOf("result__a");
    if (idx >= 0) {
        int start = qMax(0, idx - 80);
        int len = qMin(400, postData.size() - start);
        QString snippet = QString::fromUtf8(postData.mid(start, len));
        snippet = snippet.replace(QLatin1Char('\n'), QLatin1Char(' '));
        ln(QStringLiteral("  首个 result__a 片段: %1").arg(snippet));
    }
    if (postData.contains("no-results") || postData.contains("no results"))
        ln(QStringLiteral("  可能: 无结果页"));
    else if (postData.size() > 0 && postData.size() < 1500)
        ln(QStringLiteral("  HTML 预览: %1").arg(QString::fromUtf8(postData).left(800)));
    else if (postData.size() >= 1500)
        ln(QStringLiteral("  HTML 预览: %1...").arg(QString::fromUtf8(postData.left(600))));
    ln(QStringLiteral(""));

    /* 执行实际搜索 */
    ln(QStringLiteral("[步骤2] 解析并执行 DuckDuckGo 搜索"));
    QVector<QPair<QString, QString>> results = search(q, QStringLiteral("duckduckgo"), mode, proxyUrl, &nam);
    ln(QStringLiteral("  解析结果数: %1").arg(results.size()));
    for (int i = 0; i < qMin(results.size(), 5); ++i)
        ln(QStringLiteral("    [%1] %2").arg(i + 1).arg(results[i].first));
    if (results.size() > 5)
        ln(QStringLiteral("    ... 还有 %1 条").arg(results.size() - 5));
    ln(QStringLiteral(""));
    ln(QStringLiteral("=== 调试结束 ==="));
    return lines.join(QChar('\n'));
}
