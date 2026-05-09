#ifndef MODELREGISTRY_H
#define MODELREGISTRY_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class ModelRegistry : public QObject
{
    Q_OBJECT

public:
    explicit ModelRegistry(QObject *parent = nullptr);

    /**
     * @brief Register a model version after training completes.
     * Auto-generates UUID. Return version ID.
     */
    Q_INVOKABLE QString registerModelVersion(const QString &runId,
                                               const QString &bestWeightPath,
                                               const QString &lastWeightPath,
                                               const QString &metricsJson);

    /**
     * @brief List all model versions for a project (joins with training_runs).
     */
    Q_INVOKABLE QVariantList listModelVersions(const QString &projectId);

    /**
     * @brief Get details of a specific version.
     */
    Q_INVOKABLE QVariantMap getModelVersion(const QString &versionId);

    /**
     * @brief Delete a version (only if no exports reference it).
     */
    Q_INVOKABLE bool deleteModelVersion(const QString &versionId);

    /**
     * @brief Set parent model version for lineage tracking.
     */
    Q_INVOKABLE bool setParentVersion(const QString &versionId, const QString &parentVersionId);

    /**
     * @brief Add tag (baseline/best-so-far/production-candidate).
     * Tags stored in metrics_snapshot_json as a "tags" array field.
     */
    Q_INVOKABLE bool setTag(const QString &versionId, const QString &tag);

    /**
     * @brief Remove a tag from the version.
     */
    Q_INVOKABLE bool removeTag(const QString &versionId, const QString &tag);
};

#endif // MODELREGISTRY_H
