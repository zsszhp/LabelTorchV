#ifndef APPCONTROLLER_H
#define APPCONTROLLER_H

#include <QObject>
#include <QString>

/**
 * @brief 全局应用控制器
 *
 * 管理应用级状态：当前项目、导航页面、Python后端状态等
 */
class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentPage READ currentPage WRITE setCurrentPage NOTIFY currentPageChanged)
    Q_PROPERTY(QString currentProject READ currentProject NOTIFY currentProjectChanged)
    Q_PROPERTY(bool pythonBackendReady READ pythonBackendReady NOTIFY pythonBackendReadyChanged)

public:
    explicit AppController(QObject *parent = nullptr);

    QString currentPage() const { return m_currentPage; }
    void setCurrentPage(const QString &page);

    QString currentProject() const { return m_currentProject; }

    bool pythonBackendReady() const { return m_pythonBackendReady; }

    Q_INVOKABLE void createProject(const QString &name, const QString &path);
    Q_INVOKABLE void openProject(const QString &path);
    Q_INVOKABLE void closeProject();

signals:
    void currentPageChanged();
    void currentProjectChanged();
    void pythonBackendReadyChanged();

private:
    QString m_currentPage = "project";
    QString m_currentProject;
    bool m_pythonBackendReady = false;
};

#endif // APPCONTROLLER_H
