#ifndef SETTINGS_H
#define SETTINGS_H

#include <QObject>
#include <QString>
#include <QStringList>

class LocaleBridge;
class QNetworkAccessManager;

class Settings : public QObject {
    Q_OBJECT

    // ── API ──────────────────────────────────────────────────────────────────
    Q_PROPERTY(QString   apiKey      READ apiKey      WRITE setApiKey      NOTIFY apiKeyChanged)
    Q_PROPERTY(QString   apiUrl      READ apiUrl      WRITE setApiUrl      NOTIFY apiUrlChanged)
    Q_PROPERTY(QString   modelName   READ modelName   WRITE setModelName   NOTIFY modelNameChanged)
    Q_PROPERTY(QStringList modelList READ modelList   NOTIFY modelListChanged)
    // 工作区专用模型（空则回退 modelName）：Ask / Plan / Agent / Debug
    Q_PROPERTY(QString modelNameAsk   READ modelNameAsk   WRITE setModelNameAsk   NOTIFY modelNameAskChanged)
    Q_PROPERTY(QString modelNamePlan  READ modelNamePlan  WRITE setModelNamePlan  NOTIFY modelNamePlanChanged)
    Q_PROPERTY(QString modelNameAgent READ modelNameAgent WRITE setModelNameAgent NOTIFY modelNameAgentChanged)
    Q_PROPERTY(QString modelNameDebug READ modelNameDebug WRITE setModelNameDebug NOTIFY modelNameDebugChanged)
    // 工作区温度覆盖：<0 表示使用全局 temperature
    Q_PROPERTY(double temperatureAsk   READ temperatureAsk   WRITE setTemperatureAsk   NOTIFY temperatureAskChanged)
    Q_PROPERTY(double temperaturePlan  READ temperaturePlan  WRITE setTemperaturePlan  NOTIFY temperaturePlanChanged)
    Q_PROPERTY(double temperatureAgent READ temperatureAgent WRITE setTemperatureAgent NOTIFY temperatureAgentChanged)
    Q_PROPERTY(double temperatureDebug READ temperatureDebug WRITE setTemperatureDebug NOTIFY temperatureDebugChanged)

    // ── 参数 ─────────────────────────────────────────────────────────────────
    Q_PROPERTY(double temperature READ temperature WRITE setTemperature NOTIFY temperatureChanged)
    Q_PROPERTY(double topP        READ topP        WRITE setTopP        NOTIFY topPChanged)
    Q_PROPERTY(int    topK        READ topK        WRITE setTopK        NOTIFY topKChanged)
    Q_PROPERTY(int    maxTokens   READ maxTokens   WRITE setMaxTokens   NOTIFY maxTokensChanged)

    // ── 系统提示词 ───────────────────────────────────────────────────────────
    Q_PROPERTY(QString systemPrompt READ systemPrompt WRITE setSystemPrompt NOTIFY systemPromptChanged)

    // ── Agent 行为 ──────────────────────────────────────────────────────────────
    Q_PROPERTY(int maxToolRounds READ maxToolRounds WRITE setMaxToolRounds NOTIFY maxToolRoundsChanged)
    // Debug 工作区是否允许 shell（及 python）工具
    Q_PROPERTY(bool debugAllowShell READ debugAllowShell WRITE setDebugAllowShell NOTIFY debugAllowShellChanged)

    // ── 系统设置 ────────────────────────────────────────────────────────────
    Q_PROPERTY(QString theme          READ theme          WRITE setTheme          NOTIFY themeChanged)
    Q_PROPERTY(QString language        READ language       WRITE setLanguage       NOTIFY languageChanged)
    Q_PROPERTY(QString cacheDirectory READ cacheDirectory WRITE setCacheDirectory NOTIFY cacheDirectoryChanged)
    // 联网搜索：DuckDuckGo | Bing | Brave | Google | Tencent
    Q_PROPERTY(QString searchEngine   READ searchEngine   WRITE setSearchEngine   NOTIFY searchEngineChanged)
    // 搜索 API Key：Bing/Brave/Google(Serper) 需要
    Q_PROPERTY(QString webSearchApiKey READ webSearchApiKey WRITE setWebSearchApiKey NOTIFY webSearchApiKeyChanged)
    // 腾讯搜索：SecretId + SecretKey（选 tencent 时使用）
    Q_PROPERTY(QString tencentSecretId  READ tencentSecretId  WRITE setTencentSecretId  NOTIFY tencentSecretIdChanged)
    Q_PROPERTY(QString tencentSecretKey READ tencentSecretKey WRITE setTencentSecretKey NOTIFY tencentSecretKeyChanged)
    // 网络代理：off=关闭, system=系统代理, manual=手动输入
    Q_PROPERTY(QString proxyMode      READ proxyMode      WRITE setProxyMode      NOTIFY proxyModeChanged)
    // 手动代理时的 URL，格式如 http://127.0.0.1:7890 或 socks5://127.0.0.1:1080
    Q_PROPERTY(QString proxyUrl       READ proxyUrl       WRITE setProxyUrl       NOTIFY proxyUrlChanged)
    Q_PROPERTY(bool hasConfigFile READ hasConfigFile NOTIFY hasConfigFileChanged)

    // ── UI ──────────────────────────────────────────────────────────────────
    // 是否显示“思考过程”
    Q_PROPERTY(bool showThinking READ showThinking WRITE setShowThinking NOTIFY showThinkingChanged)
    // 对话模式下是否启用联网搜索（RAG）
    Q_PROPERTY(bool chatOnline READ chatOnline WRITE setChatOnline NOTIFY chatOnlineChanged)

    // 模型刷新状态与提示信息（用于设置界面显示“刷新中/成功/失败”）
    Q_PROPERTY(bool    modelsRefreshing READ modelsRefreshing NOTIFY modelsRefreshingChanged)
    Q_PROPERTY(QString modelsStatus     READ modelsStatus     NOTIFY modelsStatusChanged)

public:
    explicit Settings(QObject *parent = nullptr);

    // Getters
    QString    apiKey()       const { return m_apiKey; }
    QString    apiUrl()       const { return m_apiUrl; }
    QString    modelName()    const { return m_modelName; }
    QStringList modelList()   const { return m_modelList; }
    QString    modelNameAsk()   const { return m_modelNameAsk; }
    QString    modelNamePlan()  const { return m_modelNamePlan; }
    QString    modelNameAgent() const { return m_modelNameAgent; }
    QString    modelNameDebug() const { return m_modelNameDebug; }
    double     temperatureAsk()   const { return m_temperatureAsk; }
    double     temperaturePlan()  const { return m_temperaturePlan; }
    double     temperatureAgent() const { return m_temperatureAgent; }
    double     temperatureDebug() const { return m_temperatureDebug; }
    double     temperature()  const { return m_temperature; }
    double     topP()         const { return m_topP; }
    int        topK()         const { return m_topK; }
    int        maxTokens()    const { return m_maxTokens; }
    QString    systemPrompt() const { return m_systemPrompt; }
    int        maxToolRounds() const { return m_maxToolRounds; }
    bool       debugAllowShell() const { return m_debugAllowShell; }
    bool       showThinking() const { return m_showThinking; }
    bool       chatOnline()   const { return m_chatOnline; }
    bool       modelsRefreshing() const { return m_modelsRefreshing; }
    QString    modelsStatus() const     { return m_modelsStatus; }
    QString    theme() const { return m_theme; }
    QString    language() const { return m_language; }
    QString    cacheDirectory() const { return m_cacheDirectory; }
    QString    searchEngine() const { return m_searchEngine; }
    QString    webSearchApiKey() const { return m_webSearchApiKey; }
    QString    tencentSecretId() const { return m_tencentSecretId; }
    QString    tencentSecretKey() const { return m_tencentSecretKey; }
    QString    proxyMode() const { return m_proxyMode; }
    QString    proxyUrl() const { return m_proxyUrl; }
    bool       hasConfigFile() const;

    // Setters
    void setApiKey(const QString &v);
    void setApiUrl(const QString &v);
    void setModelName(const QString &v);
    void setModelNameAsk(const QString &v);
    void setModelNamePlan(const QString &v);
    void setModelNameAgent(const QString &v);
    void setModelNameDebug(const QString &v);
    void setTemperatureAsk(double v);
    void setTemperaturePlan(double v);
    void setTemperatureAgent(double v);
    void setTemperatureDebug(double v);
    void setTemperature(double v);
    void setTopP(double v);
    void setTopK(int v);
    void setMaxTokens(int v);
    void setSystemPrompt(const QString &v);
    void setMaxToolRounds(int v);
    void setDebugAllowShell(bool v);
    void setShowThinking(bool v);
    void setChatOnline(bool v);
    void setTheme(const QString &v);
    void setLanguage(const QString &v);
    void setCacheDirectory(const QString &v);
    void setSearchEngine(const QString &v);
    void setWebSearchApiKey(const QString &v);
    void setTencentSecretId(const QString &v);
    void setTencentSecretKey(const QString &v);
    void setProxyMode(const QString &v);
    void setProxyUrl(const QString &v);

    // 将 file:// URL 转为本地路径（供 QML FolderDialog 使用）
    Q_INVOKABLE QString urlToLocalPath(const QString &url) const;
    // 获取默认缓存目录（AppDataLocation）
    Q_INVOKABLE QString defaultCacheDirectory() const;
    // 获取实际使用的数据目录（用户设置 > 默认），所有模块统一使用此方法
    QString effectiveDataDirectory() const;

    // Model list management
    Q_INVOKABLE void addModel(const QString &model);
    Q_INVOKABLE void removeModel(const QString &model);
    Q_INVOKABLE void refreshModels();   // 通过 API /models 自动刷新模型列表

    // Persistence
    Q_INVOKABLE void save();
    Q_INVOKABLE void load();

    // 切换语言并立即生效（供 QML 调用）
    Q_INVOKABLE void applyLanguage(const QString &lang);

    /** chatMode: ask | plan | agent | debug */
    Q_INVOKABLE QString effectiveModelForChatMode(const QString &chatMode) const;
    Q_INVOKABLE double effectiveTemperatureForChatMode(const QString &chatMode) const;

    void setLocaleBridge(LocaleBridge *bridge);

signals:
    void apiKeyChanged();
    void apiUrlChanged();
    void modelNameChanged();
    void modelListChanged();
    void modelNameAskChanged();
    void modelNamePlanChanged();
    void modelNameAgentChanged();
    void modelNameDebugChanged();
    void temperatureAskChanged();
    void temperaturePlanChanged();
    void temperatureAgentChanged();
    void temperatureDebugChanged();
    void temperatureChanged();
    void topPChanged();
    void topKChanged();
    void maxTokensChanged();
    void systemPromptChanged();
    void maxToolRoundsChanged();
    void debugAllowShellChanged();
    void showThinkingChanged();
    void chatOnlineChanged();
    void modelsRefreshingChanged();
    void modelsStatusChanged();
    void themeChanged();
    void languageChanged();
    void cacheDirectoryChanged();
    void searchEngineChanged();
    void webSearchApiKeyChanged();
    void tencentSecretIdChanged();
    void tencentSecretKeyChanged();
    void proxyModeChanged();
    void proxyUrlChanged();
    void hasConfigFileChanged();

private:
    QString     m_apiKey;
    // 这里默认指向 OpenAI 风格的基础地址（可自行改成兼容端点）
    QString     m_apiUrl      = QStringLiteral("https://dashscope.aliyuncs.com/compatible-mode/v1");
    // 改这两行
    QString     m_modelName   = QStringLiteral("qwen3.5-plus");
    QStringList m_modelList   = { "qwen3.5-plus" };   // 启动后由 refreshModels() 覆盖
    QString     m_modelNameAsk;
    QString     m_modelNamePlan;
    QString     m_modelNameAgent;
    QString     m_modelNameDebug;
    double      m_temperatureAsk   = -1.0;
    double      m_temperaturePlan  = -1.0;
    double      m_temperatureAgent = -1.0;
    double      m_temperatureDebug = -1.0;

    double      m_temperature = 0.7;
    double      m_topP        = 0.9;
    int         m_topK        = 50;
    int         m_maxTokens   = 4096;
    QString     m_systemPrompt;
    int         m_maxToolRounds = 40;
    bool        m_debugAllowShell = false;
    bool        m_showThinking = false;
    bool        m_chatOnline   = true;    // 对话模式下联网搜索开关（默认开启以便用户看到联网功能）

    QString     m_theme          = QStringLiteral("dark");   // "dark" | "light"
    QString     m_language       = QStringLiteral("en");     // "en" | "zh" | "ru" | "fr"
    QString     m_cacheDirectory;   // 空则使用默认
    QString     m_searchEngine   = QStringLiteral("duckduckgo");  // duckduckgo | baidu | bing | brave | google | tencent
    QString     m_webSearchApiKey;   // Bing/Brave/Google(Serper) API Key
    QString     m_tencentSecretId;   // 腾讯联网搜索
    QString     m_tencentSecretKey;
    QString     m_proxyMode     = QStringLiteral("system");  // off | system | manual（默认 system 便于 VPN 用户访问 DuckDuckGo）
    QString     m_proxyUrl;        // 手动代理时的 URL，如 http://127.0.0.1:7890

    bool        m_modelsRefreshing = false;
    QString     m_modelsStatus;

    QNetworkAccessManager *m_nam = nullptr;
    LocaleBridge *m_locale = nullptr;

    QString settingsFilePath() const;
    QString _tr(const QString &key, const QString &arg = QString()) const;
    void syncWorkspaceModelsToList();
};

#endif // SETTINGS_H

