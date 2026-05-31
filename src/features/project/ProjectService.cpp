#include "ProjectService.h"
#include "TaxonomyService.h"
#include "database/Database.h"
#include "filesystem/ProjectFs.h"
#include "utils/Id.h"
#include "utils/Log.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>

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

    ensureTaskTypeColumn();

    QSqlQuery query(Database::instance().database());
    query.prepare("INSERT INTO projects (id, name, root_path, task_type) VALUES (?, ?, ?, ?)");
    query.addBindValue(projectId);
    query.addBindValue(name);
    query.addBindValue(rootPath);
    query.addBindValue(QStringLiteral("detect"));

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

QString ProjectService::importProject(const QString &rootPath)
{
    ltInfo(LT_LOG_PROJECT()) << "importProject rootPath=" << rootPath;

    if (rootPath.isEmpty()) {
        ltWarning(LT_LOG_PROJECT()) << "importProject: empty rootPath";
        return {};
    }

    // 检查目录是否存在
    QFileInfo rootInfo(rootPath);
    if (!rootInfo.exists() || !rootInfo.isDir()) {
        ltWarning(LT_LOG_PROJECT()) << "importProject: directory does not exist:" << rootPath;
        return {};
    }

    // 读取 project.json 获取项目名称和任务类型
    QString projectName;
    QString taskType = QStringLiteral("detect");

    QString jsonPath = rootPath + QStringLiteral("/project.json");
    QFile jsonFile(jsonPath);
    if (jsonFile.exists() && jsonFile.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(jsonFile.readAll());
        jsonFile.close();
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            projectName = obj[QStringLiteral("name")].toString();
            taskType = obj[QStringLiteral("task_type")].toString();
        }
    }

    // 如果没有 project.json，用目录名作为项目名
    if (projectName.isEmpty()) {
        projectName = rootInfo.fileName();
        if (projectName.isEmpty()) {
            projectName = QStringLiteral("导入项目");
        }
    }

    // 检查是否已导入过（同一路径不重复导入）
    QSqlQuery checkQuery(Database::instance().database());
    checkQuery.prepare("SELECT id FROM projects WHERE root_path = ?");
    checkQuery.addBindValue(rootPath);
    if (checkQuery.exec() && checkQuery.next()) {
        QString existingId = checkQuery.value(0).toString();
        ltWarning(LT_LOG_PROJECT()) << "importProject: project already imported:" << existingId;
        return existingId;
    }

    // 确保目录结构完整（补齐缺失的子目录）
    if (!ProjectFs::createProjectDirs(rootPath)) {
        ltError(LT_LOG_PROJECT()) << "importProject: failed to ensure project dirs:" << rootPath;
        return {};
    }

    // 如果没有 project.json 则创建
    if (!QFile::exists(jsonPath)) {
        if (!ProjectFs::createProjectJson(rootPath, projectName, taskType)) {
            ltError(LT_LOG_PROJECT()) << "importProject: failed to create project.json";
            return {};
        }
    }

    // 在数据库中注册项目
    QString projectId = Id::generate();
    ensureTaskTypeColumn();

    QSqlQuery query(Database::instance().database());
    query.prepare("INSERT INTO projects (id, name, root_path, task_type) VALUES (?, ?, ?, ?)");
    query.addBindValue(projectId);
    query.addBindValue(projectName);
    query.addBindValue(rootPath);
    query.addBindValue(taskType);

    if (!query.exec()) {
        ltError(LT_LOG_PROJECT()) << "importProject: failed to insert project:" << query.lastError().text();
        return {};
    }

    // 为项目创建默认类别体系
    if (m_taxonomyService) {
        m_taxonomyService->createTaxonomy(projectId, QStringLiteral("默认类别体系"), {});
    }

    ltInfo(LT_LOG_PROJECT()) << "Project imported:" << projectId << projectName << "at" << rootPath;
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
