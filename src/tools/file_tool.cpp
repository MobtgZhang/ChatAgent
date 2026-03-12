#include "file_tool.h"
#include "settings.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDebug>

FileTool::FileTool(Settings *settings, QObject *parent)
    : BaseTool(parent), m_settings(settings) {}

FileTool::FileTool(QObject *parent) : BaseTool(parent) {}

QString FileTool::agentFilesBase() const {
    QString base = m_settings
        ? m_settings->effectiveDataDirectory()
        : QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(base);
    dir.mkpath(QStringLiteral("agent_files"));
    dir.cd(QStringLiteral("agent_files"));
    return dir.absolutePath();
}

QString FileTool::description() const {
    return QStringLiteral("读取、写入文件、列出目录或创建文件夹。支持 read/write/list/mkdir 四种操作。创建文件夹请用 mkdir，路径相对于应用数据目录下的 agent_files。");
}

QVariantMap FileTool::parametersSchema() const {
    return QVariantMap{
        { "type", "object" },
        { "properties", QVariantMap{
            { "action", QVariantMap{
                { "type", "string" },
                { "enum", QVariantList{"read", "write", "list", "mkdir"} },
                { "description", "操作类型：read 读取文件，write 写入文件，list 列目录，mkdir 创建文件夹" }
            }},
            { "path", QVariantMap{
                { "type", "string" },
                { "description", "文件或目录路径（支持相对路径，相对应用数据目录）" }
            }},
            { "content", QVariantMap{
                { "type", "string" },
                { "description", "写入时的文件内容（仅 write 时需要）" }
            }}
        }},
        { "required", QVariantList{"action", "path"} }
    };
}

QString FileTool::safePath(const QString &path) {
    QDir baseDir(agentFilesBase());

    QFileInfo info(baseDir.absoluteFilePath(path));
    QString canonical = info.canonicalFilePath();
    QString baseCanonical = baseDir.canonicalPath();

    if (!canonical.startsWith(baseCanonical)) {
        return QString();
    }
    return canonical;
}

QString FileTool::readFile(const QString &path) {
    QString safe = safePath(path);
    if (safe.isEmpty()) return QStringLiteral("{\"error\":\"路径不合法\"}");

    QFile f(safe);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString("{\"error\":\"无法打开文件: %1\"}").arg(f.errorString());
    }
    QString content = QString::fromUtf8(f.readAll());
    QJsonObject o;
    o["content"] = content;
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

QString FileTool::writeFile(const QString &path, const QString &content) {
    QString safe = safePath(path);
    if (safe.isEmpty()) return QStringLiteral("{\"error\":\"路径不合法\"}");

    QFile f(safe);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return QString("{\"error\":\"无法写入: %1\"}").arg(f.errorString());
    }
    f.write(content.toUtf8());
    f.close();
    return QStringLiteral("{\"success\":true,\"path\":\"%1\"}").arg(safe);
}

QString FileTool::createDir(const QString &path) {
    QString canonicalBase = QDir(agentFilesBase()).canonicalPath();

    QString absPath = QDir(canonicalBase).absoluteFilePath(path);
    QString cleanPath = QDir::cleanPath(absPath);
    if (!cleanPath.startsWith(canonicalBase + QChar('/')) && cleanPath != canonicalBase) {
        return QStringLiteral("{\"error\":\"路径不合法\"}");
    }
    QDir d;
    if (!d.mkpath(cleanPath)) {
        return QStringLiteral("{\"error\":\"无法创建目录: %1\"}").arg(cleanPath);
    }
    QFileInfo info(cleanPath);
    return QStringLiteral("{\"success\":true,\"path\":\"%1\"}").arg(info.canonicalFilePath());
}

QString FileTool::listDir(const QString &path) {
    QString safe = path.isEmpty() ? agentFilesBase() : safePath(path);
    if (safe.isEmpty()) return QStringLiteral("{\"error\":\"路径不合法\"}");

    QDir dir(safe);
    if (!dir.exists()) return QStringLiteral("{\"error\":\"目录不存在\"}");

    QJsonArray entries;
    for (const QFileInfo &fi : dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name)) {
        QJsonObject e;
        e["name"] = fi.fileName();
        e["type"] = fi.isDir() ? "dir" : "file";
        e["size"] = fi.isFile() ? static_cast<qint64>(fi.size()) : 0;
        entries.append(e);
    }
    QJsonObject o;
    o["entries"] = entries;
    o["path"] = dir.absolutePath();
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

QString FileTool::execute(const QVariantMap &args) {
    QString action = args.value("action").toString();
    QString path   = args.value("path").toString();
    QString content = args.value("content").toString();

    QString result;
    if (action == "read") {
        result = readFile(path);
    } else if (action == "write") {
        result = writeFile(path, content);
    } else if (action == "list") {
        result = listDir(path);
    } else if (action == "mkdir") {
        result = createDir(path);
    } else {
        result = QStringLiteral("{\"error\":\"未知操作: %1\"}").arg(action);
    }

    emit executionFinished(name(), result);
    return result;
}
