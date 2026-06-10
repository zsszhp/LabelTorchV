#ifndef CLASSIFYFOLDERHANDLER_H
#define CLASSIFYFOLDERHANDLER_H

#include "DatasetFormatHandler.h"

/**
 * @brief ImageNet 风格分类数据集格式处理器
 *
 * 识别"每类一文件夹"的目录结构：
 *   dataset/
 *   ├── cat/
 *   │   ├── img001.jpg
 *   │   └── img002.jpg
 *   └── dog/
 *       ├── img003.jpg
 *       └── img004.jpg
 *
 * 每个子文件夹名即为类别名，文件夹内的图片文件为该类别的样本。
 * 不需要独立的标签文件，类别由目录结构隐含。
 */
class ClassifyFolderHandler : public DatasetFormatHandler
{
public:
    bool canHandle(const QString &folderPath) const override;
    QVariantMap scanFolder(ImportScanner *scanner, const QString &folderPath) override;
    QVariantMap scan(ImportScanner *scanner, const QString &imageDir, const QString &labelDir) override;
    QString getFormatName() const override { return QStringLiteral("classify_folder"); }
    int getPriority() const override { return 10; } // 介于 anomalib(5) 和 labelme(15) 之间

private:
    /**
     * @brief 检查目录是否符合 ImageNet 风格分类布局
     * @param folderPath 数据集根目录
     * @return 类别子目录列表（按名称排序），空列表表示不符合
     */
    QStringList detectClassifyLayout(const QString &folderPath) const;
};

#endif // CLASSIFYFOLDERHANDLER_H
