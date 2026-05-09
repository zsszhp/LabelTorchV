#include "AppController.h"
#include <QDebug>

AppController::AppController(QObject *parent)
    : QObject(parent)
{
}

void AppController::setCurrentPage(const QString &page)
{
    if (m_currentPage != page) {
        m_currentPage = page;
        emit currentPageChanged();
    }
}

void AppController::createProject(const QString &name, const QString &path)
{
    Q_UNUSED(name)
    Q_UNUSED(path)
    // TODO: 实现项目创建逻辑（Task 3）
    qDebug() << "Create project:" << name << path;
}

void AppController::openProject(const QString &path)
{
    if (m_currentProject != path) {
        m_currentProject = path;
        emit currentProjectChanged();
    }
}

void AppController::closeProject()
{
    m_currentProject.clear();
    emit currentProjectChanged();
}
