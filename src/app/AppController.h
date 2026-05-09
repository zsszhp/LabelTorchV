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

public:
    explicit AppController(QObject *parent = nullptr);

    QString currentPage() const { return m_currentPage; }
    void setCurrentPage(const QString &page);

    QString currentProjectId() const { return m_currentProjectId; }
    QString currentProjectName() const { return m_currentProjectName; }
    bool projectOpen() const { return !m_currentProjectId.isEmpty(); }

    bool pythonBackendReady() const { return m_pythonBackendReady; }

    Q_INVOKABLE void openProject(const QString &projectId, const QString &projectName);
    Q_INVOKABLE void closeProject();

signals:
    void currentPageChanged();
    void currentProjectIdChanged();
    void currentProjectNameChanged();
    void pythonBackendReadyChanged();

private:
    QString m_currentPage = "project";
    QString m_currentProjectId;
    QString m_currentProjectName;
    bool m_pythonBackendReady = false;
};

#endif // APPCONTROLLER_H
