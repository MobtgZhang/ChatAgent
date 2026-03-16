#ifndef FILE_TOOL_H
#define FILE_TOOL_H

#include "base_tool.h"

class Settings;

/**
 * 文件操作工具：读、写、列目录
 * 支持相对路径和绝对路径，工作目录为应用数据目录的子目录
 */
class FileTool : public BaseTool {
    Q_OBJECT

public:
    explicit FileTool(Settings *settings, QObject *parent = nullptr);
    explicit FileTool(QObject *parent = nullptr);

    QString name() const override { return QStringLiteral("file"); }
    QString description() const override;
    QVariantMap parametersSchema() const override;
    QString execute(const QVariantMap &args) override;

private:
    Settings *m_settings = nullptr;

    QString agentFilesBase() const;
    QString readFile(const QString &path);
    QString writeFile(const QString &path, const QString &content);
    QString listDir(const QString &path);
    QString createDir(const QString &path);
    QString safePath(const QString &path);
};

#endif // FILE_TOOL_H
