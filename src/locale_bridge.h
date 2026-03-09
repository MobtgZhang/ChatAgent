#ifndef LOCALE_BRIDGE_H
#define LOCALE_BRIDGE_H

#include <QObject>
#include <QVariantMap>
#include <QVariantList>
#include <QString>

class Settings;

class LocaleBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap t READ t NOTIFY tChanged)
    Q_PROPERTY(QString lang READ lang NOTIFY langChanged)
    Q_PROPERTY(int tVersion READ tVersion NOTIFY tVersionChanged)

public:
    explicit LocaleBridge(Settings *settings, QObject *parent = nullptr);

    QVariantMap t() const { return m_t; }
    QString lang() const { return m_lang; }
    int tVersion() const { return m_tVersion; }

    Q_INVOKABLE QString tr(const QString &key,
                            const QString &arg1 = QString(),
                            const QString &arg2 = QString(),
                            const QString &arg3 = QString()) const;

    // 动态探测翻译文件夹中的语言，英文排最前；无配置文件时仅返回英语
    Q_INVOKABLE QVariantList availableLanguageList() const;

public slots:
    void reload();

signals:
    void tChanged();
    void langChanged();
    void tVersionChanged();

private:
    bool loadFromJson(const QString &langCode);

    Settings *m_settings = nullptr;
    QVariantMap m_t;
    QString m_lang;
    int m_tVersion = 0;
};

#endif // LOCALE_BRIDGE_H
