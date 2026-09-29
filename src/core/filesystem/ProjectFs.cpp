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

    ltInfo(LT_LOG_FS()) << "Project root directory initialized:" << rootPath;
    return true;
}

bool ProjectFs::createProjectJson(const QString &rootPath, const QString &projectName,
                                   const QString &taskType)
{
    ltTrace(LT_LOG_FS()) << "createProjectJson rootPath=" << rootPath << "name=" << projectName;

    QString filePath = rootPath + QStringLiteral("/project.json");
    if (QFile::exists(filePath)) {
        ltWarning(LT_LOG_FS()) << "project.json already exists:" << filePath;
        return true;
    }

    QJsonObject json;
    json[QStringLiteral("name")] = projectName;
    json[QStringLiteral("task_type")] = taskType;
    json[QStringLiteral("version")] = QStringLiteral("1.0");
    json[QStringLiteral("created_at")] = QDateTime::currentDateTime().toString(Qt::ISODate);
    json[QStringLiteral("updated_at")] = QDateTime::currentDateTime().toString(Qt::ISODate);
    json[QStringLiteral("labeltorch_version")] = QStringLiteral("0.1.0");

    QJsonDocument doc(json);
    QByteArray content = doc.toJson();

    // 原子写入：先写 .tmp 再 rename，防止崩溃/断电留下半写 project.json
    QString tmpPath = filePath + QStringLiteral(".tmp");
    QFile tmpFile(tmpPath);
    if (!tmpFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        ltError(LT_LOG_FS()) << "Failed to create temp project.json:" << tmpPath;
        return false;
    }
    qint64 written = tmpFile.write(content);
    tmpFile.flush();
    tmpFile.close();
    if (written != content.size()) {
        ltError(LT_LOG_FS()) << "Incomplete write to temp project.json:" << tmpPath;
        QFile::remove(tmpPath);
        return false;
    }
    if (!tmpFile.rename(filePath)) {
        ltError(LT_LOG_FS()) << "Failed to rename temp project.json to:" << filePath;
        QFile::remove(tmpPath);
        return false;
    }

    ltInfo(LT_LOG_FS()) << "project.json created:" << filePath;
    return true;
}

bool ProjectFs::validateProjectDir(const QString &rootPath)
{
    bool valid = QDir(rootPath).exists() && QFileInfo(rootPath + "/project.json").exists();
    ltTrace(LT_LOG_FS()) << "validateProjectDir rootPath=" << rootPath << "valid=" << valid;
    return valid;
}

QString ProjectFs::dataDir(const QString &rootPath) {
    QString path = rootPath + "/data";
    QDir().mkpath(path);
    return path;
}
QString ProjectFs::datasetsDir(const QString &rootPath) {
    QString path = rootPath + "/data/datasets";
    QDir().mkpath(path);
    return path;
}
QString ProjectFs::snapshotsDir(const QString &rootPath) {
    QString path = rootPath + "/data/snapshots";
    QDir().mkpath(path);
    return path;
}
QString ProjectFs::taxonomyDir(const QString &rootPath) {
    QString path = rootPath + "/data/taxonomy";
    QDir().mkpath(path);
    return path;
}
QString ProjectFs::revisionsDir(const QString &rootPath) {
    QString path = rootPath + "/data/revisions";
    QDir().mkpath(path);
    return path;
}
QString ProjectFs::modelsDir(const QString &rootPath) {
    QString path = rootPath + "/models";
    QDir().mkpath(path);
    return path;
}
QString ProjectFs::versionsDir(const QString &rootPath) {
    QString path = rootPath + "/models/versions";
    QDir().mkpath(path);
    return path;
}
QString ProjectFs::runsDir(const QString &rootPath) {
    QString path = rootPath + "/models/runs";
    QDir().mkpath(path);
    return path;
}
QString ProjectFs::exportsDir(const QString &rootPath) {
    QString path = rootPath + "/exports";
    QDir().mkpath(path);
    return path;
}
QString ProjectFs::cacheDir(const QString &rootPath) {
    QString path = rootPath + "/cache";
    QDir().mkpath(path);
    return path;
}
QString ProjectFs::thumbnailsDir(const QString &rootPath) {
    QString path = rootPath + "/cache/thumbnails";
    QDir().mkpath(path);
    return path;
}
QString ProjectFs::logsDir(const QString &rootPath) {
    QString path = rootPath + "/logs";
    QDir().mkpath(path);
    return path;
}
