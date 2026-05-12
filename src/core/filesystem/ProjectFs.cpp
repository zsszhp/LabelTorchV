#include "ProjectFs.h"
#include "utils/Log.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>

ProjectFs::ProjectFs(QObject *parent) : QObject(parent) {}

bool ProjectFs::createProjectDirs(const QString &rootPath)
{
    ltTrace(LT_LOG_FS()) << "createProjectDirs rootPath=" << rootPath;

    QDir dir(rootPath);
    if (!dir.exists() && !dir.mkpath(".")) {
        ltError(LT_LOG_FS()) << "Failed to create project root:" << rootPath;
        return false;
    }

    const QStringList subDirs = {
        "data/datasets", "data/snapshots", "data/taxonomy", "data/revisions",
        "models/versions", "models/runs",
        "exports",
        "cache/thumbnails", "cache/temp",
        "logs", "diagnostics"
    };

    for (const auto &sub : subDirs) {
        if (!dir.mkpath(sub)) {
            ltError(LT_LOG_FS()) << "Failed to create subdirectory:" << sub << "under" << rootPath;
            return false;
        }
    }

    ltInfo(LT_LOG_FS()) << "Project directories created:" << rootPath;
    return true;
}

bool ProjectFs::createProjectJson(const QString &rootPath, const QString &projectName,
                                   const QString &taskType)
{
    ltTrace(LT_LOG_FS()) << "createProjectJson rootPath=" << rootPath << "name=" << projectName;

    QString filePath = rootPath + QStringLiteral("/project.json");
    QFile file(filePath);
    if (file.exists()) {
        ltWarning(LT_LOG_FS()) << "project.json already exists:" << filePath;
        return true;
    }

    if (!file.open(QIODevice::WriteOnly)) {
        ltError(LT_LOG_FS()) << "Failed to create project.json:" << filePath;
        return false;
    }

    QJsonObject json;
    json[QStringLiteral("name")] = projectName;
    json[QStringLiteral("task_type")] = taskType;
    json[QStringLiteral("version")] = QStringLiteral("1.0");
    json[QStringLiteral("created_at")] = QDateTime::currentDateTime().toString(Qt::ISODate);
    json[QStringLiteral("updated_at")] = QDateTime::currentDateTime().toString(Qt::ISODate);
    json[QStringLiteral("labeltorch_version")] = QStringLiteral("0.1.0");

    QJsonDocument doc(json);
    file.write(doc.toJson());
    file.close();

    ltInfo(LT_LOG_FS()) << "project.json created:" << filePath;
    return true;
}

bool ProjectFs::validateProjectDir(const QString &rootPath)
{
    bool valid = QDir(rootPath).exists() && QFileInfo(rootPath + "/project.json").exists();
    ltTrace(LT_LOG_FS()) << "validateProjectDir rootPath=" << rootPath << "valid=" << valid;
    return valid;
}

QString ProjectFs::dataDir(const QString &rootPath) { return rootPath + "/data"; }
QString ProjectFs::datasetsDir(const QString &rootPath) { return rootPath + "/data/datasets"; }
QString ProjectFs::snapshotsDir(const QString &rootPath) { return rootPath + "/data/snapshots"; }
QString ProjectFs::taxonomyDir(const QString &rootPath) { return rootPath + "/data/taxonomy"; }
QString ProjectFs::revisionsDir(const QString &rootPath) { return rootPath + "/data/revisions"; }
QString ProjectFs::modelsDir(const QString &rootPath) { return rootPath + "/models"; }
QString ProjectFs::versionsDir(const QString &rootPath) { return rootPath + "/models/versions"; }
QString ProjectFs::runsDir(const QString &rootPath) { return rootPath + "/models/runs"; }
QString ProjectFs::exportsDir(const QString &rootPath) { return rootPath + "/exports"; }
QString ProjectFs::cacheDir(const QString &rootPath) { return rootPath + "/cache"; }
QString ProjectFs::thumbnailsDir(const QString &rootPath) { return rootPath + "/cache/thumbnails"; }
QString ProjectFs::logsDir(const QString &rootPath) { return rootPath + "/logs"; }
