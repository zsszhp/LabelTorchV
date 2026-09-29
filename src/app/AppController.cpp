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

void AppController::setPythonBackendReady(bool ready)
{
    if (m_pythonBackendReady != ready) {
        m_pythonBackendReady = ready;
        emit pythonBackendReadyChanged();
        ltInfo(LT_LOG_APP()) << "Python backend ready changed to:" << ready;
    }
}

void AppController::reportNanAssert(const QString &detail)
{
    // 累计 NaN ASSERT 次数：hook 降级后布局/几何可能已脏，必须让上层可见
    ++m_nanAssertCount;
    emit nanAssertCountChanged();

    // 前几次打印详情便于定位；之后仅计数，避免日志刷屏
    if (m_nanAssertCount <= 5) {
        ltWarning(LT_LOG_APP()) << "NaN ASSERT #" << m_nanAssertCount << ":" << detail;
    }

    // 首次越过阈值时提示建议重启（只提示一次，不重复打扰）
    if (!m_nanRestartAdvised && m_nanAssertCount >= nanRestartThreshold()) {
        m_nanRestartAdvised = true;
        ltError(LT_LOG_APP()) << "NaN ASSERT 累计已达" << m_nanAssertCount
                              << "次，几何状态可能已脏，建议保存工作后重启应用";
        emit nanRestartAdvised(m_nanAssertCount);
    }
}

