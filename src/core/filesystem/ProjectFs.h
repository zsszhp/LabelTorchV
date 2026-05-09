#ifndef PROJECTFS_H
#define PROJECTFS_H

#include <QObject>
#include <QString>

/**
 * @brief 项目文件系统管理
 *
 * 按 11-project-filesystem-spec-v2.md 规范管理项目目录结构
 */
class ProjectFs : public QObject
{
    Q_OBJECT

public:
    explicit ProjectFs(QObject *parent = nullptr);

    static bool createProjectDirs(const QString &rootPath);
    static bool validateProjectDir(const QString &rootPath);

    static QString dataDir(const QString &rootPath);
    static QString datasetsDir(const QString &rootPath);
    static QString snapshotsDir(const QString &rootPath);
    static QString taxonomyDir(const QString &rootPath);
    static QString revisionsDir(const QString &rootPath);
    static QString modelsDir(const QString &rootPath);
    static QString versionsDir(const QString &rootPath);
    static QString runsDir(const QString &rootPath);
    static QString exportsDir(const QString &rootPath);
    static QString cacheDir(const QString &rootPath);
    static QString thumbnailsDir(const QString &rootPath);
    static QString logsDir(const QString &rootPath);
};

#endif // PROJECTFS_H
