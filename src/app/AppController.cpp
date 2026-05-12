#include "AppController.h"
#include "utils/Log.h"

AppController::AppController(QObject *parent)
    : QObject(parent)
{
    ltTrace(LT_LOG_APP()) << "AppController constructed";
}

void AppController::setCurrentPage(const QString &page)
{
    ltTrace(LT_LOG_APP()) << "setCurrentPage page=" << page << "current=" << m_currentPage;
    if (m_currentPage != page) {
        m_currentPage = page;
        emit currentPageChanged();
        ltInfo(LT_LOG_APP()) << "Page changed to:" << page;
    }
}

void AppController::openProject(const QString &projectId, const QString &projectName)
{
    ltTrace(LT_LOG_APP()) << "openProject id=" << projectId << "name=" << projectName;
    if (m_currentProjectId != projectId) {
        m_currentProjectId = projectId;
        m_currentProjectName = projectName;
        emit currentProjectIdChanged();
        emit currentProjectNameChanged();
        ltInfo(LT_LOG_APP()) << "Project opened:" << projectId << projectName;
    }
}

void AppController::closeProject()
{
    ltTrace(LT_LOG_APP()) << "closeProject";
    m_currentProjectId.clear();
    m_currentProjectName.clear();
    emit currentProjectIdChanged();
    emit currentProjectNameChanged();
    ltInfo(LT_LOG_APP()) << "Project closed";
}
