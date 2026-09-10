#ifndef TAGSERVICE_H
#define TAGSERVICE_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

/**
 * @brief 数据集标签管理服务（A6）
 *
 * 管理数据集标签的 CRUD 操作，支持按数据集分组。
 * 标签用于数据集分类与快速筛选。
 */
class TagService : public QObject
{
    Q_OBJECT

public:
    explicit TagService(QObject *parent = nullptr);

    /**
     * @brief 为数据集添加标签
     * @param datasetId 数据集 ID
     * @param name 标签名称
     * @param shortcut 快捷键（可选）
     * @return 标签 ID（失败返回空字符串）
     */
    Q_INVOKABLE QString addTag(const QString &datasetId, const QString &name,
                                const QString &shortcut = QString());

    /**
     * @brief 删除标签
     * @param tagId 标签 ID
     * @return 是否删除成功
     */
    Q_INVOKABLE bool removeTag(const QString &tagId);

    /**
     * @brief 列出数据集的所有标签
     * @param datasetId 数据集 ID
     * @return 标签列表（QVariantList of QVariantMap）
     */
    Q_INVOKABLE QVariantList listTags(const QString &datasetId) const;

    /**
     * @brief 重命名标签
     * @param tagId 标签 ID
     * @param newName 新名称
     * @return 是否成功
     */
    Q_INVOKABLE bool renameTag(const QString &tagId, const QString &newName);

signals:
    /// 标签变更信号（添加/删除/重命名时发射，UI 据此刷新）
    void tagsChanged(const QString &datasetId);

private:
    /// 检查标签名称是否已存在（同一数据集内唯一）
    bool tagExists(const QString &datasetId, const QString &name) const;
};

#endif // TAGSERVICE_H
