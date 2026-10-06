#ifndef APPCONTROLLER_H
#define APPCONTROLLER_H

#include <QObject>
#include <QString>

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentPage READ currentPage WRITE setCurrentPage NOTIFY currentPageChanged)
    Q_PROPERTY(QString currentProjectId READ currentProjectId NOTIFY currentProjectIdChanged)
    Q_PROPERTY(QString currentProjectName READ currentProjectName NOTIFY currentProjectNameChanged)
    Q_PROPERTY(bool projectOpen READ projectOpen NOTIFY currentProjectIdChanged)
    Q_PROPERTY(bool pythonBackendReady READ pythonBackendReady NOTIFY pythonBackendReadyChanged)
    // NaN ASSERT 累计次数：Debug 下 hook 降级后几何可能已脏，UI 需据此告警
    Q_PROPERTY(int nanAssertCount READ nanAssertCount NOTIFY nanAssertCountChanged)
    // 超过阈值后为 true，状态栏提示用户建议重启
    Q_PROPERTY(bool nanRestartRecommended READ nanRestartRecommended NOTIFY nanAssertCountChanged)

public:
    explicit AppController(QObject *parent = nullptr);

    QString currentPage() const { return m_currentPage; }
    void setCurrentPage(const QString &page);

    QString currentProjectId() const { return m_currentProjectId; }
    QString currentProjectName() const { return m_currentProjectName; }
    bool projectOpen() const { return !m_currentProjectId.isEmpty(); }

    bool pythonBackendReady() const { return m_pythonBackendReady; }
    void setPythonBackendReady(bool ready);

    Q_INVOKABLE void openProject(const QString &projectId, const QString &projectName);
    Q_INVOKABLE void closeProject();

    /// 用系统文件管理器打开日志目录（AppDataLocation/logs）
    Q_INVOKABLE void openLogsDir();

    int nanAssertCount() const { return m_nanAssertCount; }
    bool nanRestartRecommended() const { return m_nanAssertCount >= nanRestartThreshold(); }
    // 超过该次数后状态栏建议重启（降级只是掩盖，几何状态可能已脏）
    Q_INVOKABLE int nanRestartThreshold() const { return 10; }

    // 由 main.cpp 的 CRT hook / 消息处理器调用（可跨线程，内部队列化到主线程）
    Q_INVOKABLE void reportNanAssert(const QString &detail);

signals:
    void currentPageChanged();
    void currentProjectIdChanged();
    void currentProjectNameChanged();
    void pythonBackendReadyChanged();
    void nanAssertCountChanged();
    // 首次越过阈值时发出一次，供日志/状态栏展示
    void nanRestartAdvised(int count);

private:
    QString m_currentPage = "project";
    QString m_currentProjectId;
    QString m_currentProjectName;
    bool m_pythonBackendReady = false;
    int m_nanAssertCount = 0;
    bool m_nanRestartAdvised = false;
};

#endif // APPCONTROLLER_H
