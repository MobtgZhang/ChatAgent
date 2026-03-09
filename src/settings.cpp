#include "settings.h"
#include "locale_bridge.h"

#include <QFile>
#include <QUrl>
#include <QDir>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDebug>

// ── 构造函数 ──────────────────────────────────────────────────────────────────
Settings::Settings(QObject *parent) : QObject(parent) {
    m_nam = new QNetworkAccessManager(this);
    load();
}


// ── 文件路径 ──────────────────────────────────────────────────────────────────
QString Settings::settingsFilePath() const {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/settings.json";
}

// ── Setters ───────────────────────────────────────────────────────────────────
void Settings::setApiKey(const QString &v)      { if (m_apiKey      != v) { m_apiKey      = v; emit apiKeyChanged(); } }
void Settings::setApiUrl(const QString &v)      { if (m_apiUrl      != v) { m_apiUrl      = v; emit apiUrlChanged(); } }
void Settings::setModelName(const QString &v)   { if (m_modelName   != v) { m_modelName   = v; emit modelNameChanged(); } }
void Settings::setTemperature(double v)         { if (m_temperature != v) { m_temperature = v; emit temperatureChanged(); } }
void Settings::setTopP(double v)                { if (m_topP        != v) { m_topP        = v; emit topPChanged(); } }
void Settings::setTopK(int v)                   { if (m_topK        != v) { m_topK        = v; emit topKChanged(); } }
void Settings::setMaxTokens(int v)              { if (m_maxTokens   != v) { m_maxTokens   = v; emit maxTokensChanged(); } }
void Settings::setSystemPrompt(const QString &v){ if (m_systemPrompt!= v) { m_systemPrompt= v; emit systemPromptChanged(); } }
void Settings::setMaxToolRounds(int v)           { int clamped = qBound(5, v, 40); if (m_maxToolRounds != clamped) { m_maxToolRounds = clamped; emit maxToolRoundsChanged(); } }
void Settings::setShowThinking(bool v)            { if (m_showThinking!= v) { m_showThinking= v; emit showThinkingChanged(); } }
void Settings::setTheme(const QString &v)         { QString t = (v == QStringLiteral("light")) ? QStringLiteral("light") : QStringLiteral("dark"); if (m_theme != t) { m_theme = t; emit themeChanged(); } }
void Settings::setLanguage(const QString &v)      { if (m_language != v && !v.isEmpty()) { m_language = v; emit languageChanged(); } }
void Settings::setCacheDirectory(const QString &v){ if (m_cacheDirectory != v) { m_cacheDirectory = v; emit cacheDirectoryChanged(); } }

QString Settings::urlToLocalPath(const QString &url) const {
    return QUrl(url).toLocalFile();
}

QString Settings::defaultCacheDirectory() const {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir;
}

void Settings::applyLanguage(const QString &lang) {
    if (lang.isEmpty()) return;
    setLanguage(lang);
    save();
    if (m_locale)
        m_locale->reload();
}

bool Settings::hasConfigFile() const {
    return QFile::exists(settingsFilePath());
}

void Settings::setLocaleBridge(LocaleBridge *bridge) {
    m_locale = bridge;
    if (m_locale)
        m_locale->reload();
}

QString Settings::_tr(const QString &key, const QString &arg) const {
    if (m_locale)
        return arg.isEmpty() ? m_locale->tr(key) : m_locale->tr(key, arg);
    return key;
}

// ── 模型列表管理 ──────────────────────────────────────────────────────────────
void Settings::addModel(const QString &model) {
    if (!model.trimmed().isEmpty() && !m_modelList.contains(model)) {
        m_modelList.append(model.trimmed());
        emit modelListChanged();
        save();
    }
}

void Settings::removeModel(const QString &model) {
    if (m_modelList.removeOne(model)) {
        if (m_modelName == model && !m_modelList.isEmpty())
            setModelName(m_modelList.first());
        emit modelListChanged();
        save();
    }
}

// ── 通过 API /models 自动刷新模型列表 ───────────────────────────────────────────
void Settings::refreshModels() {
    if (!m_nam) m_nam = new QNetworkAccessManager(this);

    // 标记刷新开始
    if (!m_modelsRefreshing) {
        m_modelsRefreshing = true;
        m_modelsStatus = _tr("modelsRefreshingStatus");
        emit modelsRefreshingChanged();
        emit modelsStatusChanged();
    }

    // 从当前 apiUrl 推导出 /v1/models 地址（兼容 https://api.openai.com 或带 /v1 的端点）
    if (m_apiUrl.trimmed().isEmpty()) {
        qWarning() << "refreshModels error:" << "apiUrl is empty";
        m_modelsStatus = _tr("modelsErrorEmpty");
        m_modelsRefreshing = false;
        emit modelsRefreshingChanged();
        emit modelsStatusChanged();
        return;
    }

    QUrl base(m_apiUrl);
    if (!base.isValid() || base.scheme().isEmpty()) {
        qWarning() << "refreshModels error:" << "invalid apiUrl:" << m_apiUrl;
        m_modelsStatus = _tr("modelsErrorInvalid");
        m_modelsRefreshing = false;
        emit modelsRefreshingChanged();
        emit modelsStatusChanged();
        return;
    }
    QString path = base.path();

    // 1) 如果没有 /v1，就先补 /v1
    const QString v1Segment = QStringLiteral("/v1");
    if (!path.contains(v1Segment)) {
        if (!path.endsWith(QLatin1Char('/')))
            path.append(QLatin1Char('/'));
        path.append(QStringLiteral("v1"));
    }

    // 2) 去掉可能已经带上的 /chat/completions
    const QString chatComp = QStringLiteral("/chat/completions");
    int idx = path.indexOf(chatComp);
    if (idx >= 0) {
        path = path.left(idx);
    }

    // 3) 补上 /models
    if (!path.endsWith(QLatin1Char('/')))
        path.append(QLatin1Char('/'));
    path.append(QStringLiteral("models"));

    base.setPath(path);

    QNetworkRequest req(base);
    if (!m_apiKey.isEmpty())
        req.setRawHeader("Authorization",
                         QByteArray("Bearer ") + m_apiKey.toUtf8());

    QNetworkReply *reply = m_nam->get(req);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        m_modelsRefreshing = false;

        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "refreshModels error:" << reply->errorString();
            m_modelsStatus = _tr("modelsErrorPrefix") + reply->errorString();
            emit modelsRefreshingChanged();
            emit modelsStatusChanged();
            return;
        }

        const QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject root = doc.object();

        // 尝试按 OpenAI 风格解析：{ "data": [ { "id": "xxx" }, ... ] }
        QStringList models;
        QJsonArray arr = root.value(QStringLiteral("data")).toArray();
        for (const QJsonValue &v : arr) {
            QJsonObject o = v.toObject();
            const QString id = o.value(QStringLiteral("id")).toString();
            if (!id.isEmpty())
                models.append(id);
        }

        if (models.isEmpty()) {
            qWarning() << "refreshModels: no models found in response";
            m_modelsStatus = _tr("modelsErrorNoData");
            emit modelsRefreshingChanged();
            emit modelsStatusChanged();
            return;
        }

        m_modelList = models;
        // 当前模型不在列表中时，自动切到第一个
        if (!m_modelList.contains(m_modelName) && !m_modelList.isEmpty())
            setModelName(m_modelList.first());

        emit modelListChanged();
        save();

        m_modelsStatus = _tr("modelsSuccessFormat", QString::number(m_modelList.size()));
        emit modelsRefreshingChanged();
        emit modelsStatusChanged();
    });
}

// ── 持久化：保存 ──────────────────────────────────────────────────────────────
void Settings::save() {
    QJsonObject root;
    root["apiKey"]       = m_apiKey;
    root["apiUrl"]       = m_apiUrl;
    root["modelName"]    = m_modelName;
    root["temperature"]  = m_temperature;
    root["topP"]         = m_topP;
    root["topK"]         = m_topK;
    root["maxTokens"]    = m_maxTokens;
    root["systemPrompt"]  = m_systemPrompt;
    root["maxToolRounds"]  = m_maxToolRounds;
    root["showThinking"]   = m_showThinking;
    root["theme"]          = m_theme;
    root["language"]       = m_language;
    root["cacheDirectory"] = m_cacheDirectory;

    QJsonArray models;
    for (const QString &m : m_modelList) models.append(m);
    root["modelList"] = models;

    bool existed = QFile::exists(settingsFilePath());
    QFile f(settingsFilePath());
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        if (!existed)
            emit hasConfigFileChanged();
    }
}

// ── 持久化：加载 ──────────────────────────────────────────────────────────────
void Settings::load() {
    QFile f(settingsFilePath());
    if (!f.open(QIODevice::ReadOnly)) {
        // 无配置文件时保持默认英语
        m_language = QStringLiteral("en");
        return;
    }

    QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    if (root.isEmpty()) return;

    // 默认语言为英语，仅在配置中有有效 language 时覆盖
    m_language = QStringLiteral("en");

    if (root.contains("apiKey"))       m_apiKey       = root["apiKey"].toString();
    if (root.contains("apiUrl"))       m_apiUrl       = root["apiUrl"].toString();
    if (root.contains("modelName"))    m_modelName    = root["modelName"].toString();
    if (root.contains("temperature"))  m_temperature  = root["temperature"].toDouble(0.7);
    if (root.contains("topP"))         m_topP         = root["topP"].toDouble(0.9);
    if (root.contains("topK"))         m_topK         = root["topK"].toInt(50);
    if (root.contains("maxTokens"))    m_maxTokens    = root["maxTokens"].toInt(4096);
    if (root.contains("systemPrompt"))  m_systemPrompt  = root["systemPrompt"].toString();
    if (root.contains("maxToolRounds"))  m_maxToolRounds = qBound(5, root["maxToolRounds"].toInt(40), 40);
    if (root.contains("showThinking"))   m_showThinking  = root["showThinking"].toBool(false);
    if (root.contains("theme")) {
        QString t = root["theme"].toString();
        m_theme = (t == QStringLiteral("light")) ? t : QStringLiteral("dark");
    }
    if (root.contains("language")) {
        QString lang = root["language"].toString().trimmed();
        m_language = lang.isEmpty() ? QStringLiteral("en") : lang;
    }
    if (root.contains("cacheDirectory")) m_cacheDirectory = root["cacheDirectory"].toString();

    if (root.contains("modelList")) {
        m_modelList.clear();
        for (const QJsonValue &v : root["modelList"].toArray())
            m_modelList.append(v.toString());
    }
}

