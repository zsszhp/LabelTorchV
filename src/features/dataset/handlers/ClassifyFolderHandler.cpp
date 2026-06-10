#include "ClassifyFolderHandler.h"
#include "ImportScanner.h"
#include "ScanContext.h"
#include "utils/Log.h"

#include <QDir>
#include <QMap>

bool ClassifyFolderHandler::canHandle(const QString &folderPath) const
{
    QStringList classDirs = detectClassifyLayout(folderPath);
    return !classDirs.isEmpty();
}

QVariantMap ClassifyFolderHandler::scanFolder(ImportScanner *scanner, const QString &folderPath)
{
    ltTrace(LT_LOG_DATASET()) << "ClassifyFolderHandler::scanFolder folderPath=" << folderPath;

    QVariantMap result = ScanContext::makeEmptyFolderResult();

    QStringList classDirs = detectClassifyLayout(folderPath);
    if (classDirs.isEmpty()) {
        return result;
    }

    // 统计每个类别的图片数量
    int totalImages = 0;
    QVariantMap classCounts;
    QMap<int, QString> classIndexMap; // class_id -> class_name

    for (int i = 0; i < classDirs.size(); ++i) {
        QDir classDir(folderPath + QStringLiteral("/") + classDirs[i]);
        QFileInfoList images = ImportScanner::collectImageFilesStatic(classDir, true);
        int count = images.size();
        totalImages += count;
        classCounts[classDirs[i]] = count;
        classIndexMap[i] = classDirs[i];
    }

    if (totalImages == 0) {
        return result;
    }

    // 构建 classIds 列表（0, 1, 2, ...）
    QVariantList classIdList;
    for (int i = 0; i < classDirs.size(); ++i) {
        classIdList.append(i);
    }

    result["isValid"] = true;
    result["detectedFormat"] = QStringLiteral("classify_folder");
    result["imageDir"] = folderPath;
    result["labelDirOrPath"] = QString();
    result["imageCount"] = totalImages;
    result["labelCount"] = 0;
    result["unmatchedImagesCount"] = 0;
    result["classIds"] = classIdList;
    result["classes"] = classCounts;

    // 分类数据集特有的布局统计信息
    QVariantMap layoutStats;
    layoutStats["numClasses"] = classDirs.size();
    layoutStats["classNames"] = classDirs;
    result["layoutStats"] = layoutStats;

    ltInfo(LT_LOG_DATASET()) << "Classify folder layout detected: classes=" << classDirs.size()
                             << "totalImages=" << totalImages;
    return result;
}

QVariantMap ClassifyFolderHandler::scan(ImportScanner *scanner, const QString &imageDir, const QString &labelDir)
{
    // 分类文件夹格式不支持分别指定图片和标签目录
    QVariantMap result = ScanContext::makeEmptyScanResult();
    result["error"] = QStringLiteral("分类文件夹格式不支持分别指定图片和标签目录");
    return result;
}

QStringList ClassifyFolderHandler::detectClassifyLayout(const QString &folderPath) const
{
    QDir rootDir(folderPath);
    if (!rootDir.exists()) {
        return {};
    }

    // 获取所有子目录（排除常见的非类别目录）
    QFileInfoList subDirs = rootDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    if (subDirs.isEmpty()) {
        return {};
    }

    // 排除已知的数据集结构目录（这些不是类别目录）
    static const QStringList excludeDirNames = {
        QStringLiteral("images"),
        QStringLiteral("labels"),
        QStringLiteral("train"),
        QStringLiteral("val"),
        QStringLiteral("test"),
        QStringLiteral("valid"),
        QStringLiteral("annotations"),
        QStringLiteral("cache"),
        QStringLiteral("__pycache__"),
        QStringLiteral(".git"),
        QStringLiteral(".svn")
    };

    // 检查子目录是否包含图片文件（ImageNet 风格布局的特征）
    QStringList classDirs;
    int totalImages = 0;

    for (const auto &subDir : subDirs) {
        QString dirName = subDir.fileName();

        // 跳过已知的数据集结构目录
        if (excludeDirNames.contains(dirName.toLower())) {
            continue;
        }

        // 检查子目录内是否有图片文件
        QDir classDir(subDir.absoluteFilePath());
        QFileInfoList images = ImportScanner::collectImageFilesStatic(classDir, false);

        if (!images.isEmpty()) {
            classDirs.append(dirName);
            totalImages += images.size();
        }
    }

    // 至少需要2个类别目录才认为是分类布局（单类别没有分类意义）
    // 且至少有一定数量的图片
    if (classDirs.size() >= 2 && totalImages >= 2) {
        // 按名称排序，确保类别索引稳定
        classDirs.sort();
        return classDirs;
    }

    return {};
}
