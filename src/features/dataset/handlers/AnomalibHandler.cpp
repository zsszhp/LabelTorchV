#include "AnomalibHandler.h"
#include "ImportScanner.h"
#include "ScanContext.h"
#include "utils/Log.h"

#include <QDir>

bool AnomalibHandler::canHandle(const QString &folderPath) const
{
    QString trainGoodPath = folderPath + QStringLiteral("/train/good");
    if (!QDir(trainGoodPath).exists()) {
        return false;
    }

    QDir trainGoodDir(trainGoodPath);
    QFileInfoList imageFiles = ImportScanner::collectImageFilesStatic(trainGoodDir, true);
    return !imageFiles.isEmpty();
}

QVariantMap AnomalibHandler::scanFolder(ImportScanner *scanner, const QString &folderPath)
{
    ltTrace(LT_LOG_DATASET()) << "AnomalibHandler::scanFolder folderPath=" << folderPath;

    QVariantMap result = ScanContext::makeEmptyFolderResult();

    QString trainGoodPath = folderPath + QStringLiteral("/train/good");
    if (!QDir(trainGoodPath).exists()) {
        return result;
    }

    int trainGoodCount = ImportScanner::collectImageFilesStatic(QDir(trainGoodPath), true).size();
    int testGoodCount = 0;
    int testDefectiveCount = 0;

    QString testGoodPath = folderPath + QStringLiteral("/test/good");
    QString testDefectivePath = folderPath + QStringLiteral("/test/defective");

    if (QDir(testGoodPath).exists()) {
        testGoodCount = ImportScanner::collectImageFilesStatic(QDir(testGoodPath), true).size();
    }
    if (QDir(testDefectivePath).exists()) {
        testDefectiveCount = ImportScanner::collectImageFilesStatic(QDir(testDefectivePath), true).size();
    }

    int totalImages = trainGoodCount + testGoodCount + testDefectiveCount;
    if (totalImages == 0) {
        return result;
    }

    result["isValid"] = true;
    result["detectedFormat"] = QStringLiteral("anomaly_unsupervised");
    result["imageDir"] = folderPath;
    result["labelDirOrPath"] = QString();
    result["imageCount"] = totalImages;
    result["labelCount"] = 0;
    result["unmatchedImagesCount"] = 0;
    result["classIds"] = QVariantList();
    result["classes"] = QVariantMap();

    QVariantMap layoutStats;
    layoutStats["trainGood"] = trainGoodCount;
    layoutStats["testGood"] = testGoodCount;
    layoutStats["testDefective"] = testDefectiveCount;
    result["layoutStats"] = layoutStats;

    ltInfo(LT_LOG_DATASET()) << "Anomalib layout detected: train/good=" << trainGoodCount
                             << "test/good=" << testGoodCount
                             << "test/defective=" << testDefectiveCount;
    return result;
}

QVariantMap AnomalibHandler::scan(ImportScanner *scanner, const QString &imageDir, const QString &labelDir)
{
    // Anomalib 格式在 scan 模式下不适用，返回空结果
    QVariantMap result = ScanContext::makeEmptyScanResult();
    result["error"] = QStringLiteral("Anomalib 格式不支持分别指定图片和标签目录");
    return result;
}
