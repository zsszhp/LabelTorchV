#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QObject>
#include <QSettings>
#include <QStringList>
#include <QSize>

class AppSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList recentProjects READ recentProjects NOTIFY recentProjectsChanged)
    Q_PROPERTY(QString lastProjectPath READ lastProjectPath WRITE setLastProjectPath NOTIFY lastProjectPathChanged)
    Q_PROPERTY(QString pythonPath READ pythonPath WRITE setPythonPath NOTIFY pythonPathChanged)
    Q_PROPERTY(QSize windowSize READ windowSize WRITE setWindowSize NOTIFY windowSizeChanged)
    Q_PROPERTY(bool windowMaximized READ windowMaximized WRITE setWindowMaximized NOTIFY windowMaximizedChanged)

public:
    explicit AppSettings(QObject *parent = nullptr);

    QStringList recentProjects() const;
    Q_INVOKABLE void addRecentProject(const QString &projectPath);
    Q_INVOKABLE void removeRecentProject(const QString &projectPath);
    Q_INVOKABLE void clearRecentProjects();

    QString lastProjectPath() const;
    void setLastProjectPath(const QString &path);

    QString pythonPath() const;
    void setPythonPath(const QString &path);

    QSize windowSize() const;
    void setWindowSize(const QSize &size);

    bool windowMaximized() const;
    void setWindowMaximized(bool maximized);

    Q_INVOKABLE QString defaultDevice() const;
    Q_INVOKABLE void setDefaultDevice(const QString &device);

    Q_INVOKABLE QString defaultModelFamily() const;
    Q_INVOKABLE void setDefaultModelFamily(const QString &family);

signals:
    void recentProjectsChanged();
    void lastProjectPathChanged();
    void pythonPathChanged();
    void windowSizeChanged();
    void windowMaximizedChanged();

private:
    QSettings m_settings;
};

#endif // APPSETTINGS_H
