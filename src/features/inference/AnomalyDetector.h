#ifndef ANOMALYDETECTOR_H
#define ANOMALYDETECTOR_H

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QImage>
#include <vector>

/**
 * @brief 异常检测推理器
 *
 * 基于 ONNX Runtime C++ API 实现纯 C++ 推理，
 * 支持从 ONNX 文件内嵌元数据自适应加载阈值和输入尺寸。
 * 输出图像级异常评分、OK/NG 判定和像素级异常热力图。
 */
class AnomalyDetector : public QObject
{
    Q_OBJECT

public:
    explicit AnomalyDetector(QObject *parent = nullptr);
    ~AnomalyDetector();

    /**
     * @brief 加载 ONNX 模型并读取内嵌元数据
     * @param modelPath ONNX 模型文件路径
     * @return true 加载成功，false 加载失败
     */
    Q_INVOKABLE bool loadModel(const QString &modelPath);

    /**
     * @brief 对单张图片执行异常检测推理
     * @param imagePath 图片文件路径
     * @return QVariantMap 包含：
     *         - "anomalyScore": float 图像级异常评分 [0.0, 1.0]
     *         - "isAnomalous": int OK/NG 判定 (1=NG, 0=OK)
     *         - "anomalyMapWidth": int 热力图宽度
     *         - "anomalyMapHeight": int 热力图高度
     *         - "anomalyMapData": QVariantList 热力图 float 数组
     *         - "heatmapImage": QString 伪彩热力图 QImage 的 base64 编码（PNG格式）
     */
    Q_INVOKABLE QVariantMap infer(const QString &imagePath);

    /**
     * @brief 检查模型是否已加载
     */
    Q_INVOKABLE bool isLoaded() const;

    /**
     * @brief 获取模型元数据
     */
    Q_INVOKABLE QVariantMap getModelMetadata() const;

signals:
    /**
     * @brief 推理完成信号
     * @param result 推理结果
     */
    void inferenceCompleted(const QVariantMap &result);

    /**
     * @brief 推理失败信号
     * @param error 错误信息
     */
    void inferenceFailed(const QString &error);

private:
    /**
     * @brief 从 ONNX 元数据中读取模型参数
     */
    void loadMetadataFromModel();

    /**
     * @brief 将异常热力图 float 数组转换为伪彩 QImage
     * @param anomalyMap 热力图数据
     * @param width 宽度
     * @param height 高度
     * @param originalWidth 原始图片宽度
     * @param originalHeight 原始图片高度
     * @return QImage 伪彩热力图
     */
    QImage createHeatmapImage(const std::vector<float> &anomalyMap,
                               int width, int height,
                               int originalWidth, int originalHeight);

    struct Impl;
    Impl *m_impl;
};

#endif // ANOMALYDETECTOR_H
