#ifndef SKILL_MANAGER_H
#define SKILL_MANAGER_H

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class Settings;

class SkillManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList skills READ skills NOTIFY skillsChanged)

public:
    explicit SkillManager(Settings *settings, QObject *parent = nullptr);

    QVariantList skills() const { return m_skills; }

    Q_INVOKABLE void loadSkills();
    Q_INVOKABLE void saveSkills();
    Q_INVOKABLE void addSkill(const QString &title, const QString &content,
                               const QString &icon = QString::fromUtf8("\xF0\x9F\x94\xA7"),
                               const QString &category = "custom");
    Q_INVOKABLE void removeSkill(const QString &id);
    Q_INVOKABLE QVariantMap getSkill(const QString &id) const;
    Q_INVOKABLE void updateSkill(const QString &id, const QString &title,
                                  const QString &content);

signals:
    void skillsChanged();

private:
    QString skillsDirPath() const;
    QString skillsFilePath() const;
    void initDefaultSkills();
    void migrateOldSkillsFile();

    Settings *m_settings;
    QVariantList m_skills;
};

#endif // SKILL_MANAGER_H
