#include "AnomalyDetector.h"
#include "utils/Log.h"

#include <QFile>
#include <QBuffer>
#include <QDateTime>
#include <cmath>

#ifdef WITH_ONNXRUNTIME
#include <onnxruntime_cxx_api.h>
#endif

struct AnomalyDetector::Impl
{
    bool loaded = false;
    QString modelPath;

    // 从 ONNX 元数据读取的参数
    float imageThreshold = 0.5f;
    float pixelThreshold = 0.5f;
    int inputWidth = 256;
    int inputHeight = 256;
    std::vector<float> normalizationMean = {0.485f, 0.456f, 0.406f};
    std::vector<float> normalizationStd = {0.229f, 0.224f, 0.225f};
    QString algorithm = "efficient_ad";

    QVariantMap metadata;

#ifdef WITH_ONNXRUNTIME
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "LabelTorch-AnomalyDetector"};
    Ort::SessionOptions sessionOptions;
    std::unique_ptr<Ort::Session> session;
#endif
};

AnomalyDetector::AnomalyDetector(QObject *parent)
    : QObject(parent)
    , m_impl(new Impl())
{
    ltTrace(LT_LOG_INFERENCE()) << "AnomalyDetector constructed";
}

AnomalyDetector::~AnomalyDetector()
{
    delete m_impl;
}

bool AnomalyDetector::loadModel(const QString &modelPath)
{
    ltInfo(LT_LOG_INFERENCE()) << "AnomalyDetector::loadModel path=" << modelPath;

    if (modelPath.isEmpty() || !QFile::exists(modelPath)) {
        ltError(LT_LOG_INFERENCE()) << "Model file does not exist:" << modelPath;
        return false;
    }

    m_impl->modelPath = modelPath;

#ifdef WITH_ONNXRUNTIME
    try {
        m_impl->sessionOptions.SetIntraOpNumThreads(1);
        m_impl->sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        m_impl->session = std::make_unique<Ort::Session>(
            m_impl->env, modelPath.toStdWString().c_str(), m_impl->sessionOptions);

        m_impl->loaded = true;
        loadMetadataFromModel();

        ltInfo(LT_LOG_INFERENCE()) << "ONNX model loaded successfully:" << modelPath
                                   << "threshold=" << m_impl->imageThreshold;
        return true;
    } catch (const Ort::Exception &e) {
        ltError(LT_LOG_INFERENCE()) << "Failed to load ONNX model:" << e.what();
        m_impl->loaded = false;
        return false;
    }
#else
    // 无 ONNX Runtime 时，仅读取元数据（不实际推理）
    m_impl->loaded = true;
    loadMetadataFromModel();
    ltWarning(LT_LOG_INFERENCE()) << "ONNX Runtime not available, model loaded in metadata-only mode";
    return true;
#endif
}

void AnomalyDetector::loadMetadataFromModel()
{
    ltTrace(LT_LOG_INFERENCE()) << "Loading metadata from ONNX model";

#ifdef WITH_ONNXRUNTIME
    if (!m_impl->session) return;

    try {
        Ort::ModelMetadata metadata = m_impl->session->GetModelMetadata();
        Ort::AllocatorWithDefaultOptions allocator;

        auto lookupKey = [&](const char *key) -> QString {
            auto val = metadata.LookupCustomMetadataMap(key, allocator);
            return val ? QString::fromUtf8(val.get()) : QString();
        };

        QString thresh = lookupKey("image_threshold");
        if (!thresh.isEmpty()) {
            m_impl->imageThreshold = thresh.toFloat();
        }

        QString pixelThresh = lookupKey("pixel_threshold");
        if (!pixelThresh.isEmpty()) {
            m_impl->pixelThreshold = pixelThresh.toFloat();
        }

        QString inputSize = lookupKey("input_size");
        if (!inputSize.isEmpty()) {
            // 解析 JSON 数组 [256, 256]
            QString cleaned = inputSize;
            cleaned.remove('[').remove(']').remove(' ');
            QStringList parts = cleaned.split(',');
            if (parts.size() >= 2) {
                m_impl->inputWidth = parts[0].toInt();
                m_impl->inputHeight = parts[1].toInt();
            }
        }

        QString algo = lookupKey("algorithm");
        if (!algo.isEmpty()) {
            m_impl->algorithm = algo;
        }

        m_impl->metadata["imageThreshold"] = m_impl->imageThreshold;
        m_impl->metadata["pixelThreshold"] = m_impl->pixelThreshold;
        m_impl->metadata["inputWidth"] = m_impl->inputWidth;
        m_impl->metadata["inputHeight"] = m_impl->inputHeight;
        m_impl->metadata["algorithm"] = m_impl->algorithm;

    } catch (const Ort::Exception &e) {
        ltWarning(LT_LOG_INFERENCE()) << "Failed to read model metadata:" << e.what();
    }
#endif
}

QVariantMap AnomalyDetector::infer(const QString &imagePath)
{
    ltTrace(LT_LOG_INFERENCE()) << "AnomalyDetector::infer image=" << imagePath;

    QVariantMap result;
    result["anomalyScore"] = 0.0;
    result["isAnomalous"] = 0;
    result["anomalyMapWidth"] = 0;
    result["anomalyMapHeight"] = 0;
    result["anomalyMapData"] = QVariantList();

    if (!m_impl->loaded) {
        ltError(LT_LOG_INFERENCE()) << "Model not loaded";
        emit inferenceFailed(QStringLiteral("模型未加载"));
        return result;
    }

#ifdef WITH_ONNXRUNTIME
    if (!m_impl->session) {
        emit inferenceFailed(QStringLiteral("ONNX Session 无效"));
        return result;
    }

    try {
        QImage img(imagePath);
        if (img.isNull()) {
            emit inferenceFailed(QStringLiteral("无法加载图片: %1").arg(imagePath));
            return result;
        }

        int origWidth = img.width();
        int origHeight = img.height();

        // 预处理：resize + RGB + Z-Score 归一化
        QImage resized = img.convertToFormat(QImage::Format_RGB888)
                             .scaled(m_impl->inputWidth, m_impl->inputHeight,
                                     Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        std::vector<float> inputData(m_impl->inputWidth * m_impl->inputHeight * 3);
        int channelStride = m_impl->inputWidth * m_impl->inputHeight;
        for (int y = 0; y < m_impl->inputHeight; ++y) {
            for (int x = 0; x < m_impl->inputWidth; ++x) {
                QRgb pixel = resized.pixel(x, y);
                int pixelIdx = y * m_impl->inputWidth + x;
                inputData[pixelIdx] = (static_cast<float>(qRed(pixel)) / 255.0f - m_impl->normalizationMean[0]) / m_impl->normalizationStd[0];
                inputData[pixelIdx + channelStride] = (static_cast<float>(qGreen(pixel)) / 255.0f - m_impl->normalizationMean[1]) / m_impl->normalizationStd[1];
                inputData[pixelIdx + 2 * channelStride] = (static_cast<float>(qBlue(pixel)) / 255.0f - m_impl->normalizationMean[2]) / m_impl->normalizationStd[2];
            }
        }

        // 执行推理
        auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        std::array<int64_t, 4> inputShape = {1, 3, m_impl->inputHeight, m_impl->inputWidth};
        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            memoryInfo, inputData.data(), inputData.size(),
            inputShape.data(), inputShape.size());

        const char *inputNames[] = {"input"};
        const char *outputNames[] = {"output"};

        auto outputTensors = m_impl->session->Run(
            Ort::RunOptions{nullptr}, inputNames, &inputTensor, 1, outputNames, 1);

        // 解析输出
        float anomalyScore = 0.0f;
        std::vector<float> anomalyMap;

        if (outputTensors.size() > 0) {
            auto &output = outputTensors[0];
            auto typeInfo = output.GetTensorTypeAndShapeInfo();
            auto shape = typeInfo.GetShape();

            float *outputData = output.GetTensorMutableData<float>();
            size_t outputSize = typeInfo.GetElementCount();

            if (outputSize == 1) {
                // 图像级评分
                anomalyScore = outputData[0];
            } else if (shape.size() == 4 && shape[0] == 1) {
                // [1, 1, H, W] 像素级概率图
                int mapH = static_cast<int>(shape[2]);
                int mapW = static_cast<int>(shape[3]);
                anomalyMap.assign(outputData, outputData + outputSize);

                // 取最大值作为图像级评分
                float maxVal = *std::max_element(outputData, outputData + outputSize);
                anomalyScore = maxVal;

                result["anomalyMapWidth"] = mapW;
                result["anomalyMapHeight"] = mapH;

                QVariantList mapList;
                for (float val : anomalyMap) {
                    mapList.append(val);
                }
                result["anomalyMapData"] = mapList;

                // 生成伪彩热力图
                QImage heatmap = createHeatmapImage(anomalyMap, mapW, mapH, origWidth, origHeight);
                if (!heatmap.isNull()) {
                    QByteArray ba;
                    QBuffer buffer(&ba);
                    buffer.open(QIODevice::WriteOnly);
                    heatmap.save(&buffer, "PNG");
                    result["heatmapImage"] = QString::fromLatin1(ba.toBase64());
                }
            }
        }

        // 归一化评分到 [0, 1] 范围
        float normalizedScore = std::min(1.0f, std::max(0.0f, anomalyScore));
        int isAnomalous = (normalizedScore >= m_impl->imageThreshold) ? 1 : 0;

        result["anomalyScore"] = normalizedScore;
        result["isAnomalous"] = isAnomalous;

        ltInfo(LT_LOG_INFERENCE()) << "Anomaly inference: score=" << normalizedScore
                                   << "isAnomalous=" << isAnomalous
                                   << "threshold=" << m_impl->imageThreshold;

        emit inferenceCompleted(result);
        return result;

    } catch (const Ort::Exception &e) {
        ltError(LT_LOG_INFERENCE()) << "ONNX inference failed:" << e.what();
        emit inferenceFailed(QString::fromUtf8(e.what()));
        return result;
    } catch (const std::exception &e) {
        ltError(LT_LOG_INFERENCE()) << "Inference exception:" << e.what();
        emit inferenceFailed(QString::fromUtf8(e.what()));
        return result;
    }
#else
    // 无 ONNX Runtime 时的占位实现
    ltWarning(LT_LOG_INFERENCE()) << "ONNX Runtime not available, returning placeholder result";
    result["anomalyScore"] = 0.0;
    result["isAnomalous"] = 0;
    emit inferenceCompleted(result);
    return result;
#endif
}

bool AnomalyDetector::isLoaded() const
{
    return m_impl->loaded;
}

QVariantMap AnomalyDetector::getModelMetadata() const
{
    return m_impl->metadata;
}

QImage AnomalyDetector::createHeatmapImage(const std::vector<float> &anomalyMap,
                                             int width, int height,
                                             int originalWidth, int originalHeight)
{
    ltTrace(LT_LOG_INFERENCE()) << "Creating heatmap image" << width << "x" << height;

    if (anomalyMap.empty() || width <= 0 || height <= 0) {
        return QImage();
    }

    // 找到最大值用于归一化
    float maxVal = *std::max_element(anomalyMap.begin(), anomalyMap.end());
    if (maxVal <= 0.0f) maxVal = 1.0f;

    // 应用伪彩映射（JET colormap 近似）
    QImage colorImage(width, height, QImage::Format_RGB888);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            float val = std::min(1.0f, std::max(0.0f, anomalyMap[idx] / maxVal));

            int r, g, b;
            if (val < 0.25f) {
                r = 0;
                g = static_cast<int>(val * 4.0f * 255);
                b = 255;
            } else if (val < 0.5f) {
                r = 0;
                g = 255;
                b = static_cast<int>((0.5f - val) * 4.0f * 255);
            } else if (val < 0.75f) {
                r = static_cast<int>((val - 0.5f) * 4.0f * 255);
                g = 255;
                b = 0;
            } else {
                r = 255;
                g = static_cast<int>((1.0f - val) * 4.0f * 255);
                b = 0;
            }

            colorImage.setPixel(x, y, qRgb(r, g, b));
        }
    }

    // 缩放到原始图片尺寸
    QImage scaled = colorImage.scaled(originalWidth > 0 ? originalWidth : width,
                                       originalHeight > 0 ? originalHeight : height,
                                       Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    return scaled;
}
