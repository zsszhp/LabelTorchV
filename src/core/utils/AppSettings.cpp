#include "AppSettings.h"

AppSettings::AppSettings(QObject *parent)
    : QObject(parent)
    , m_settings(QStringLiteral("LabelTorch"), QStringLiteral("LabelTorchV"))
{
}

QStringList AppSettings::recentProjects() const
{
    return m_settings.value(QStringLiteral("recentProjects")).toStringList();
}

void AppSettings::addRecentProject(const QString &projectPath)
{
    QStringList list = recentProjects();
    list.removeAll(projectPath);
    list.prepend(projectPath);
    while (list.size() > 10) {
        list.removeLast();
    }
    m_settings.setValue(QStringLiteral("recentProjects"), list);
    emit recentProjectsChanged();
}

void AppSettings::removeRecentProject(const QString &projectPath)
{
    QStringList list = recentProjects();
    list.removeAll(projectPath);
    m_settings.setValue(QStringLiteral("recentProjects"), list);
    emit recentProjectsChanged();
}

void AppSettings::clearRecentProjects()
{
    m_settings.remove(QStringLiteral("recentProjects"));
    emit recentProjectsChanged();
}

QString AppSettings::lastProjectPath() const
{
    return m_settings.value(QStringLiteral("lastProjectPath")).toString();
}

void AppSettings::setLastProjectPath(const QString &path)
{
    if (m_settings.value(QStringLiteral("lastProjectPath")).toString() == path) return;
    m_settings.setValue(QStringLiteral("lastProjectPath"), path);
    emit lastProjectPathChanged();
}

QString AppSettings::pythonPath() const
{
    return m_settings.value(QStringLiteral("pythonPath"), QStringLiteral("python")).toString();
}

void AppSettings::setPythonPath(const QString &path)
{
    if (m_settings.value(QStringLiteral("pythonPath")).toString() == path) return;
    m_settings.setValue(QStringLiteral("pythonPath"), path);
    emit pythonPathChanged();
}

QSize AppSettings::windowSize() const
{
    return m_settings.value(QStringLiteral("windowSize"), QSize(1280, 800)).toSize();
}

void AppSettings::setWindowSize(const QSize &size)
{
    m_settings.setValue(QStringLiteral("windowSize"), size);
    emit windowSizeChanged();
}

bool AppSettings::windowMaximized() const
{
    return m_settings.value(QStringLiteral("windowMaximized"), false).toBool();
}

void AppSettings::setWindowMaximized(bool maximized)
{
    m_settings.setValue(QStringLiteral("windowMaximized"), maximized);
    emit windowMaximizedChanged();
}

QString AppSettings::defaultDevice() const
{
    return m_settings.value(QStringLiteral("defaultDevice"), QStringLiteral("auto")).toString();
}

void AppSettings::setDefaultDevice(const QString &device)
{
    m_settings.setValue(QStringLiteral("defaultDevice"), device);
}

QString AppSettings::defaultModelFamily() const
{
    return m_settings.value(QStringLiteral("defaultModelFamily"), QStringLiteral("yolov8")).toString();
}

void AppSettings::setDefaultModelFamily(const QString &family)
{
    m_settings.setValue(QStringLiteral("defaultModelFamily"), family);
}
