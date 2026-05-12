#include "ProjectService.h"
#include "TaxonomyService.h"
#include "database/Database.h"
#include "filesystem/ProjectFs.h"
#include "utils/Id.h"
#include "utils/Log.h"
#include <QSqlQuery>
#include <QSqlError>

ProjectService::ProjectService(QObject *parent) : QObject(parent) {}

QString ProjectService::createProject(const QString &name, const QString &rootPath)
{
    ltTrace(LT_LOG_PROJECT()) << "createProject name=" << name << "path=" << rootPath;

    QString projectId = Id::generate();
    ltTrace(LT_LOG_PROJECT()) << "Generated project ID:" << projectId;

    if (!ProjectFs::createProjectDirs(rootPath)) {
        ltError(LT_LOG_PROJECT()) << "Failed to create project directories:" << rootPath;
        return {};
    }

    if (!ProjectFs::createProjectJson(rootPath, name, QStringLiteral("detect"))) {
        ltError(LT_LOG_PROJECT()) << "Failed to create project.json:" << rootPath;
        return {};
    }

    QSqlQuery query(Database::instance().database());
    query.prepare("INSERT INTO projects (id, name, root_path) VALUES (?, ?, ?)");
    query.addBindValue(projectId);
    query.addBindValue(name);
    query.addBindValue(rootPath);

    if (!query.exec()) {
        ltError(LT_LOG_PROJECT()) << "Failed to create project:" << query.lastError().text();
        return {};
    }

    // 为项目创建默认类别体系
    if (m_taxonomyService) {
        m_taxonomyService->createTaxonomy(projectId, "默认类别体系", {});
    }

    ltInfo(LT_LOG_PROJECT()) << "Project created:" << projectId << name << "at" << rootPath;
    return projectId;
}

QVariantList ProjectService::listProjects()
{
    ltTrace(LT_LOG_PROJECT()) << "listProjects";

    QVariantList projects;
    QSqlQuery query(Database::instance().database());
    query.exec("SELECT id, name, root_path, created_at FROM projects ORDER BY updated_at DESC");

    while (query.next()) {
        QVariantMap p;
        p["id"] = query.value(0);
        p["name"] = query.value(1);
        p["rootPath"] = query.value(2);
        p["createdAt"] = query.value(3);
        projects.append(p);
    }

    ltDebug(LT_LOG_PROJECT()) << "Listed" << projects.size() << "projects";
    return projects;
}

bool ProjectService::deleteProject(const QString &projectId)
{
    ltTrace(LT_LOG_PROJECT()) << "deleteProject id=" << projectId;

    QSqlQuery query(Database::instance().database());
    query.prepare("DELETE FROM projects WHERE id = ?");
    query.addBindValue(projectId);
    bool ok = query.exec();

    if (ok) {
        ltInfo(LT_LOG_PROJECT()) << "Project deleted:" << projectId;
    } else {
        ltError(LT_LOG_PROJECT()) << "Failed to delete project:" << query.lastError().text();
    }
    return ok;
}

bool ProjectService::openProject(const QString &projectId)
{
    ltTrace(LT_LOG_PROJECT()) << "openProject id=" << projectId;

    QSqlQuery query(Database::instance().database());
    query.prepare("SELECT id FROM projects WHERE id = ?");
    query.addBindValue(projectId);
    if (query.exec() && query.next()) {
        m_currentProjectId = projectId;
        emit currentProjectChanged();
        ltInfo(LT_LOG_PROJECT()) << "Project opened:" << projectId;
        return true;
    }

    ltWarning(LT_LOG_PROJECT()) << "Project not found:" << projectId;
    return false;
}

void ProjectService::closeProject()
{
    ltTrace(LT_LOG_PROJECT()) << "closeProject";

    m_currentProjectId.clear();
    emit currentProjectChanged();
    ltInfo(LT_LOG_PROJECT()) << "Project closed";
}

QVariantMap ProjectService::getCurrentProject() const
{
    ltTrace(LT_LOG_PROJECT()) << "getCurrentProject";

    if (m_currentProjectId.isEmpty()) return {};

    QSqlQuery query(Database::instance().database());
    query.prepare("SELECT id, name, root_path, default_device, default_model_family, created_at FROM projects WHERE id = ?");
    query.addBindValue(m_currentProjectId);
    if (query.exec() && query.next()) {
        QVariantMap p;
        p["id"] = query.value(0);
        p["name"] = query.value(1);
        p["rootPath"] = query.value(2);
        p["defaultDevice"] = query.value(3);
        p["defaultModelFamily"] = query.value(4);
        p["createdAt"] = query.value(5);
        // Add task type via const-cast (getTaskType is non-const due to column migration)
        p["taskType"] = const_cast<ProjectService*>(this)->getTaskType(m_currentProjectId);
        return p;
    }
    return {};
}

bool ProjectService::ensureTaskTypeColumn()
{
    ltTrace(LT_LOG_PROJECT()) << "ensureTaskTypeColumn";

    QSqlQuery query(Database::instance().database());
    query.exec("SELECT task_type FROM projects LIMIT 0");
    if (query.lastError().isValid()) {
        // Column does not exist, add it via ALTER TABLE
        QSqlQuery alterQuery(Database::instance().database());
        if (!alterQuery.exec("ALTER TABLE projects ADD COLUMN task_type TEXT DEFAULT 'detect'")) {
            ltError(LT_LOG_PROJECT()) << "Failed to add task_type column:" << alterQuery.lastError().text();
            return false;
        }
        ltInfo(LT_LOG_PROJECT()) << "Added task_type column to projects table";
    }
    return true;
}

QString ProjectService::getTaskType(const QString &projectId)
{
    ltTrace(LT_LOG_PROJECT()) << "getTaskType projectId=" << projectId;

    if (projectId.isEmpty()) return QStringLiteral("detect");

    // Ensure the task_type column exists
    ensureTaskTypeColumn();

    QSqlQuery query(Database::instance().database());
    query.prepare("SELECT task_type FROM projects WHERE id = ?");
    query.addBindValue(projectId);
    if (query.exec() && query.next()) {
        QString taskType = query.value(0).toString();
        return taskType.isEmpty() ? QStringLiteral("detect") : taskType;
    }
    return QStringLiteral("detect");
}

bool ProjectService::setTaskType(const QString &projectId, const QString &taskType)
{
    ltTrace(LT_LOG_PROJECT()) << "setTaskType projectId=" << projectId << "taskType=" << taskType;

    if (projectId.isEmpty()) return false;

    // Validate task type
    if (taskType != "detect" && taskType != "obb" && taskType != "classify" && taskType != "anomaly") {
        ltWarning(LT_LOG_PROJECT()) << "Invalid task type:" << taskType;
        return false;
    }

    // Ensure the task_type column exists
    ensureTaskTypeColumn();

    QSqlQuery query(Database::instance().database());
    query.prepare("UPDATE projects SET task_type = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?");
    query.addBindValue(taskType);
    query.addBindValue(projectId);

    if (query.exec()) {
        emit taskTypeChanged(projectId, taskType);
        ltInfo(LT_LOG_PROJECT()) << "Task type set:" << projectId << taskType;
        return true;
    }

    ltError(LT_LOG_PROJECT()) << "Failed to set task type:" << query.lastError().text();
    return false;
}
