#ifndef EXPORTSERVICE_H
#define EXPORTSERVICE_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QJsonObject>

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

    /**
     * @brief 冷启动自检：修正残留的 running / verifying 状态导出产物
     *
     * 应用启动时调用，将上次异常退出遗留的 running / verifying 状态
     * 导出产物修正为 failed，防止"幽灵导出"任务永远无法完成。
     *
     * @return 修正的记录数
     */
    int reconcileStaleExports();

    /**
     * @brief 导出测试报告为JSON文件
     *
     * 将模型版本的测试指标、混淆矩阵等数据导出为JSON报告文件，
     * 保存到项目的exports目录下。
     *
     * @param projectId 项目ID
     * @param modelVersionId 模型版本ID
     * @param reportType 报告类型（训练报告/评估报告/对比报告）
     * @param reportDataJson 报告数据JSON字符串
     * @return 报告文件路径，失败返回空字符串
     */
    Q_INVOKABLE QString exportReport(const QString &projectId,
                                      const QString &modelVersionId,
                                      const QString &reportType,
                                      const QString &reportDataJson);

signals:
    /**
     * @brief Emitted when an export artifact's status changes.
     * @param artifactId The artifact ID.
     * @param status The new status.
     */
    void exportStatusChanged(const QString &artifactId, const QString &status);

private slots:
    void handleIpcResponse(const QJsonObject &response);

private:
    bool ensureStatusColumn();

    IpcClient *m_ipcClient = nullptr;
};

#endif // EXPORTSERVICE_H
