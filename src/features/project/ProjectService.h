#ifndef PROJECTSERVICE_H
#define PROJECTSERVICE_H

#include <QObject>
#include <QString>
#include <QVariantList>

class Database;

class ProjectService : public QObject
{
    Q_OBJECT
public:
    explicit ProjectService(QObject *parent = nullptr);

    Q_INVOKABLE QString createProject(const QString &name, const QString &rootPath);
    Q_INVOKABLE QVariantList listProjects();
    Q_INVOKABLE bool deleteProject(const QString &projectId);
    Q_INVOKABLE bool openProject(const QString &projectId);
};

#endif
