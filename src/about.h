#ifndef ABOUT_H
#define ABOUT_H

#include <QObject>
#include <QString>

class About : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString appName    READ appName    CONSTANT)
    Q_PROPERTY(QString version    READ version    CONSTANT)
    Q_PROPERTY(QString buildDate  READ buildDate  CONSTANT)
    Q_PROPERTY(QString description READ description CONSTANT)
    Q_PROPERTY(QString qtVersion  READ qtVersion  CONSTANT)
    Q_PROPERTY(QString license    READ license    CONSTANT)
    Q_PROPERTY(QString homepage   READ homepage   CONSTANT)

public:
    explicit About(QObject *parent = nullptr);

    QString appName()     const { return QStringLiteral("ChatAgent"); }
    QString version()     const { return QStringLiteral("1.0.0"); }
    QString buildDate()   const { return QStringLiteral(__DATE__ " " __TIME__); }
    QString description() const { return QStringLiteral("支持 OpenAI 兼容接口的本地 AI 聊天客户端，\n支持流式输出、Markdown 渲染与数学公式显示。"); }
    QString qtVersion()   const { return QStringLiteral(QT_VERSION_STR); }
    QString license()     const { return QStringLiteral("MIT License"); }
    QString homepage()    const { return QStringLiteral("https://github.com/mobtgzhang/chat-agent"); }
};

#endif // ABOUT_H

