#ifndef SCANCONTEXT_H
#define SCANCONTEXT_H

#include <QVariantList>
#include <QVariantMap>
#include <QSet>
#include <QList>
#include <algorithm>

/**
 * @brief 扫描上下文工具类，封装各处理器共享的通用逻辑
 */
namespace ScanContext {

/**
 * @brief 将 QSet<int> 排序后转为 QVariantList
 */
inline QVariantList sortClassIds(const QSet<int> &classIds)
{
    QVariantList result;
    QList<int> sortedIds = classIds.values();
    std::sort(sortedIds.begin(), sortedIds.end());
    for (int cid : sortedIds) {
        result.append(cid);
    }
    return result;
}

/**
 * @brief 构建 scanFolder 模式的默认空结果
 */
inline QVariantMap makeEmptyFolderResult()
{
    QVariantMap result;
    result["isValid"] = false;
    result["detectedFormat"] = QString();
    result["imageDir"] = QString();
    result["labelDirOrPath"] = QString();
    result["imageCount"] = 0;
    result["labelCount"] = 0;
    result["unmatchedImagesCount"] = 0;
    result["classIds"] = QVariantList();
    result["classes"] = QVariantMap();
    return result;
}

/**
 * @brief 构建 scan 模式的默认空结果
 */
inline QVariantMap makeEmptyScanResult()
{
    QVariantMap result;
    result["total"] = 0;
    result["matched"] = 0;
    result["unmatchedImages"] = 0;
    result["unmatchedLabels"] = 0;
    result["samples"] = QVariantList();
    return result;
}

} // namespace ScanContext

#endif // SCANCONTEXT_H
