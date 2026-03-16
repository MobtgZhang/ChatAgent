#ifndef SKILL_MANAGER_H
#define SKILL_MANAGER_H

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class Settings;

class SkillManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList skills READ skills NOTIFY skillsChanged)
    Q_PROPERTY(QVariantList folders READ folders NOTIFY foldersChanged)

public:
    explicit SkillManager(Settings *settings, QObject *parent = nullptr);

    QVariantList skills() const { return m_skills; }
    QVariantList folders() const { return m_folders; }

    Q_INVOKABLE void loadSkills();
    Q_INVOKABLE void saveSkills();
    Q_INVOKABLE void addSkill(const QString &title, const QString &content,
                               const QString &icon = QString::fromUtf8("\xF0\x9F\x94\xA7"),
                               const QString &category = "custom");
    Q_INVOKABLE void removeSkill(const QString &id);
    Q_INVOKABLE QVariantMap getSkill(const QString &id) const;
    Q_INVOKABLE void updateSkill(const QString &id, const QString &title,
                                  const QString &content);
    Q_INVOKABLE void updateSkillFolder(const QString &id, const QString &folderId);

    // 文件夹操作
    Q_INVOKABLE void addFolder(const QString &name);
    Q_INVOKABLE void removeFolder(const QString &id);
    Q_INVOKABLE void renameFolder(const QString &id, const QString &name);
    Q_INVOKABLE QVariantMap getFolder(const QString &id) const;

signals:
    void skillsChanged();
    void foldersChanged();

private:
    QString memoryDirPath() const;
    QString skillsFilePath() const;
    QString foldersFilePath() const;
    void initDefaultSkills();
    void loadFolders();
    void saveFolders();

    Settings *m_settings;
    QVariantList m_skills;
    QVariantList m_folders;
};

#endif // SKILL_MANAGER_H
