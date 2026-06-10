#ifndef DATASETFORMATHANDLER_H
#define DATASETFORMATHANDLER_H

#include <QString>
#include <QVariantMap>

class ImportScanner;

/**
 * @brief 数据集格式处理器接口
 *
 * 采用策略模式，每种数据集格式实现此接口。
 * ImportScanner 通过 canHandle() 检测格式，通过 scan() 委托扫描。
 */
class DatasetFormatHandler
{
public:
    virtual ~DatasetFormatHandler() = default;

    /**
     * @brief 检查是否能处理指定目录的数据集格式
     * @param folderPath 数据集目录路径
     * @return true 如果该处理器可以处理此格式
     */
    virtual bool canHandle(const QString &folderPath) const = 0;

    /**
     * @brief 扫描数据集并返回样本信息（scanFolder 模式）
     * @param scanner 所属的 ImportScanner，用于访问共享工具方法
     * @param folderPath 数据集根目录
     * @return QVariantMap 包含扫描结果
     */
    virtual QVariantMap scanFolder(ImportScanner *scanner, const QString &folderPath) = 0;

    /**
     * @brief 扫描指定图片和标签目录（scan 模式）
     * @param scanner 所属的 ImportScanner
     * @param imageDir 图片目录
     * @param labelDir 标签目录
     * @return QVariantMap 包含扫描结果
     */
    virtual QVariantMap scan(ImportScanner *scanner, const QString &imageDir, const QString &labelDir) = 0;

    /**
     * @brief 获取格式名称
     */
    virtual QString getFormatName() const = 0;

    /**
     * @brief 获取格式优先级（数值越小优先级越高）
     */
    virtual int getPriority() const { return 100; }
};

#endif // DATASETFORMATHANDLER_H
