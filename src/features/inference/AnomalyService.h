#ifndef ANOMALYSERVICE_H
#define ANOMALYSERVICE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class IpcClient;

/**
 * @brief 异常检测服务
 *
 * 管理异常检测模型的推理、热力图生成等功能。
 * 训练功能复用 TrainingService（通过 anomalib 适配器）。
 */
class AnomalyService : public QObject
{
    Q_OBJECT

public:
    explicit AnomalyService(QObject *parent = nullptr);

    void setIpcClient(IpcClient *client);

    /**
     * @brief 列出支持的异常检测模型
     * @return 模型名称列表
     */
    Q_INVOKABLE QStringList listModels() const;

    /**
     * @brief 执行异常检测推理
     * @param weightPath 模型权重路径
     * @param imagePaths 图片路径列表（JSON数组字符串）
     * @param modelFamily 模型家族（patchcore/padim/stfpm等）
     * @param device 推理设备
     * @param imgsz 图片尺寸
     * @return true 请求已发送，false 发送失败
     */
    Q_INVOKABLE bool runInference(const QString &weightPath,
                                   const QString &imagePaths,
                                   const QString &modelFamily = "patchcore",
                                   const QString &device = "auto",
                                   int imgsz = 256);

signals:
    /**
     * @brief 推理结果返回信号
     * @param predictions 推理结果列表
     */
    void inferenceResult(const QVariantList &predictions);

    /**
     * @brief 推理失败信号
     * @param error 错误信息
     */
    void inferenceFailed(const QString &error);

private slots:
    void onResponseReceived(const QJsonObject &response);

private:
    IpcClient *m_ipcClient = nullptr;
    int m_pendingInferenceId = 0;
};

#endif // ANOMALYSERVICE_H
