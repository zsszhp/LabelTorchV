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

void AppController::openProject(const QString &projectId, const QString &projectName)
{
    if (m_currentProjectId != projectId) {
        m_currentProjectId = projectId;
        m_currentProjectName = projectName;
        emit currentProjectIdChanged();
        emit currentProjectNameChanged();
        qDebug() << "Project opened:" << projectId << projectName;
    }
}

void AppController::closeProject()
{
    m_currentProjectId.clear();
    m_currentProjectName.clear();
    emit currentProjectIdChanged();
    emit currentProjectNameChanged();
}
