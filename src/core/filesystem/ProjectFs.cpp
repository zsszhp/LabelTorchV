#include "ProjectFs.h"
#include "utils/Log.h"
#include <QDir>

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
