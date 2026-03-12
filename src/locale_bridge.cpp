#include "locale_bridge.h"
#include "settings.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QDir>
#include <QHash>
#include <QVariantMap>

LocaleBridge::LocaleBridge(Settings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    reload();
    if (m_settings)
        connect(m_settings, &Settings::languageChanged, this, &LocaleBridge::reload);
}

void LocaleBridge::reload()
{
    QString langCode = m_settings ? m_settings->language() : QStringLiteral("en");
    if (langCode.isEmpty())
        langCode = QStringLiteral("en");

    if (loadFromJson(langCode)) {
        m_lang = langCode;
    } else if (m_t.isEmpty()) {
        // 首次加载且所有翻译文件失败时，使用内置英文回退，避免界面完全无文字
        loadFromJson(QStringLiteral("en"));
        m_lang = QStringLiteral("en");
    }
    if (!m_t.isEmpty()) {
        ++m_tVersion;
        emit tChanged();
        emit langChanged();
        emit tVersionChanged();  // int 类型确保 QML 绑定可靠刷新
    }
}

bool LocaleBridge::loadFromJson(const QString &langCode)
{
    QString path = QStringLiteral(":/src/translations/%1.json").arg(langCode);
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (langCode != QStringLiteral("en")) {
            return loadFromJson(QStringLiteral("en"));
        }
        return false;
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    f.close();
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return false;

    // 先填充临时 map，成功后再替换，避免加载失败时清空 m_t 导致界面文字丢失
    QVariantMap newMap;
    QJsonObject obj = doc.object();
    for (auto it = obj.begin(); it != obj.end(); ++it)
        newMap.insert(it.key(), it.value().toString());

    m_t = newMap;
    return true;
}

QVariantList LocaleBridge::availableLanguageList() const
{
    QVariantList out;
    // 始终根据现有翻译文件动态生成语言列表（遵循 i18n 约定），
    // 不再依赖 settings.json 是否存在。
    // 探测 :/src/translations/ 下的 *.json
    QDir dir(QStringLiteral(":/src/translations"));
    QStringList files = dir.entryList(QStringList{QStringLiteral("*.json")}, QDir::Files);
    QStringList codes;
    for (const QString &f : files) {
        QString code = f;
        if (code.endsWith(QStringLiteral(".json")))
            code = code.chopped(5);
        if (!code.isEmpty())
            codes.append(code);
    }
    if (codes.isEmpty()) {
        QVariantMap m;
        m.insert(QStringLiteral("code"), QStringLiteral("en"));
        m.insert(QStringLiteral("name"), QStringLiteral("English"));
        out.append(m);
        return out;
    }
    // 排序：英文最前，其余按字母
    codes.removeDuplicates();
    if (codes.contains(QStringLiteral("en")))
        codes.removeAll(QStringLiteral("en"));
    codes.sort(Qt::CaseInsensitive);
    codes.prepend(QStringLiteral("en"));

    // 语言显示名称：不依赖当前界面语言，而是从“各自语言”的翻译文件中读取：
    // 对于代码 c，例如 "zh"，约定在 :/src/translations/zh.json 中有键 "languageZh"，
    // 直接用该值作为显示名；若读取失败或键缺失，则退回为代码首字母大写形式。
    for (const QString &c : codes) {
        QVariantMap m;
        m.insert(QStringLiteral("code"), c);

        QString name;
        QString path = QStringLiteral(":/src/translations/%1.json").arg(c);
        QFile f(path);
        if (f.open(QIODevice::ReadOnly)) {
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
            f.close();
            if (err.error == QJsonParseError::NoError && doc.isObject()) {
                QJsonObject obj = doc.object();
                name = obj.value(QStringLiteral("languageSelfName")).toString();
                if (name.isEmpty()) {
                    QString key = QStringLiteral("language") + c.left(1).toUpper() + c.mid(1);
                    name = obj.value(key).toString();
                }
            }
        }
        if (name.isEmpty()) {
            name = c.left(1).toUpper() + c.mid(1);
        }

        m.insert(QStringLiteral("name"), name);
        out.append(m);
    }
    return out;
}

QString LocaleBridge::tr(const QString &key,
                         const QString &arg1,
                         const QString &arg2,
                         const QString &arg3) const
{
    QString s = m_t.value(key).toString();
    if (s.isEmpty())
        return key;
    if (!arg1.isEmpty()) s = s.arg(arg1);
    if (!arg2.isEmpty()) s = s.arg(arg2);
    if (!arg3.isEmpty()) s = s.arg(arg3);
    return s;
}
