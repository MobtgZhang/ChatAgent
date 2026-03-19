#include "skill_manager.h"
#include "settings.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <QUuid>

SkillManager::SkillManager(Settings *settings, QObject *parent)
    : QObject(parent), m_settings(settings) {
    migrateOldSkillsFile();
    loadSkills();
}

QString SkillManager::skillsDirPath() const {
    return m_settings->effectiveDataDirectory() + "/skills";
}

QString SkillManager::skillsFilePath() const {
    return skillsDirPath() + "/skills.json";
}

void SkillManager::migrateOldSkillsFile() {
    QString oldPath = m_settings->effectiveDataDirectory() + "/memory/skills.json";
    QString newPath = skillsFilePath();
    if (QFile::exists(oldPath) && !QFile::exists(newPath)) {
        QDir().mkpath(skillsDirPath());
        QFile::copy(oldPath, newPath);
    }
}

void SkillManager::loadSkills() {
    m_skills.clear();

    QFile f(skillsFilePath());
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
        initDefaultSkills();
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isArray()) {
        initDefaultSkills();
        return;
    }

    QJsonArray arr = doc.array();
    for (const QJsonValue &v : arr) {
        QJsonObject obj = v.toObject();
        QVariantMap skill;
        skill["id"]         = obj["id"].toString();
        skill["title"]      = obj["title"].toString();
        skill["content"]    = obj["content"].toString();
        skill["icon"]       = obj["icon"].toString(QString::fromUtf8("\xF0\x9F\x94\xA7"));
        skill["category"]   = obj["category"].toString("custom");
        skill["created_at"] = obj["created_at"].toInteger();
        skill["updated_at"] = obj["updated_at"].toInteger();
        m_skills.append(skill);
    }

    if (m_skills.isEmpty())
        initDefaultSkills();

    emit skillsChanged();
}

void SkillManager::saveSkills() {
    QDir().mkpath(skillsDirPath());

    QJsonArray arr;
    for (const QVariant &v : m_skills) {
        QVariantMap m = v.toMap();
        QJsonObject obj;
        obj["id"]         = m["id"].toString();
        obj["title"]      = m["title"].toString();
        obj["content"]    = m["content"].toString();
        obj["icon"]       = m["icon"].toString();
        obj["category"]   = m["category"].toString();
        obj["created_at"] = m["created_at"].toLongLong();
        obj["updated_at"] = m["updated_at"].toLongLong();
        arr.append(obj);
    }

    QFile f(skillsFilePath());
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

void SkillManager::addSkill(const QString &title, const QString &content,
                             const QString &icon, const QString &category) {
    QVariantMap skill;
    skill["id"]       = QUuid::createUuid().toString(QUuid::WithoutBraces);
    skill["title"]    = title;
    skill["content"]  = content;
    skill["icon"]     = icon.isEmpty() ? QString::fromUtf8("\xF0\x9F\x94\xA7") : icon;
    skill["category"] = category.isEmpty() ? "custom" : category;
    qint64 now = QDateTime::currentSecsSinceEpoch();
    skill["created_at"] = now;
    skill["updated_at"] = now;

    m_skills.append(skill);
    saveSkills();
    emit skillsChanged();
}

void SkillManager::removeSkill(const QString &id) {
    for (int i = 0; i < m_skills.size(); ++i) {
        if (m_skills[i].toMap()["id"].toString() == id) {
            m_skills.removeAt(i);
            saveSkills();
            emit skillsChanged();
            return;
        }
    }
}

QVariantMap SkillManager::getSkill(const QString &id) const {
    for (const QVariant &v : m_skills) {
        QVariantMap m = v.toMap();
        if (m["id"].toString() == id)
            return m;
    }
    return QVariantMap();
}

void SkillManager::updateSkill(const QString &id, const QString &title,
                                const QString &content) {
    for (int i = 0; i < m_skills.size(); ++i) {
        QVariantMap m = m_skills[i].toMap();
        if (m["id"].toString() == id) {
            m["title"]      = title;
            m["content"]    = content;
            m["updated_at"] = QDateTime::currentSecsSinceEpoch();
            m_skills[i] = m;
            saveSkills();
            emit skillsChanged();
            return;
        }
    }
}

void SkillManager::initDefaultSkills() {
    m_skills.clear();
    qint64 now = QDateTime::currentSecsSinceEpoch();

    auto addCoreSkill = [&](const QString &id, const QString &title,
                            const QString &icon, const QString &content) {
        QVariantMap s;
        s["id"]         = id;
        s["title"]      = title;
        s["icon"]       = icon;
        s["category"]   = QStringLiteral("core");
        s["content"]    = content;
        s["created_at"] = now;
        s["updated_at"] = now;
        m_skills.append(s);
    };

    addCoreSkill(
        QStringLiteral("memory_management_sop"),
        QStringLiteral("Memory Management"),
        QStringLiteral("\xF0\x9F\xA7\xA0"),
        QStringLiteral(
            "Agent Memory Management SOP (L0 Constitution)\n\n"
            "Memory Hierarchy:\n"
            "- L0 Meta-memory: rules governing memory management itself\n"
            "- L2 Global Facts: user environment, preferences, credentials, paths\n"
            "- L3 Task SOPs: standard operating procedures learned from experience\n\n"
            "Core Principles:\n"
            "1. After solving a new problem, distill key steps into an SOP\n"
            "2. Before starting a similar task, search existing SOPs first\n"
            "3. Store user preferences and environment info in L2 facts\n"
            "4. Record failed attempts as lessons to avoid repeating mistakes\n"
            "5. Periodically clean outdated information to keep memory efficient"));

    addCoreSkill(
        QStringLiteral("autonomous_operation_sop"),
        QStringLiteral("Autonomous Operation"),
        QStringLiteral("\xF0\x9F\xA4\x96"),
        QStringLiteral(
            "Autonomous Task Execution SOP\n\n"
            "Execution Flow:\n"
            "1. Receive task -> Analyze intent -> Decompose into sub-steps\n"
            "2. For each sub-step: select tool -> execute -> verify result\n"
            "3. On obstacle: try alternative -> install dependency -> search solution\n"
            "4. On completion: summarize flow -> save as SOP -> report result\n\n"
            "Safety Principles:\n"
            "- Confirm before dangerous operations (delete, modify system files)\n"
            "- Keep network requests and file operations within reasonable scope\n"
            "- Maintain operation reversibility; backup before critical operations"));

    addCoreSkill(
        QStringLiteral("scheduled_task_sop"),
        QStringLiteral("Scheduled Tasks"),
        QStringLiteral("\xe2\x8f\xb0"),
        QStringLiteral(
            "Scheduled Task Management SOP\n\n"
            "Manage periodic auto-execution tasks, similar to Cron.\n\n"
            "Steps:\n"
            "1. Define task: specify content, trigger condition, frequency\n"
            "2. Write script: encapsulate task logic into standalone scripts\n"
            "3. Configure scheduler: set timed trigger rules\n"
            "4. Monitor execution: log results, detect anomalies and alert\n\n"
            "Use Cases:\n"
            "- Periodic data collection and analysis\n"
            "- System status monitoring and alerting\n"
            "- File backup and cleanup\n"
            "- Message push and reminders"));

    addCoreSkill(
        QStringLiteral("web_setup_sop"),
        QStringLiteral("Web Environment Setup"),
        QStringLiteral("\xF0\x9F\x8C\x90"),
        QStringLiteral(
            "Browser Environment Setup SOP\n\n"
            "Configure and manage browser automation environment.\n\n"
            "Steps:\n"
            "1. Detect browser environment: check available browsers and versions\n"
            "2. Install driver dependencies: configure WebDriver or injection bridge\n"
            "3. Configure connection: establish communication channel with browser\n"
            "4. Verify functionality: test page loading, element ops, JS execution\n"
            "5. Save configuration: record successful configuration parameters\n\n"
            "Core Capabilities:\n"
            "- Web content reading and parsing\n"
            "- Form filling and submission\n"
            "- Page navigation and interaction\n"
            "- Maintain login state operations"));

    addCoreSkill(
        QStringLiteral("desktop_control_sop"),
        QStringLiteral("Desktop Control"),
        QStringLiteral("\xF0\x9F\x96\xA5\xef\xb8\x8f"),
        QStringLiteral(
            "Desktop Physical Control SOP\n\n"
            "Automate desktop applications via keyboard/mouse simulation.\n\n"
            "Core Capabilities:\n"
            "1. Keyboard control: simulate keystrokes, combos, text input\n"
            "2. Mouse control: move, click, drag, scroll\n"
            "3. Screen awareness: screenshot, OCR text recognition, image matching\n"
            "4. Window management: switch, resize, position control\n"
            "5. DPI adaptation: auto-detect screen scaling, coordinate adaptation\n\n"
            "Usage Notes:\n"
            "- Ensure target window is in foreground before operating\n"
            "- Screenshot to confirm current state before critical operations\n"
            "- Use reasonable operation intervals\n"
            "- Stop and report on anomalies"));

    saveSkills();
    emit skillsChanged();
}
