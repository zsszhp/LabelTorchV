#ifndef PROJECTSERVICE_H
#define PROJECTSERVICE_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class Database;
class TaxonomyService;

class ProjectService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentProjectId READ currentProjectId NOTIFY currentProjectChanged)

public:
    explicit ProjectService(QObject *parent = nullptr);

    QString currentProjectId() const { return m_currentProjectId; }

    void setTaxonomyService(TaxonomyService *service) { m_taxonomyService = service; }

    Q_INVOKABLE QString createProject(const QString &name, const QString &rootPath,
                                      const QString &taskType = QStringLiteral("detect"));
    Q_INVOKABLE QString importProject(const QString &rootPath);
    Q_INVOKABLE QVariantList listProjects();
    /**
     * @brief 预览删除项目的影响范围（供 UI 确认框展示）。
     * @param projectId 项目 ID
     * @return QVariantMap: { datasetCount, sampleCount, snapshotCount, trainingRunCount,
     *                       modelCount, taxonomyCount, rootPath, projectName }
     */
    Q_INVOKABLE QVariantMap previewProjectDeletion(const QString &projectId);
    /**
     * @brief 删除项目：事务内级联删除所有子表记录，并清理项目磁盘目录。
     * 磁盘清理仅删除已注册的项目根目录树，拒绝空路径/盘符根等危险路径。
     * @param projectId 项目 ID
     * @return true=删除成功
     */
    Q_INVOKABLE bool deleteProject(const QString &projectId);
    Q_INVOKABLE bool openProject(const QString &projectId);
    /// 项目根路径是否在磁盘上存在（换机/移盘后用于 UI 提示）
    Q_INVOKABLE bool pathExists(const QString &path) const;
    Q_INVOKABLE void closeProject();
    Q_INVOKABLE QVariantMap getCurrentProject() const;
    Q_INVOKABLE bool saveProjectConfig(const QString &projectId);

    /**
     * @brief 校验项目路径是否包含中文或空格
     * @param path 待校验的路径
     * @return QVariantMap: { "valid": bool, "warnings": [string], "errors": [string] }
     *   - valid: true=路径可用, false=路径不可用
     *   - warnings: 警告列表（中文/空格等兼容性风险）
     *   - errors: 错误列表（路径为空等致命问题）
     */
    Q_INVOKABLE QVariantMap validateProjectPath(const QString &path);

    /**
     * @brief Get the current task type for a project.
     * Returns: "detect", "obb", "classify", or "anomaly".
     * Defaults to "detect" if not set.
     */
    Q_INVOKABLE QString getTaskType(const QString &projectId);

    /**
     * @brief Set the task type for a project.
     * @param taskType One of: "detect", "obb", "classify", "anomaly"
     */
    Q_INVOKABLE bool setTaskType(const QString &projectId, const QString &taskType);

    /**
     * @brief 获取项目缩略图缓存目录（cache/thumbnails）。
     * @param projectId 项目 ID；空串时返回空
     * @return 绝对路径（已确保目录存在），失败返回空串
     */
    Q_INVOKABLE QString thumbnailCacheDir(const QString &projectId);

signals:
    void currentProjectChanged();
    void taskTypeChanged(const QString &projectId, const QString &taskType);

private:
    QString m_currentProjectId;
    TaxonomyService *m_taxonomyService = nullptr;

    bool ensureTaskTypeColumn();
};

#endif
