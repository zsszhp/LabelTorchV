#include "ProjectService.h"
#include "database/Database.h"
#include "filesystem/ProjectFs.h"
#include "utils/Id.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

ProjectService::ProjectService(QObject *parent) : QObject(parent) {}

QString ProjectService::createProject(const QString &name, const QString &rootPath)
{
    QString projectId = Id::generate();

    if (!ProjectFs::createProjectDirs(rootPath)) return {};

    QSqlQuery query(Database::instance().database());
    query.prepare("INSERT INTO projects (id, name, root_path) VALUES (?, ?, ?)");
    query.addBindValue(projectId);
    query.addBindValue(name);
    query.addBindValue(rootPath);

    if (!query.exec()) {
        qWarning() << "Failed to create project:" << query.lastError().text();
        return {};
    }

    qDebug() << "Project created:" << projectId << name;
    return projectId;
}

QVariantList ProjectService::listProjects()
{
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
    return projects;
}

bool ProjectService::deleteProject(const QString &projectId)
{
    QSqlQuery query(Database::instance().database());
    query.prepare("DELETE FROM projects WHERE id = ?");
    query.addBindValue(projectId);
    return query.exec();
}

bool ProjectService::openProject(const QString &projectId)
{
    Q_UNUSED(projectId)
    // TODO: 加载项目数据到内存
    return true;
}
