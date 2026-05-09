#include "ExportService.h"
#include "Database.h"
#include "ipc/IpcClient.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <QDateTime>
#include <QDebug>

ExportService::ExportService(QObject *parent) : QObject(parent) {}

void ExportService::setIpcClient(IpcClient *client)
{
    m_ipcClient = client;
}

QString ExportService::exportModel(const QString &modelVersionId,
                                    const QString &format,
                                    const QString &optionsJson)
{
    auto db = Database::instance().database();
    if (!db.isOpen()) return {};

    // Validate model version exists
    QSqlQuery checkVersion(db);
    checkVersion.prepare("SELECT id, best_weight_path FROM model_versions WHERE id = ?");
    checkVersion.addBindValue(modelVersionId);
    if (!checkVersion.exec() || !checkVersion.next()) {
        qWarning() << "Model version not found:" << modelVersionId;
        return {};
    }

    QString bestWeightPath = checkVersion.value(1).toString();

    // Validate format
    if (format != "pt" && format != "onnx" && format != "tflite" && format != "engine") {
        qWarning() << "Invalid export format:" << format;
        return {};
    }

    // Validate optionsJson is valid JSON (or empty)
    QString validatedOptionsJson = optionsJson;
    if (!optionsJson.isEmpty()) {
        QJsonParseError parseError;
        QJsonDocument::fromJson(optionsJson.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            qWarning() << "Invalid options JSON:" << parseError.errorString();
            return {};
        }
    } else {
        validatedOptionsJson = "{}";
    }

    QString artifactId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    // Build output path based on format
    QString outputPath = bestWeightPath;
    if (!outputPath.isEmpty()) {
        int dotPos = outputPath.lastIndexOf('.');
        if (dotPos > 0) {
            outputPath = outputPath.left(dotPos) + "." + format;
        } else {
            outputPath += "." + format;
        }
    } else {
        outputPath = "export_" + artifactId.left(8) + "." + format;
    }

    // Build options_snapshot_json with status field for state machine
    QJsonObject optionsObj = QJsonDocument::fromJson(validatedOptionsJson.toUtf8()).object();
    optionsObj["status"] = "pending";
    QString snapshotJson = QString::fromUtf8(
        QJsonDocument(optionsObj).toJson(QJsonDocument::Compact));

    QSqlQuery query(db);
    query.prepare(
        "INSERT INTO export_artifacts "
        "(id, model_version_id, format, options_snapshot_json, output_path, validation_result) "
        "VALUES (?, ?, ?, ?, ?, ?)"
    );
    query.addBindValue(artifactId);
    query.addBindValue(modelVersionId);
    query.addBindValue(format);
    query.addBindValue(snapshotJson);
    query.addBindValue(outputPath);
    query.addBindValue(""); // validation_result empty initially

    if (!query.exec()) {
        qWarning() << "Failed to create export artifact:" << query.lastError().text();
        return {};
    }

    // Send export.run via IpcClient if available
    if (m_ipcClient) {
        QJsonObject payload;
        payload["artifact_id"] = artifactId;
        payload["model_version_id"] = modelVersionId;
        payload["format"] = format;
        payload["output_path"] = outputPath;
        payload["options"] = QJsonDocument::fromJson(validatedOptionsJson.toUtf8()).object();
        m_ipcClient->sendRequest("export.run", payload);
    }

    // Transition to running status
    updateExportStatus(artifactId, "running");

    emit exportStatusChanged(artifactId, "pending");
    return artifactId;
}

QVariantMap ExportService::getExportStatus(const QString &artifactId)
{
    auto db = Database::instance().database();
    QVariantMap result;

    QSqlQuery query(db);
    query.prepare(
        "SELECT id, model_version_id, format, options_snapshot_json, "
        "output_path, validation_result, created_at "
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

    // Extract status from options_snapshot_json
    QString optionsJson = query.value(3).toString();
    if (!optionsJson.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(optionsJson.toUtf8());
        if (doc.isObject()) {
            result["status"] = doc.object().value("status").toString("pending");
        } else {
            result["status"] = "pending";
        }
    } else {
        result["status"] = "pending";
    }

    return result;
}

QVariantList ExportService::listExports(const QString &modelVersionId)
{
    auto db = Database::instance().database();
    QVariantList result;

    QSqlQuery query(db);
    query.prepare(
        "SELECT id, model_version_id, format, options_snapshot_json, "
        "output_path, validation_result, created_at "
        "FROM export_artifacts WHERE model_version_id = ? "
        "ORDER BY created_at DESC"
    );
    query.addBindValue(modelVersionId);

    if (!query.exec()) {
        qWarning() << "Failed to list exports:" << query.lastError().text();
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

        // Extract status from options_snapshot_json
        QString optionsJson = query.value(3).toString();
        if (!optionsJson.isEmpty()) {
            QJsonDocument doc = QJsonDocument::fromJson(optionsJson.toUtf8());
            if (doc.isObject()) {
                artifact["status"] = doc.object().value("status").toString("pending");
            } else {
                artifact["status"] = "pending";
            }
        } else {
            artifact["status"] = "pending";
        }

        result.append(artifact);
    }

    return result;
}

bool ExportService::verifyExport(const QString &artifactId)
{
    auto db = Database::instance().database();
    if (!db.isOpen()) return false;

    // Get current status
    QSqlQuery getQuery(db);
    getQuery.prepare("SELECT options_snapshot_json FROM export_artifacts WHERE id = ?");
    getQuery.addBindValue(artifactId);
    if (!getQuery.exec() || !getQuery.next()) return false;

    QString optionsJsonStr = getQuery.value(0).toString();

    QJsonObject optionsObj;
    if (!optionsJsonStr.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(optionsJsonStr.toUtf8());
        if (doc.isObject()) {
            optionsObj = doc.object();
        }
    }

    // Can only verify succeeded exports
    QString currentStatus = optionsObj.value("status").toString("pending");
    if (currentStatus != "succeeded") {
        qWarning() << "Cannot verify export in status:" << currentStatus;
        return false;
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
        m_ipcClient->sendRequest("artifact.verify", payload);
    }

    return true;
}

bool ExportService::updateExportStatus(const QString &artifactId, const QString &status)
{
    auto db = Database::instance().database();
    if (!db.isOpen()) return false;

    // Get current options_snapshot_json
    QSqlQuery getQuery(db);
    getQuery.prepare("SELECT options_snapshot_json FROM export_artifacts WHERE id = ?");
    getQuery.addBindValue(artifactId);
    if (!getQuery.exec() || !getQuery.next()) return false;

    QString optionsJsonStr = getQuery.value(0).toString();

    QJsonObject optionsObj;
    if (!optionsJsonStr.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(optionsJsonStr.toUtf8());
        if (doc.isObject()) {
            optionsObj = doc.object();
        }
    }

    optionsObj["status"] = status;
    QString updatedJson = QString::fromUtf8(
        QJsonDocument(optionsObj).toJson(QJsonDocument::Compact));

    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE export_artifacts SET options_snapshot_json = ? WHERE id = ?");
    updateQuery.addBindValue(updatedJson);
    updateQuery.addBindValue(artifactId);

    if (!updateQuery.exec()) {
        qWarning() << "Failed to update export status:" << updateQuery.lastError().text();
        return false;
    }

    emit exportStatusChanged(artifactId, status);
    return true;
}
