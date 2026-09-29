#include "ExportService.h"
#include "Database.h"
#include "ipc/IpcClient.h"
#include "ipc/IpcProtocol.h"
#include "filesystem/ProjectFs.h"
#include "utils/AuditLog.h"
#include "utils/Log.h"
#include "utils/Id.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QDateTime>

ExportService::ExportService(QObject *parent) : QObject(parent)
{
    ltTrace(LT_LOG_EXPORT()) << "parent=" << parent;
    ensureStatusColumn();
}

bool ExportService::ensureStatusColumn()
{
    auto db = Database::instance().database();
    if (!db.isOpen()) return false;
    QSqlQuery query(db);
    query.exec("SELECT status FROM export_artifacts LIMIT 0");
    if (query.lastError().isValid()) {
        QSqlQuery alterQuery(db);
        if (!alterQuery.exec("ALTER TABLE export_artifacts ADD COLUMN status TEXT DEFAULT 'pending'")) {
            ltError(LT_LOG_EXPORT()) << "Failed to add status column:" << alterQuery.lastError().text();
            return false;
        }
        ltInfo(LT_LOG_EXPORT()) << "Added status column to export_artifacts table";
    }
    return true;
}

void ExportService::setIpcClient(IpcClient *client)
{
    ltTrace(LT_LOG_EXPORT()) << "client=" << client;
    m_ipcClient = client;

    if (m_ipcClient) {
        connect(m_ipcClient, &IpcClient::responseReceived,
                this, &ExportService::handleIpcResponse);
    }
}

QString ExportService::exportModel(const QString &modelVersionId,
                                    const QString &format,
                                    const QString &optionsJson)
{
    ltTrace(LT_LOG_EXPORT()) << "modelVersionId=" << modelVersionId
                             << "format=" << format
                             << "optionsJson=" << optionsJson;

    auto db = Database::instance().database();
    if (!db.isOpen()) return {};

    // 查询 model_version，关联获取 task_type 和 project_root
    // 支持训练模型（通过 training_runs → projects 关联）和导入模型（通过 project_id → projects 关联）
    QSqlQuery checkVersion(db);
    checkVersion.prepare(
        "SELECT m.best_weight_path, "
        "COALESCE(p.task_type, p2.task_type, 'detect') AS task_type, "
        "COALESCE(p.root_path, p2.root_path) AS root_path "
        "FROM model_versions m "
        "LEFT JOIN training_runs r ON m.run_id = r.id "
        "LEFT JOIN projects p ON r.project_id = p.id "
        "LEFT JOIN projects p2 ON m.project_id = p2.id "
        "WHERE m.id = ?"
    );
    checkVersion.addBindValue(modelVersionId);
    if (!checkVersion.exec() || !checkVersion.next()) {
        ltError(LT_LOG_EXPORT()) << "Model version not found:" << modelVersionId;
        return {};
    }

    QString bestWeightPath = checkVersion.value(0).toString();
    QString taskType = checkVersion.value(1).toString();
    QString projectRoot = checkVersion.value(2).toString();

    // 默认判定适配器类型
    QString adapter = (taskType == QStringLiteral("anomaly")) ? QStringLiteral("anomalib") : QStringLiteral("ultralytics");

    if (format != "pt" && format != "onnx" && format != "tflite" && format != "engine") {
        ltWarning(LT_LOG_EXPORT()) << "Invalid export format:" << format;
        return {};
    }

    QString validatedOptionsJson = optionsJson;
    if (!optionsJson.isEmpty()) {
        QJsonParseError parseError;
        QJsonDocument::fromJson(optionsJson.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            ltWarning(LT_LOG_EXPORT()) << "Invalid options JSON:" << parseError.errorString();
            return {};
        }
    } else {
        validatedOptionsJson = "{}";
    }

    QString artifactId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    // 导出文件放在项目的 exports/ 目录下，避免与训练权重目录混淆
    QDir exportsDir(projectRoot + QStringLiteral("/exports"));
    if (!exportsDir.exists()) {
        exportsDir.mkpath(QStringLiteral("."));
    }
    QString outputPath = exportsDir.absoluteFilePath(
        QStringLiteral("export_%1.%2").arg(artifactId.left(8), format));

    QJsonObject optionsObj = QJsonDocument::fromJson(validatedOptionsJson.toUtf8()).object();
    // options_snapshot_json 只存储导出选项配置，不存储运行时状态
    QString snapshotJson = QString::fromUtf8(
        QJsonDocument(optionsObj).toJson(QJsonDocument::Compact));

    QSqlQuery query(db);
    query.prepare(
        "INSERT INTO export_artifacts "
        "(id, model_version_id, format, options_snapshot_json, output_path, validation_result, status) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)"
    );
    query.addBindValue(artifactId);
    query.addBindValue(modelVersionId);
    query.addBindValue(format);
    query.addBindValue(snapshotJson);
    query.addBindValue(outputPath);
    query.addBindValue(""); 
    query.addBindValue(QStringLiteral("pending")); 

    if (!query.exec()) {
        ltError(LT_LOG_EXPORT()) << "Failed to create export artifact:" << query.lastError().text();
        return {};
    }

    // 审计：导出任务创建落库留痕
    {
        QVariantMap auditPayload;
        auditPayload[QStringLiteral("modelVersionId")] = modelVersionId;
        auditPayload[QStringLiteral("format")] = format;
        auditPayload[QStringLiteral("outputPath")] = outputPath;
        auditPayload[QStringLiteral("adapter")] = adapter;
        AuditLog::record(QStringLiteral("export"), artifactId,
                         QStringLiteral("export_start"), auditPayload);
    }

    // 发送 IPC 导出指令
    // A12+M7：检查 IPC 连接状态，断连时标记为 failed 并清理
    if (m_ipcClient && m_ipcClient->connected()) {
        QJsonObject payload;
        payload["artifact_id"] = artifactId;
        payload["model_version_id"] = modelVersionId;
        payload["format"] = format;
        payload["weight_path"] = bestWeightPath;
        payload["output_path"] = outputPath;
        payload["adapter"] = adapter; // 传入正确适配器
        payload["options"] = QJsonDocument::fromJson(validatedOptionsJson.toUtf8()).object();
        QString reqId = m_ipcClient->sendRequest(IpcProtocol::CMD_EXPORT_RUN, payload);
        if (reqId.isEmpty()) {
            // IPC 发送失败，标记为 failed
            ltError(LT_LOG_EXPORT()) << "IPC send failed for export artifact:" << artifactId;
            updateExportStatus(artifactId, QStringLiteral("failed"));
            emit exportStatusChanged(artifactId, QStringLiteral("failed"));
            return artifactId;
        }
    } else {
        // M7：IPC 未连接，标记为 failed
        ltError(LT_LOG_EXPORT()) << "IPC not connected, cannot export artifact:" << artifactId;
        updateExportStatus(artifactId, QStringLiteral("failed"));
        emit exportStatusChanged(artifactId, QStringLiteral("failed"));
        return artifactId;
    }

    updateExportStatus(artifactId, "running");
    ltInfo(LT_LOG_EXPORT()) << "Created export artifact:" << artifactId
                            << "format:" << format << "adapter:" << adapter;
    emit exportStatusChanged(artifactId, "pending");
    return artifactId;
}

QVariantMap ExportService::getExportStatus(const QString &artifactId)
{
    ltTrace(LT_LOG_EXPORT()) << "artifactId=" << artifactId;

    auto db = Database::instance().database();
    QVariantMap result;

    QSqlQuery query(db);
    query.prepare(
        "SELECT id, model_version_id, format, options_snapshot_json, "
        "output_path, validation_result, created_at, status "
        "FROM export_artifacts WHERE id = ?"
    );
    query.addBindValue(artifactId);

    if (!query.exec() || !query.next()) return result;

    result["id"] = query.value(0).toString();
    result["modelVersionId"] = query.value(1).toString();
    result["format"] = query.value(2).toString();
    result["optionsJson"] = query.value(3).toString();
    result["outputPath"] = query.value(4).toString();
    result["validationResult"] = query.value(5).toString();
    result["createdAt"] = query.value(6).toString();
    result["status"] = query.value(7).toString().isEmpty() ? QStringLiteral("pending") : query.value(7).toString();

    return result;
}

QVariantList ExportService::listExports(const QString &modelVersionId)
{
    ltTrace(LT_LOG_EXPORT()) << "modelVersionId=" << modelVersionId;

    auto db = Database::instance().database();
    QVariantList result;

    QSqlQuery query(db);
    query.prepare(
        "SELECT id, model_version_id, format, options_snapshot_json, "
        "output_path, validation_result, created_at, status "
        "FROM export_artifacts WHERE model_version_id = ? "
        "ORDER BY created_at DESC"
    );
    query.addBindValue(modelVersionId);

    if (!query.exec()) {
        ltError(LT_LOG_EXPORT()) << "Failed to list exports:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        QVariantMap artifact;
        artifact["id"] = query.value(0).toString();
        artifact["modelVersionId"] = query.value(1).toString();
        artifact["format"] = query.value(2).toString();
        artifact["optionsJson"] = query.value(3).toString();
        artifact["outputPath"] = query.value(4).toString();
        artifact["validationResult"] = query.value(5).toString();
        artifact["createdAt"] = query.value(6).toString();
        artifact["status"] = query.value(7).toString().isEmpty() ? QStringLiteral("pending") : query.value(7).toString();

        result.append(artifact);
    }

    ltDebug(LT_LOG_EXPORT()) << "Listed" << result.size() << "exports for model version:" << modelVersionId;
    return result;
}

bool ExportService::verifyExport(const QString &artifactId)
{
    ltTrace(LT_LOG_EXPORT()) << "artifactId=" << artifactId;

    auto db = Database::instance().database();
    if (!db.isOpen()) return false;

    // 从 status 列读取当前状态（不再从 options_snapshot_json 读取）
    QSqlQuery getQuery(db);
    getQuery.prepare("SELECT status FROM export_artifacts WHERE id = ?");
    getQuery.addBindValue(artifactId);
    if (!getQuery.exec() || !getQuery.next()) return false;

    QString currentStatus = getQuery.value(0).toString();
    if (currentStatus.isEmpty()) currentStatus = QStringLiteral("pending");

    // 仅允许从 succeeded 或 verifying 状态发起验证
    // verifying: 自动验证流程中；succeeded: 手动重新验证
    if (currentStatus != "succeeded" && currentStatus != "verifying") {
        ltWarning(LT_LOG_EXPORT()) << "Cannot verify export in status:" << currentStatus;
        return false;
    }

    // 如果已经是verifying状态（自动验证中），避免重复发送IPC请求
    if (currentStatus == "verifying") {
        ltInfo(LT_LOG_EXPORT()) << "Verification already in progress for artifact:" << artifactId;
        return true;
    }

    // Transition to verifying
    if (!updateExportStatus(artifactId, "verifying")) return false;

    // Send artifact.verify via IpcClient if available
    if (m_ipcClient) {
        // Get artifact details for the payload
        QVariantMap details = getExportStatus(artifactId);
        QJsonObject payload;
        payload["artifact_id"] = artifactId;
        payload["model_version_id"] = details["modelVersionId"].toString();
        payload["format"] = details["format"].toString();
        payload["output_path"] = details["outputPath"].toString();
        m_ipcClient->sendRequest(IpcProtocol::CMD_ARTIFACT_VERIFY, payload);
    }

    ltInfo(LT_LOG_EXPORT()) << "Verifying export artifact:" << artifactId;
    return true;
}

bool ExportService::updateExportStatus(const QString &artifactId, const QString &status)
{
    ltTrace(LT_LOG_EXPORT()) << "artifactId=" << artifactId << "status=" << status;

    auto db = Database::instance().database();
    if (!db.isOpen()) return false;

    // 直接更新 status 列，不再冗余写入 options_snapshot_json
    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE export_artifacts SET status = ? WHERE id = ?");
    updateQuery.addBindValue(status);
    updateQuery.addBindValue(artifactId);

    if (!updateQuery.exec()) {
        ltError(LT_LOG_EXPORT()) << "Failed to update export status:" << updateQuery.lastError().text();
        return false;
    }

    // 检查是否实际更新了行（不存在的 artifactId）
    if (updateQuery.numRowsAffected() == 0) {
        ltWarning(LT_LOG_EXPORT()) << "No rows affected, artifact not found:" << artifactId;
        return false;
    }

    ltInfo(LT_LOG_EXPORT()) << "Export status updated:" << artifactId << "->" << status;
    emit exportStatusChanged(artifactId, status);
    return true;
}

void ExportService::handleIpcResponse(const QJsonObject &response)
{
    QString command = response[QStringLiteral("command")].toString();
    bool success = response[QStringLiteral("success")].toBool();
    QJsonObject result = response[QStringLiteral("result")].toObject();

    if (command == IpcProtocol::CMD_EXPORT_RUN) {
        // A7：移除 request_id 回退逻辑，artifact_id 为空时记录错误并返回
        QString artifactId = result[QStringLiteral("artifact_id")].toString();
        if (artifactId.isEmpty()) {
            ltError(LT_LOG_EXPORT()) << "Export response missing artifact_id, request_id="
                                     << response[QStringLiteral("request_id")].toString();
            return;
        }
        QString status = result[QStringLiteral("status")].toString();

        if (success && status == QStringLiteral("succeeded")) {
            QString exportPath = result[QStringLiteral("export_path")].toString();

            // A12：检查 exec 返回值
            auto db = Database::instance().database();
            QSqlQuery updateQuery(db);
            updateQuery.prepare("UPDATE export_artifacts SET output_path = ? WHERE id = ?");
            updateQuery.addBindValue(exportPath);
            updateQuery.addBindValue(artifactId);
            if (!updateQuery.exec()) {
                ltError(LT_LOG_EXPORT()) << "Failed to update output_path for artifact:"
                                         << artifactId << updateQuery.lastError().text();
            }

            // P1-20：导出成功后生成交付目录（模型 + 配置 + 脚本 + 评估摘要）
            QString deliveryDir = generateDeliveryPackage(artifactId);
            if (!deliveryDir.isEmpty()) {
                ltInfo(LT_LOG_EXPORT()) << "Delivery package generated at:" << deliveryDir;
            } else {
                ltWarning(LT_LOG_EXPORT()) << "Failed to generate delivery package for artifact:" << artifactId;
            }

            // 导出成功后自动进入验证阶段，而非直接标记为succeeded
            updateExportStatus(artifactId, QStringLiteral("verifying"));
            ltInfo(LT_LOG_EXPORT()) << "Export succeeded, auto-starting verification for artifact:" << artifactId << "path:" << exportPath;

            // 自动发送 artifact.verify IPC请求
            if (m_ipcClient && m_ipcClient->connected()) {
                QVariantMap details = getExportStatus(artifactId);
                QJsonObject verifyPayload;
                verifyPayload["artifact_id"] = artifactId;
                verifyPayload["model_version_id"] = details["modelVersionId"].toString();
                verifyPayload["format"] = details["format"].toString();
                verifyPayload["output_path"] = details["outputPath"].toString();
                m_ipcClient->sendRequest(IpcProtocol::CMD_ARTIFACT_VERIFY, verifyPayload);
                ltInfo(LT_LOG_EXPORT()) << "Auto-verify request sent for artifact:" << artifactId;
            }
        } else {
            updateExportStatus(artifactId, QStringLiteral("failed"));
            QString error = result.contains("error") ? result["error"].toString() : 
                            response[QStringLiteral("error")].toObject()[QStringLiteral("message")].toString();
            ltError(LT_LOG_EXPORT()) << "Export failed for artifact:" << artifactId << "error:" << error;
        }
    } else if (command == IpcProtocol::CMD_ARTIFACT_VERIFY) {
        QString artifactId = result[QStringLiteral("artifact_id")].toString();
        // A7：artifact_id 为空时记录错误并返回
        if (artifactId.isEmpty()) {
            ltError(LT_LOG_EXPORT()) << "Verify response missing artifact_id, request_id="
                                     << response[QStringLiteral("request_id")].toString();
            return;
        }

        // P0-3：根据 validation_result.valid===true 且 IPC success 才走成功分支
        // 三种情况：
        //   1. valid===true  → 验证通过，status=succeeded
        //   2. valid===false → 验证失败，status=failed
        //   3. valid===null  → 无验证器（tflite/engine），status=succeeded 但 validation_result
        //                     标记为未验证，UI 显示「未验证」，不得标记为已验证成功
        QJsonValue validVal = result[QStringLiteral("valid")];
        bool ipcSuccess = success;
        bool verifiedOk = ipcSuccess && validVal.isBool() && validVal.toBool();
        bool noVerifier = validVal.isNull() && !result[QStringLiteral("verified")].toBool(true);

        // 将验证结果完整存入 validation_result（含 valid/verified/reason/error）
        QString validationResult = QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact));

        auto db = Database::instance().database();
        QSqlQuery updateQuery(db);
        updateQuery.prepare("UPDATE export_artifacts SET validation_result = ? WHERE id = ?");
        updateQuery.addBindValue(validationResult);
        updateQuery.addBindValue(artifactId);
        // A12：检查 exec 返回值
        if (!updateQuery.exec()) {
            ltError(LT_LOG_EXPORT()) << "Failed to update validation_result for artifact:"
                                     << artifactId << updateQuery.lastError().text();
        }

        if (verifiedOk) {
            // 验证通过：标记成功
            updateExportStatus(artifactId, QStringLiteral("succeeded"));
            ltInfo(LT_LOG_EXPORT()) << "Validation succeeded for artifact:" << artifactId;
        } else if (noVerifier) {
            // 无验证器格式：导出流程走完但未验证，status 置为 succeeded（文件已产出）
            // validation_result.valid=null + verified=false，UI 据此显示「未验证」
            // 关键：绝不写入 valid:true
            updateExportStatus(artifactId, QStringLiteral("succeeded"));
            ltWarning(LT_LOG_EXPORT()) << "Artifact exported but NOT verified (no verifier for format):"
                                       << artifactId;
        } else {
            // 验证失败或 IPC 层失败：标记 failed
            if (!result.contains(QStringLiteral("error"))) {
                // IPC 层失败但 result 中无 error 时，从 response.error 提取
                QJsonObject errorResult = result;
                errorResult[QStringLiteral("valid")] = false;
                errorResult[QStringLiteral("verified")] = false;
                errorResult[QStringLiteral("error")] =
                    response[QStringLiteral("error")].toObject()[QStringLiteral("message")].toString();
                QString errorJson = QString::fromUtf8(QJsonDocument(errorResult).toJson(QJsonDocument::Compact));
                QSqlQuery errUpdate(db);
                errUpdate.prepare("UPDATE export_artifacts SET validation_result = ? WHERE id = ?");
                errUpdate.addBindValue(errorJson);
                errUpdate.addBindValue(artifactId);
                errUpdate.exec();
            }
            updateExportStatus(artifactId, QStringLiteral("failed"));
            // error 字段可能是字符串或结构化对象（{"code":..,"message":..}），两者都提取可读文本
            QJsonValue errVal = result[QStringLiteral("error")];
            QString errMsg = errVal.isString() ? errVal.toString()
                            : errVal.isObject() ? errVal.toObject()[QStringLiteral("message")].toString()
                            : QStringLiteral("验证失败");
            ltError(LT_LOG_EXPORT()) << "Validation failed for artifact:" << artifactId
                                     << "error:" << errMsg;
        }
    }
}

int ExportService::reconcileStaleExports()
{
    auto db = Database::instance().database();
    if (!db.isOpen()) return 0;

    int fixed = 0;

    // 查询需要修正的记录
    QSqlQuery query(db);
    query.prepare("SELECT id FROM export_artifacts WHERE status IN ('running', 'verifying')");
    if (!query.exec()) {
        ltError(LT_LOG_EXPORT()) << "reconcileStaleExports: query failed:" << query.lastError().text();
        return 0;
    }

    QStringList staleArtifactIds;
    while (query.next()) {
        staleArtifactIds.append(query.value(0).toString());
    }

    if (staleArtifactIds.isEmpty()) return 0;

    // 批量更新状态为 failed（导出中断无法恢复，标记为失败）
    QSqlQuery fixQuery(db);
    fixQuery.prepare("UPDATE export_artifacts SET status = 'failed' "
                     "WHERE status IN ('running', 'verifying')");
    if (fixQuery.exec()) {
        fixed = fixQuery.numRowsAffected();
    }

    // 逐条发射状态变更信号，通知 UI 层刷新
    for (const QString &artifactId : staleArtifactIds) {
        emit exportStatusChanged(artifactId, QStringLiteral("failed"));
    }

    if (fixed > 0) {
        ltWarning(LT_LOG_EXPORT()) << "Cold boot: reconciled" << fixed
                                   << "orphaned running/verifying exports -> failed";
    }

    return fixed;
}

QString ExportService::exportReport(const QString &projectId,
                                     const QString &modelVersionId,
                                     const QString &reportType,
                                     const QString &reportDataJson)
{
    ltInfo(LT_LOG_EXPORT()) << "Exporting report for project:" << projectId
                            << "modelVersion:" << modelVersionId
                            << "type:" << reportType;

    // 获取项目根路径
    auto db = Database::instance().database();
    if (!db.isOpen()) {
        ltWarning(LT_LOG_EXPORT()) << "Database not open, cannot export report";
        return {};
    }

    QSqlQuery query(db);
    query.prepare("SELECT root_path FROM projects WHERE id = ?");
    query.addBindValue(projectId);
    if (!query.exec() || !query.next()) {
        ltWarning(LT_LOG_EXPORT()) << "Project not found:" << projectId;
        return {};
    }

    QString rootPath = query.value(0).toString();
    if (rootPath.isEmpty()) {
        ltWarning(LT_LOG_EXPORT()) << "Project root path is empty";
        return {};
    }

    // 确保exports目录存在
    QString exportsDir = ProjectFs::exportsDir(rootPath);
    QDir dir(exportsDir);
    if (!dir.exists() && !dir.mkpath(".")) {
        ltWarning(LT_LOG_EXPORT()) << "Failed to create exports directory:" << exportsDir;
        return {};
    }

    // 构建报告文件名
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString versionSuffix = modelVersionId.left(8);
    QString typeSuffix;
    if (reportType == "训练报告") typeSuffix = "training";
    else if (reportType == "评估报告") typeSuffix = "evaluation";
    else if (reportType == "对比报告") typeSuffix = "comparison";
    else typeSuffix = "report";

    QString fileName = QString("report_%1_%2_%3.json").arg(typeSuffix, versionSuffix, timestamp);
    QString filePath = dir.filePath(fileName);

    // 原子写入：先写临时文件，再重命名
    QString tmpPath = filePath + ".tmp";
    QFile tmpFile(tmpPath);
    if (!tmpFile.open(QIODevice::WriteOnly)) {
        ltWarning(LT_LOG_EXPORT()) << "Failed to create temp report file:" << tmpPath;
        return {};
    }

    // 构建报告内容
    QJsonObject reportObj = QJsonDocument::fromJson(reportDataJson.toUtf8()).object();
    reportObj["reportType"] = reportType;
    reportObj["projectId"] = projectId;
    reportObj["modelVersionId"] = modelVersionId;
    reportObj["generatedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    QJsonDocument doc(reportObj);
    tmpFile.write(doc.toJson());
    tmpFile.close();

    // 重命名临时文件为最终文件
    QFile finalFile(filePath);
    if (finalFile.exists()) {
        finalFile.remove();
    }
    if (!QFile::rename(tmpPath, filePath)) {
        ltWarning(LT_LOG_EXPORT()) << "Failed to rename temp report file to:" << filePath;
        QFile::remove(tmpPath);
        return {};
    }

    ltInfo(LT_LOG_EXPORT()) << "Report exported to:" << filePath;
    return filePath;
}

// ============================================================================
// P1-20：导出交付目录 —— onnx/pt + classes.yaml + thresholds.json + 脚本 + 评估摘要
// ============================================================================

bool ExportService::writeFileAtomically(const QString &targetPath, const QString &content)
{
    QString tmpPath = targetPath + QStringLiteral(".tmp");
    QFile tmpFile(tmpPath);
    if (!tmpFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        ltError(LT_LOG_EXPORT()) << "Failed to create temp file:" << tmpPath;
        return false;
    }
    QTextStream out(&tmpFile);
    out << content;
    out.flush();
    tmpFile.close();

    QFile::remove(targetPath);
    if (!tmpFile.rename(targetPath)) {
        ltError(LT_LOG_EXPORT()) << "Failed to rename temp file to:" << targetPath;
        QFile::remove(tmpPath);
        return false;
    }
    return true;
}

bool ExportService::writeClassesYaml(const QString &deliveryDir, const QString &modelVersionId)
{
    auto db = Database::instance().database();
    if (!db.isOpen()) return false;

    // 沿血缘取项目：训练模型 run_id→training_runs.project_id；导入模型 project_id
    QSqlQuery query(db);
    query.prepare(
        "SELECT COALESCE(p.id, p2.id) AS project_id "
        "FROM model_versions m "
        "LEFT JOIN training_runs r ON m.run_id = r.id "
        "LEFT JOIN projects p ON r.project_id = p.id "
        "LEFT JOIN projects p2 ON m.project_id = p2.id "
        "WHERE m.id = ?"
    );
    query.addBindValue(modelVersionId);
    QString projectId;
    if (query.exec() && query.next()) {
        projectId = query.value(0).toString();
    }

    QStringList classNames;
    if (!projectId.isEmpty()) {
        QSqlQuery taxQuery(db);
        taxQuery.prepare("SELECT class_definitions_json FROM taxonomies "
                         "WHERE project_id = ? ORDER BY version DESC LIMIT 1");
        taxQuery.addBindValue(projectId);
        if (taxQuery.exec() && taxQuery.next()) {
            QJsonDocument doc = QJsonDocument::fromJson(taxQuery.value(0).toString().toUtf8());
            for (const auto &val : doc.array()) {
                if (val.isString()) classNames.append(val.toString());
            }
        }
    }
    if (classNames.isEmpty()) {
        classNames.append(QStringLiteral("defect"));
    }

    QString content;
    QTextStream stream(&content);
    stream << "# 类别清单（导出交付物）\n";
    stream << "nc: " << classNames.size() << "\n";
    stream << "names:\n";
    for (int i = 0; i < classNames.size(); ++i) {
        stream << "  " << i << ": " << classNames[i] << "\n";
    }

    return writeFileAtomically(deliveryDir + QStringLiteral("/classes.yaml"), content);
}

bool ExportService::writeThresholdsJson(const QString &deliveryDir, const QString &modelVersionId)
{
    auto db = Database::instance().database();
    if (!db.isOpen()) return false;

    // 取该模型版本最近一次测试任务的阈值推荐
    QSqlQuery query(db);
    query.prepare("SELECT metrics_json FROM testing_runs "
                  "WHERE model_version_id = ? ORDER BY created_at DESC LIMIT 1");
    query.addBindValue(modelVersionId);

    QJsonObject thresholds;
    thresholds[QStringLiteral("recommended_conf")] = 0.25;
    thresholds[QStringLiteral("method")] = QStringLiteral("max_f1");
    thresholds[QStringLiteral("source")] = QStringLiteral("default");

    if (query.exec() && query.next()) {
        QJsonObject metrics = QJsonDocument::fromJson(query.value(0).toString().toUtf8()).object();
        QJsonObject rec = metrics.value(QStringLiteral("threshold_recommendation")).toObject();
        if (!rec.isEmpty()) {
            thresholds = rec;
            thresholds[QStringLiteral("source")] = QStringLiteral("testing_runs");
        }
        QJsonObject goNoGo = metrics.value(QStringLiteral("go_no_go")).toObject();
        if (!goNoGo.isEmpty()) {
            thresholds[QStringLiteral("go_no_go")] = goNoGo;
        }
    }
    thresholds[QStringLiteral("generatedAt")] = QDateTime::currentDateTime().toString(Qt::ISODate);

    QString json = QString::fromUtf8(QJsonDocument(thresholds).toJson(QJsonDocument::Indented));
    return writeFileAtomically(deliveryDir + QStringLiteral("/thresholds.json"), json);
}

bool ExportService::writeEvalSummaryJson(const QString &deliveryDir, const QString &modelVersionId)
{
    auto db = Database::instance().database();
    if (!db.isOpen()) return false;

    QJsonObject summary;
    summary[QStringLiteral("modelVersionId")] = modelVersionId;
    summary[QStringLiteral("generatedAt")] = QDateTime::currentDateTime().toString(Qt::ISODate);

    QSqlQuery query(db);
    query.prepare("SELECT id, metrics_json, created_at FROM testing_runs "
                  "WHERE model_version_id = ? ORDER BY created_at DESC LIMIT 1");
    query.addBindValue(modelVersionId);
    if (query.exec() && query.next()) {
        summary[QStringLiteral("testingRunId")] = query.value(0).toString();
        summary[QStringLiteral("evaluatedAt")] = query.value(2).toString();
        QJsonObject metrics = QJsonDocument::fromJson(query.value(1).toString().toUtf8()).object();
        // 摘要只保留关键指标，避免嵌套曲线撑爆交付文件
        QJsonObject flat;
        for (const QString &key : {QStringLiteral("mAP50"), QStringLiteral("mAP50-95"),
                                   QStringLiteral("precision"), QStringLiteral("recall"),
                                   QStringLiteral("f1"), QStringLiteral("auroc"),
                                   QStringLiteral("image_auroc"), QStringLiteral("pixel_auroc")}) {
            if (metrics.contains(key)) flat[key] = metrics.value(key);
        }
        if (metrics.contains(QStringLiteral("per_class"))) {
            flat[QStringLiteral("per_class")] = metrics.value(QStringLiteral("per_class"));
        }
        if (metrics.contains(QStringLiteral("go_no_go"))) {
            flat[QStringLiteral("go_no_go")] = metrics.value(QStringLiteral("go_no_go"));
        }
        if (metrics.contains(QStringLiteral("threshold_recommendation"))) {
            flat[QStringLiteral("threshold_recommendation")] = metrics.value(QStringLiteral("threshold_recommendation"));
        }
        summary[QStringLiteral("metrics")] = flat;
    } else {
        summary[QStringLiteral("metrics")] = QJsonObject();
        summary[QStringLiteral("note")] = QStringLiteral("尚无评估记录，摘要为空");
    }

    QString json = QString::fromUtf8(QJsonDocument(summary).toJson(QJsonDocument::Indented));
    return writeFileAtomically(deliveryDir + QStringLiteral("/eval_summary.json"), json);
}

bool ExportService::writeWarmupScript(const QString &deliveryDir, const QString &modelFileName)
{
    QString content = QString::fromUtf8(R"(#!/usr/bin/env python3
# -*- coding: utf-8 -*-
\"\"\"模型预热脚本：加载模型并执行一次推理，消除首次调用延迟。\"\"\"
import argparse
import os
import sys
import time


def main() -> int:
    parser = argparse.ArgumentParser(description="预热导出模型")
    parser.add_argument("--model", default=os.path.join(os.path.dirname(__file__), "%1"),
                        help="模型文件路径")
    parser.add_argument("--imgsz", type=int, default=640, help="推理尺寸")
    parser.add_argument("--runs", type=int, default=3, help="预热次数")
    args = parser.parse_args()

    if not os.path.isfile(args.model):
        print(f"[warmup] 模型文件不存在: {args.model}", file=sys.stderr)
        return 1

    model_name = os.path.basename(args.model).lower()
    try:
        if model_name.endswith(".onnx"):
            import numpy as np
            import onnxruntime as ort
            sess = ort.InferenceSession(args.model, providers=["CPUExecutionProvider"])
            inp = sess.get_inputs()[0]
            shape = [(d if isinstance(d, int) else 1) for d in inp.shape]
            dummy = np.random.rand(*shape).astype(np.float32)
            for i in range(args.runs):
                t0 = time.perf_counter()
                sess.run(None, {inp.name: dummy})
                print(f"[warmup] onnx run {i + 1}: {time.perf_counter() - t0:.4f}s")
        else:
            import torch
            try:
                from ultralytics import YOLO
                model = YOLO(args.model)
                import numpy as np
                dummy = (np.random.rand(args.imgsz, args.imgsz, 3) * 255).astype("uint8")
                for i in range(args.runs):
                    t0 = time.perf_counter()
                    model.predict(dummy, imgsz=args.imgsz, verbose=False)
                    print(f"[warmup] yolo run {i + 1}: {time.perf_counter() - t0:.4f}s")
            except ImportError:
                model = torch.jit.load(args.model, map_location="cpu")
                model.eval()
                dummy = torch.rand(1, 3, args.imgsz, args.imgsz)
                with torch.no_grad():
                    for i in range(args.runs):
                        t0 = time.perf_counter()
                        model(dummy)
                        print(f"[warmup] torchscript run {i + 1}: {time.perf_counter() - t0:.4f}s")
        print("[warmup] 完成")
        return 0
    except Exception as exc:
        print(f"[warmup] 失败: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
)").arg(modelFileName);

    return writeFileAtomically(deliveryDir + QStringLiteral("/warmup.py"), content);
}

bool ExportService::writeInferSampleScript(const QString &deliveryDir,
                                           const QString &modelFileName,
                                           const QString &format)
{
    QString content = QString::fromUtf8(R"(#!/usr/bin/env python3
# -*- coding: utf-8 -*-
\"\"\"推理样例脚本：加载交付模型对单张图片做推理并打印结果。

用法:
    python infer_sample.py --image path/to/image.jpg
\"\"\"
import argparse
import json
import os
import sys


def load_class_names():
    \"\"\"从同目录 classes.yaml 读取类别名。\"\"\"
    names = {}
    yaml_path = os.path.join(os.path.dirname(__file__), "classes.yaml")
    if not os.path.isfile(yaml_path):
        return names
    try:
        import yaml
        with open(yaml_path, "r", encoding="utf-8") as f:
            data = yaml.safe_load(f) or {}
        raw = data.get("names", {})
        if isinstance(raw, dict):
            names = {int(k): str(v) for k, v in raw.items()}
        elif isinstance(raw, list):
            names = {i: str(v) for i, v in enumerate(raw)}
    except Exception:
        pass
    return names


def main() -> int:
    parser = argparse.ArgumentParser(description="导出模型推理样例")
    parser.add_argument("--image", required=True, help="输入图片路径")
    parser.add_argument("--model", default=os.path.join(os.path.dirname(__file__), "%1"),
                        help="模型文件路径")
    parser.add_argument("--conf", type=float, default=0.25, help="置信度阈值")
    parser.add_argument("--imgsz", type=int, default=640, help="推理尺寸")
    args = parser.parse_args()

    if not os.path.isfile(args.image):
        print(f"[infer] 图片不存在: {args.image}", file=sys.stderr)
        return 1
    if not os.path.isfile(args.model):
        print(f"[infer] 模型文件不存在: {args.model}", file=sys.stderr)
        return 1

    names = load_class_names()
    model_name = os.path.basename(args.model).lower()

    try:
        if model_name.endswith(".onnx"):
            import numpy as np
            import onnxruntime as ort
            from PIL import Image

            img = Image.open(args.image).convert("RGB").resize((args.imgsz, args.imgsz))
            arr = np.asarray(img, dtype=np.float32) / 255.0
            arr = arr.transpose(2, 0, 1)[None]  # NCHW
            sess = ort.InferenceSession(args.model, providers=["CPUExecutionProvider"])
            inp = sess.get_inputs()[0]
            outputs = sess.run(None, {inp.name: arr})
            print("[infer] ONNX 输出张量:")
            for i, out in enumerate(outputs):
                print(f"  output[{i}] shape={out.shape}")
            print("[infer] 完成（原始输出见上，需按模型头后处理）")
            return 0

        # pt / torchscript：优先 ultralytics
        try:
            from ultralytics import YOLO
            model = YOLO(args.model)
            results = model.predict(args.image, conf=args.conf, imgsz=args.imgsz, verbose=False)
            for r in results:
                boxes = getattr(r, "boxes", None)
                if boxes is None:
                    print("[infer] 无检测框")
                    continue
                for b in boxes:
                    cls_id = int(b.cls[0]) if b.cls is not None else -1
                    conf = float(b.conf[0]) if b.conf is not None else 0.0
                    xyxy = b.xyxy[0].tolist() if b.xyxy is not None else []
                    label = names.get(cls_id, str(cls_id))
                    print(f"[infer] {label} conf={conf:.3f} box={xyxy}")
            print("[infer] 完成")
            return 0
        except ImportError:
            import torch
            model = torch.jit.load(args.model, map_location="cpu")
            model.eval()
            print("[infer] TorchScript 已加载，请根据模型输入自行预处理图片")
            return 0
    except Exception as exc:
        print(f"[infer] 失败: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
)").arg(modelFileName);

    Q_UNUSED(format);
    return writeFileAtomically(deliveryDir + QStringLiteral("/infer_sample.py"), content);
}

QString ExportService::getDeliveryDir(const QString &artifactId) const
{
    auto db = Database::instance().database();
    if (!db.isOpen()) return {};

    // 交付目录固定为 exports/delivery_<artifactId前8位>/
    QSqlQuery query(db);
    query.prepare("SELECT output_path FROM export_artifacts WHERE id = ?");
    query.addBindValue(artifactId);
    if (!query.exec() || !query.next()) return {};

    QString outputPath = query.value(0).toString();
    if (outputPath.isEmpty()) return {};

    QFileInfo outInfo(outputPath);
    QString exportsDir = outInfo.absolutePath();
    QString deliveryDir = exportsDir + QStringLiteral("/delivery_") + artifactId.left(8);
    return QDir(deliveryDir).exists() ? deliveryDir : QString();
}

QString ExportService::generateDeliveryPackage(const QString &artifactId)
{
    ltTrace(LT_LOG_EXPORT()) << "artifactId=" << artifactId;

    auto db = Database::instance().database();
    if (!db.isOpen()) return {};

    QVariantMap details = getExportStatus(artifactId);
    if (details.isEmpty()) {
        ltWarning(LT_LOG_EXPORT()) << "Export artifact not found:" << artifactId;
        return {};
    }

    QString modelVersionId = details[QStringLiteral("modelVersionId")].toString();
    QString outputPath = details[QStringLiteral("outputPath")].toString();
    QString format = details[QStringLiteral("format")].toString();

    if (outputPath.isEmpty() || !QFile::exists(outputPath)) {
        ltWarning(LT_LOG_EXPORT()) << "Export output not ready for delivery package:" << outputPath;
        return {};
    }

    QFileInfo outInfo(outputPath);
    QString exportsDir = outInfo.absolutePath();
    QString deliveryDir = exportsDir + QStringLiteral("/delivery_") + artifactId.left(8);

    QDir dir(deliveryDir);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        ltError(LT_LOG_EXPORT()) << "Failed to create delivery dir:" << deliveryDir;
        return {};
    }

    // 1. 拷贝模型文件（统一命名为 model.<format>）
    QString modelFileName = QStringLiteral("model.") + (format.isEmpty() ? QStringLiteral("onnx") : format);
    QString modelDest = deliveryDir + QLatin1Char('/') + modelFileName;
    QFile::remove(modelDest);
    if (!QFile::copy(outputPath, modelDest)) {
        ltError(LT_LOG_EXPORT()) << "Failed to copy model into delivery dir:" << outputPath;
        return {};
    }

    // 2. classes.yaml / thresholds.json / eval_summary.json / 脚本
    bool ok = true;
    ok = writeClassesYaml(deliveryDir, modelVersionId) && ok;
    ok = writeThresholdsJson(deliveryDir, modelVersionId) && ok;
    ok = writeEvalSummaryJson(deliveryDir, modelVersionId) && ok;
    ok = writeWarmupScript(deliveryDir, modelFileName) && ok;
    ok = writeInferSampleScript(deliveryDir, modelFileName, format) && ok;

    if (!ok) {
        ltError(LT_LOG_EXPORT()) << "Delivery package incomplete for artifact:" << artifactId;
        return {};
    }

    // 3. 清单文件，便于交付核对
    QString manifest;
    QTextStream stream(&manifest);
    stream << "{\n";
    stream << "  \"artifactId\": \"" << artifactId << "\",\n";
    stream << "  \"modelVersionId\": \"" << modelVersionId << "\",\n";
    stream << "  \"modelFile\": \"" << modelFileName << "\",\n";
    stream << "  \"format\": \"" << format << "\",\n";
    stream << "  \"files\": [\"" << modelFileName
           << "\", \"classes.yaml\", \"thresholds.json\", \"eval_summary.json\", \"warmup.py\", \"infer_sample.py\"],\n";
    stream << "  \"generatedAt\": \"" << QDateTime::currentDateTime().toString(Qt::ISODate) << "\"\n";
    stream << "}\n";
    writeFileAtomically(deliveryDir + QStringLiteral("/manifest.json"), manifest);

    ltInfo(LT_LOG_EXPORT()) << "Delivery package ready:" << deliveryDir;
    return deliveryDir;
}
