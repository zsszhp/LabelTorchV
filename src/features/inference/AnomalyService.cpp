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
        // A9：IPC 连接后异步刷新模型列表
        connect(m_ipcClient, &IpcClient::connectedChanged, this, [this]() {
            if (m_ipcClient && m_ipcClient->connected()) {
                refreshModels();
            }
        });
    }
}

QStringList AnomalyService::fallbackModels()
{
    // A9：完整 12 个 Anomalib 模型（与后端 anomalib_adapter.py 一致）
    return {
        QStringLiteral("patchcore"),
        QStringLiteral("padim"),
        QStringLiteral("stfpm"),
        QStringLiteral("cflow"),
        QStringLiteral("dfkde"),
        QStringLiteral("dfm"),
        QStringLiteral("ganomaly"),
        QStringLiteral("fastflow"),
        QStringLiteral("reverse_distillation"),
        QStringLiteral("csflow"),
        QStringLiteral("devnet"),
        QStringLiteral("efficient_ad"),
    };
}

QStringList AnomalyService::listModels() const
{
    // A9：优先返回缓存（从后端获取），否则返回 fallback 完整列表
    if (!m_cachedModels.isEmpty()) {
        return m_cachedModels;
    }
    return fallbackModels();
}

void AnomalyService::refreshModels()
{
    // A9：通过 IPC 查询后端支持的模型列表
    if (!m_ipcClient || !m_ipcClient->connected()) {
        ltDebug(LT_LOG_INFERENCE()) << "IPC not connected, using fallback model list";
        m_cachedModels = fallbackModels();
        emit modelsRefreshed();
        return;
    }

    ltInfo(LT_LOG_INFERENCE()) << "Refreshing anomaly models from backend";
    m_ipcClient->sendRequest(IpcProtocol::CMD_ANOMALY_LIST_MODELS, {});
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

    // A9：处理 anomaly.list_models 响应
    if (command == IpcProtocol::CMD_ANOMALY_LIST_MODELS) {
        bool success = response[QStringLiteral("success")].toBool();
        if (success) {
            QJsonObject result = response[QStringLiteral("result")].toObject();
            QJsonArray modelsArray = result[QStringLiteral("models")].toArray();
            if (!modelsArray.isEmpty()) {
                m_cachedModels.clear();
                for (const auto &m : modelsArray) {
                    m_cachedModels.append(m.toString());
                }
                ltInfo(LT_LOG_INFERENCE()) << "Anomaly models refreshed from backend, count="
                                            << m_cachedModels.size();
            } else {
                // 后端返回空列表，使用 fallback
                m_cachedModels = fallbackModels();
                ltWarning(LT_LOG_INFERENCE()) << "Backend returned empty model list, using fallback";
            }
        } else {
            // 后端查询失败，使用 fallback
            m_cachedModels = fallbackModels();
            ltWarning(LT_LOG_INFERENCE()) << "Backend model list query failed, using fallback";
        }
        emit modelsRefreshed();
        return;
    }

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
