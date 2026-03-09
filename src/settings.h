#ifndef SETTINGS_H
#define SETTINGS_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QNetworkAccessManager>

class Settings : public QObject {
    Q_OBJECT

    // ── API ──────────────────────────────────────────────────────────────────
    Q_PROPERTY(QString   apiKey      READ apiKey      WRITE setApiKey      NOTIFY apiKeyChanged)
    Q_PROPERTY(QString   apiUrl      READ apiUrl      WRITE setApiUrl      NOTIFY apiUrlChanged)
    Q_PROPERTY(QString   modelName   READ modelName   WRITE setModelName   NOTIFY modelNameChanged)
    Q_PROPERTY(QStringList modelList READ modelList   NOTIFY modelListChanged)

    // ── 参数 ─────────────────────────────────────────────────────────────────
    Q_PROPERTY(double temperature READ temperature WRITE setTemperature NOTIFY temperatureChanged)
    Q_PROPERTY(double topP        READ topP        WRITE setTopP        NOTIFY topPChanged)
    Q_PROPERTY(int    topK        READ topK        WRITE setTopK        NOTIFY topKChanged)
    Q_PROPERTY(int    maxTokens   READ maxTokens   WRITE setMaxTokens   NOTIFY maxTokensChanged)

    // ── 系统提示词 ───────────────────────────────────────────────────────────
    Q_PROPERTY(QString systemPrompt READ systemPrompt WRITE setSystemPrompt NOTIFY systemPromptChanged)

    // ── UI ──────────────────────────────────────────────────────────────────
    // 是否显示“思考过程”
    Q_PROPERTY(bool showThinking READ showThinking WRITE setShowThinking NOTIFY showThinkingChanged)

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
    double     temperature()  const { return m_temperature; }
    double     topP()         const { return m_topP; }
    int        topK()         const { return m_topK; }
    int        maxTokens()    const { return m_maxTokens; }
    QString    systemPrompt() const { return m_systemPrompt; }
    bool       showThinking() const { return m_showThinking; }
    bool       modelsRefreshing() const { return m_modelsRefreshing; }
    QString    modelsStatus() const     { return m_modelsStatus; }

    // Setters
    void setApiKey(const QString &v);
    void setApiUrl(const QString &v);
    void setModelName(const QString &v);
    void setTemperature(double v);
    void setTopP(double v);
    void setTopK(int v);
    void setMaxTokens(int v);
    void setSystemPrompt(const QString &v);
    void setShowThinking(bool v);

    // Model list management
    Q_INVOKABLE void addModel(const QString &model);
    Q_INVOKABLE void removeModel(const QString &model);
    Q_INVOKABLE void refreshModels();   // 通过 API /models 自动刷新模型列表

    // Persistence
    Q_INVOKABLE void save();
    Q_INVOKABLE void load();

signals:
    void apiKeyChanged();
    void apiUrlChanged();
    void modelNameChanged();
    void modelListChanged();
    void temperatureChanged();
    void topPChanged();
    void topKChanged();
    void maxTokensChanged();
    void systemPromptChanged();
    void showThinkingChanged();
    void modelsRefreshingChanged();
    void modelsStatusChanged();

private:
    QString     m_apiKey;
    // 这里默认指向 OpenAI 风格的基础地址（可自行改成兼容端点）
    QString     m_apiUrl      = QStringLiteral("https://dashscope.aliyuncs.com/compatible-mode/v1");
    // 改这两行
    QString     m_modelName   = QStringLiteral("qwen3.5-plus");
    QStringList m_modelList   = { "qwen3.5-plus" };   // 启动后由 refreshModels() 覆盖

    double      m_temperature = 0.7;
    double      m_topP        = 0.9;
    int         m_topK        = 50;
    int         m_maxTokens   = 4096;
    QString     m_systemPrompt = QStringLiteral("You are a helpful assistant.");
    bool        m_showThinking = false;

    bool        m_modelsRefreshing = false;
    QString     m_modelsStatus;

    QNetworkAccessManager *m_nam = nullptr;

    QString settingsFilePath() const;
};

#endif // SETTINGS_H

