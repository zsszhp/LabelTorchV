#ifndef PROJECTSERVICE_H
#define PROJECTSERVICE_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class Database;
class TaxonomyService;

class ProjectService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentProjectId READ currentProjectId NOTIFY currentProjectChanged)

public:
    explicit ProjectService(QObject *parent = nullptr);

    QString currentProjectId() const { return m_currentProjectId; }

    void setTaxonomyService(TaxonomyService *service) { m_taxonomyService = service; }

    Q_INVOKABLE QString createProject(const QString &name, const QString &rootPath);
    Q_INVOKABLE QVariantList listProjects();
    Q_INVOKABLE bool deleteProject(const QString &projectId);
    Q_INVOKABLE bool openProject(const QString &projectId);
    Q_INVOKABLE void closeProject();
    Q_INVOKABLE QVariantMap getCurrentProject() const;

    /**
     * @brief Get the current task type for a project.
     * Returns: "detect", "obb", "classify", or "anomaly".
     * Defaults to "detect" if not set.
     */
    Q_INVOKABLE QString getTaskType(const QString &projectId);

    /**
     * @brief Set the task type for a project.
     * @param taskType One of: "detect", "obb", "classify", "anomaly"
     */
    Q_INVOKABLE bool setTaskType(const QString &projectId, const QString &taskType);

signals:
    void currentProjectChanged();
    void taskTypeChanged(const QString &projectId, const QString &taskType);

private:
    QString m_currentProjectId;
    TaxonomyService *m_taxonomyService = nullptr;

    bool ensureTaskTypeColumn();
};

#endif
