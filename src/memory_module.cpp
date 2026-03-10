#include "memory_module.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

struct MemoryModule::Db {
    QSqlDatabase db;
    bool ok = false;
};

void MemoryModule::initDb() {
    if (m_db) return;

    m_db = new Db;
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(path);
    path += "/agent_memory.db";

    m_db->db = QSqlDatabase::addDatabase("QSQLITE", "agent_memory");
    m_db->db.setDatabaseName(path);
    m_db->ok = m_db->db.open();

    if (m_db->ok) {
        QSqlQuery q(m_db->db);
        q.exec("CREATE TABLE IF NOT EXISTS facts ("
               "key TEXT PRIMARY KEY, value TEXT, updated_at INTEGER)");
        q.exec("CREATE TABLE IF NOT EXISTS sops ("
               "name TEXT PRIMARY KEY, content TEXT, summary TEXT, "
               "created_at INTEGER, updated_at INTEGER, usage_count INTEGER DEFAULT 0)");
        q.exec("CREATE TABLE IF NOT EXISTS lessons ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT, event_summary TEXT, lesson TEXT, "
               "confidence REAL DEFAULT 0.8, created_at INTEGER)");
    }
}

MemoryModule::MemoryModule(QObject *parent) : QObject(parent) {
    initDb();
    refreshLongTerm();
    refreshSops();
}

MemoryModule::~MemoryModule() {
    if (m_db) {
        m_db->db.close();
        // 必须先释放 QSqlDatabase 对连接句柄的引用，否则 removeDatabase 会报警
        m_db->db = QSqlDatabase();
        QSqlDatabase::removeDatabase("agent_memory");
        delete m_db;
    }
}

void MemoryModule::appendShortTerm(const QVariantMap &msg) {
    m_shortTerm.append(msg);
    trimShortTerm();
    emit shortTermMessagesChanged();
}

void MemoryModule::clearShortTerm() {
    m_shortTerm.clear();
    emit shortTermMessagesChanged();
}

void MemoryModule::trimShortTerm() {
    while (m_shortTerm.size() > m_windowSize) {
        m_shortTerm.removeFirst();
    }
}

void MemoryModule::addFact(const QString &key, const QString &value) {
    initDb();
    if (!m_db->ok) return;

    QSqlQuery q(m_db->db);
    q.prepare("INSERT OR REPLACE INTO facts (key, value, updated_at) VALUES (?, ?, ?)");
    q.addBindValue(key.trimmed());
    q.addBindValue(value);
    q.addBindValue(QDateTime::currentSecsSinceEpoch());
    q.exec();

    refreshLongTerm();
}

void MemoryModule::removeFact(const QString &key) {
    initDb();
    if (!m_db->ok) return;

    QSqlQuery q(m_db->db);
    q.prepare("DELETE FROM facts WHERE key = ?");
    q.addBindValue(key);
    q.exec();

    refreshLongTerm();
}

void MemoryModule::clearLongTerm() {
    initDb();
    if (!m_db->ok) return;

    QSqlQuery q(m_db->db);
    q.exec("DELETE FROM facts");
    refreshLongTerm();
}

QString MemoryModule::getFact(const QString &key) const {
    if (!m_db || !m_db->ok) return QString();

    QSqlQuery q(m_db->db);
    q.prepare("SELECT value FROM facts WHERE key = ?");
    q.addBindValue(key);
    if (q.exec() && q.next()) {
        return q.value(0).toString();
    }
    return QString();
}

void MemoryModule::refreshLongTerm() {
    initDb();
    m_longTerm.clear();
    if (!m_db->ok) {
        emit longTermFactsChanged();
        return;
    }

    QSqlQuery q(m_db->db);
    q.exec("SELECT key, value FROM facts ORDER BY updated_at DESC");
    while (q.next()) {
        QVariantMap m;
        m["key"] = q.value(0).toString();
        m["value"] = q.value(1).toString();
        m_longTerm.append(m);
    }
    emit longTermFactsChanged();
}

bool MemoryModule::addSop(const QString &name, const QString &content, const QString &summary) {
    initDb();
    if (!m_db->ok || name.trimmed().isEmpty()) return false;

    qint64 now = QDateTime::currentSecsSinceEpoch();
    QString sum = summary.isEmpty() ? content.left(200) : summary;
    QSqlQuery q(m_db->db);
    q.prepare("INSERT INTO sops (name, content, summary, created_at, updated_at, usage_count) "
              "VALUES (?, ?, ?, ?, ?, 0) "
              "ON CONFLICT(name) DO UPDATE SET content=excluded.content, summary=excluded.summary, updated_at=excluded.updated_at");
    q.addBindValue(name.trimmed());
    q.addBindValue(content);
    q.addBindValue(sum);
    q.addBindValue(now);
    q.addBindValue(now);
    bool ok = q.exec();
    if (ok) refreshSops();
    return ok;
}

bool MemoryModule::removeSop(const QString &name) {
    initDb();
    if (!m_db->ok) return false;
    QSqlQuery q(m_db->db);
    q.prepare("DELETE FROM sops WHERE name = ?");
    q.addBindValue(name);
    bool ok = q.exec();
    if (ok) refreshSops();
    return ok;
}

void MemoryModule::refreshSops() {
    m_sopsList = listSops(QString());
    emit sopsChanged();
}

QString MemoryModule::getSop(const QString &name) const {
    if (!m_db || !m_db->ok) return QString();
    QSqlQuery q(m_db->db);
    q.prepare("SELECT content FROM sops WHERE name = ?");
    q.addBindValue(name);
    if (q.exec() && q.next())
        return q.value(0).toString();
    return QString();
}

QVariantList MemoryModule::listSops(const QString &keyword) const {
    QVariantList out;
    if (!m_db || !m_db->ok) return out;

    QSqlQuery q(m_db->db);
    if (keyword.isEmpty()) {
        q.exec("SELECT name, summary, usage_count, updated_at FROM sops ORDER BY usage_count DESC, updated_at DESC");
    } else {
        q.prepare("SELECT name, summary, usage_count, updated_at FROM sops "
                  "WHERE name LIKE ? OR summary LIKE ? OR content LIKE ? ORDER BY usage_count DESC");
        QString kw = "%" + keyword + "%";
        q.addBindValue(kw);
        q.addBindValue(kw);
        q.addBindValue(kw);
        q.exec();
    }
    while (q.next()) {
        QVariantMap m;
        m["name"] = q.value(0).toString();
        m["summary"] = q.value(1).toString();
        m["usage_count"] = q.value(2).toInt();
        m["updated_at"] = q.value(3).toLongLong();
        out.append(m);
    }
    return out;
}

void MemoryModule::incrementSopUsage(const QString &name) {
    initDb();
    if (!m_db->ok) return;
    QSqlQuery q(m_db->db);
    q.prepare("UPDATE sops SET usage_count = usage_count + 1, updated_at = ? WHERE name = ?");
    q.addBindValue(QDateTime::currentSecsSinceEpoch());
    q.addBindValue(name);
    q.exec();
}

bool MemoryModule::addLesson(const QString &eventSummary, const QString &lesson, double confidence) {
    initDb();
    if (!m_db->ok || eventSummary.trimmed().isEmpty() || lesson.trimmed().isEmpty()) return false;
    QSqlQuery q(m_db->db);
    q.prepare("INSERT INTO lessons (event_summary, lesson, confidence, created_at) VALUES (?, ?, ?, ?)");
    q.addBindValue(eventSummary.trimmed());
    q.addBindValue(lesson.trimmed());
    q.addBindValue(qBound(0.0, confidence, 1.0));
    q.addBindValue(QDateTime::currentSecsSinceEpoch());
    return q.exec();
}

QVariantList MemoryModule::getLessons(int limit) const {
    QVariantList out;
    if (!m_db || !m_db->ok) return out;
    QSqlQuery q(m_db->db);
    q.prepare("SELECT event_summary, lesson, confidence FROM lessons ORDER BY created_at DESC LIMIT ?");
    q.addBindValue(limit);
    if (q.exec()) {
        while (q.next()) {
            QVariantMap m;
            m["event_summary"] = q.value(0).toString();
            m["lesson"] = q.value(1).toString();
            m["confidence"] = q.value(2).toDouble();
            out.append(m);
        }
    }
    return out;
}

QString MemoryModule::buildContextForPrompt() const {
    QStringList lines;

    if (!m_longTerm.isEmpty()) {
        lines << "\n[长期记忆 L2 - 用户偏好与事实]";
        for (const QVariant &v : m_longTerm) {
            QVariantMap m = v.toMap();
            lines << QString("- %1: %2").arg(m["key"].toString(), m["value"].toString());
        }
    }

    // 注入 SOP 索引（自进化）
    QVariantList sopsList = listSops(QString());
    if (!sopsList.isEmpty()) {
        lines << "\n[长期记忆 L3 - 已学 SOP]";
        for (const QVariant &v : sopsList) {
            QVariantMap m = v.toMap();
            lines << QString("- %1 (调用%2次): %3")
                     .arg(m["name"].toString())
                     .arg(m["usage_count"].toInt())
                     .arg(m["summary"].toString().left(120));
        }
    }

    // 注入最近教训
    QVariantList lessonsList = getLessons(5);
    if (!lessonsList.isEmpty()) {
        lines << "\n[教训 - 避免重复错误]";
        for (const QVariant &v : lessonsList) {
            QVariantMap m = v.toMap();
            lines << QString("- %1").arg(m["lesson"].toString());
        }
    }

    if (lines.isEmpty()) return QString();
    lines << "";
    return lines.join("\n");
}
