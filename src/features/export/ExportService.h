#ifndef EXPORTSERVICE_H
#define EXPORTSERVICE_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class IpcClient;

/**
 * @brief Export service for model artifact management.
 *
 * Manages model export lifecycle: creates export_artifacts records,
 * dispatches export.run and artifact.verify via IpcClient, tracks
 * export status through the state machine:
 *   pending -> running -> verifying -> succeeded / failed
 */
class ExportService : public QObject
{
    Q_OBJECT

public:
    explicit ExportService(QObject *parent = nullptr);

    /**
     * @brief Inject IPC client dependency.
     */
    void setIpcClient(IpcClient *client);

    /**
     * @brief Start a model export.
     *
     * Creates an export_artifacts record with status "pending",
     * then sends export.run via IpcClient.
     *
     * @param modelVersionId The model version to export.
     * @param format Export format: "pt", "onnx", "tflite", or "engine".
     * @param optionsJson Export options as JSON string.
     * @return Artifact ID on success, empty string on failure.
     */
    Q_INVOKABLE QString exportModel(const QString &modelVersionId,
                                     const QString &format,
                                     const QString &optionsJson);

    /**
     * @brief Get the status/details of an export artifact.
     * @param artifactId The artifact ID.
     * @return QVariantMap with artifact fields, or empty on not found.
     */
    Q_INVOKABLE QVariantMap getExportStatus(const QString &artifactId);

    /**
     * @brief List exports for a model version.
     * @param modelVersionId The model version ID.
     * @return QVariantList of QVariantMap with artifact fields.
     */
    Q_INVOKABLE QVariantList listExports(const QString &modelVersionId);

    /**
     * @brief Verify an exported artifact.
     *
     * Transitions status from "succeeded" to "verifying",
     * then sends artifact.verify via IpcClient.
     *
     * @param artifactId The artifact ID.
     * @return true on success, false on failure.
     */
    Q_INVOKABLE bool verifyExport(const QString &artifactId);

    /**
     * @brief Update the status of an export artifact.
     * @param artifactId The artifact ID.
     * @param status The new status string.
     * @return true on success, false on failure.
     */
    Q_INVOKABLE bool updateExportStatus(const QString &artifactId, const QString &status);

signals:
    /**
     * @brief Emitted when an export artifact's status changes.
     * @param artifactId The artifact ID.
     * @param status The new status.
     */
    void exportStatusChanged(const QString &artifactId, const QString &status);

private:
    IpcClient *m_ipcClient = nullptr;
};

#endif // EXPORTSERVICE_H
