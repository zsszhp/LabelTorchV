#include "AnomalyService.h"
#include "ipc/IpcClient.h"
#include "ipc/IpcProtocol.h"
#include "utils/Log.h"

#include <QJsonDocument>
#include <QJsonArray>

AnomalyService::AnomalyService(QObject *parent)
    : QObject(parent)
{
    ltTrace(LT_LOG_INFERENCE()) << "AnomalyService created";
}

void AnomalyService::setIpcClient(IpcClient *client)
{
    ltTrace(LT_LOG_INFERENCE()) << "client=" << client;
    m_ipcClient = client;

    if (m_ipcClient) {
        connect(m_ipcClient, &IpcClient::responseReceived,
                this, &AnomalyService::onResponseReceived);
    }
}

QStringList AnomalyService::listModels() const
{
    return {
        QStringLiteral("patchcore"),
        QStringLiteral("padim"),
        QStringLiteral("stfpm"),
        QStringLiteral("cflow"),
        QStringLiteral("dfkde"),
        QStringLiteral("dfm"),
        QStringLiteral("ganomaly"),
    };
}

bool AnomalyService::runInference(const QString &weightPath,
                                   const QString &imagePaths,
                                   const QString &modelFamily,
                                   const QString &device,
                                   int imgsz)
{
    ltTrace(LT_LOG_INFERENCE()) << "runInference weight=" << weightPath
                                 << "model=" << modelFamily;

    if (!m_ipcClient) {
        ltWarning(LT_LOG_INFERENCE()) << "IPC client not available";
        return false;
    }

    // 解析图片路径JSON数组
    QJsonParseError parseError;
    QJsonDocument pathsDoc = QJsonDocument::fromJson(imagePaths.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        ltWarning(LT_LOG_INFERENCE()) << "Invalid image paths JSON:" << parseError.errorString();
        return false;
    }

    QJsonObject payload;
    payload[QStringLiteral("weight_path")] = weightPath;
    payload[QStringLiteral("image_paths")] = pathsDoc.array();
    payload[QStringLiteral("model_family")] = modelFamily;
    payload[QStringLiteral("device")] = device;
    payload[QStringLiteral("imgsz")] = imgsz;

    m_ipcClient->sendRequest(IpcProtocol::CMD_ANOMALY_INFER, payload);
    return true;
}

void AnomalyService::onResponseReceived(const QJsonObject &response)
{
    // 只处理 anomaly.infer 命令的响应
    QString command = response[QStringLiteral("command")].toString();
    if (command != IpcProtocol::CMD_ANOMALY_INFER) return;

    bool success = response[QStringLiteral("success")].toBool();
    QJsonObject result = response[QStringLiteral("result")].toObject();

    if (success) {
        QVariantList predictions;
        QJsonArray predArray = result[QStringLiteral("predictions")].toArray();
        for (const auto &pred : predArray) {
            predictions.append(pred.toVariant());
        }
        emit inferenceResult(predictions);
        ltInfo(LT_LOG_INFERENCE()) << "Anomaly inference completed, count="
                                    << predictions.size();
    } else {
        QString error = result[QStringLiteral("error")].toString();
        if (error.isEmpty()) {
            auto errorObj = response[QStringLiteral("error")].toObject();
            error = errorObj[QStringLiteral("message")].toString();
        }
        emit inferenceFailed(error);
        ltError(LT_LOG_INFERENCE()) << "Anomaly inference failed:" << error;
    }
}
