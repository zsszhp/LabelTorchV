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
 *
 * 图像 Tag 体系（对标 DLTools）：
 * - 内置评审 Tag（默认/良品/漏检/误检/待定/重要）在数据集首次使用时自动播种，
 *   不可改名、不可删除（builtin=1）；
 * - 每个样本至多归属一个 Tag（dataset_samples.tag_id，单 Tag 模型）；
 * - Tag 快捷键在同一数据集内不允许与其它 Tag 冲突。
 */
class TagService : public QObject
{
    Q_OBJECT

public:
    explicit TagService(QObject *parent = nullptr);

    /**
     * @brief 为数据集添加标签
     * @param datasetId 数据集 ID
     * @param name 标签名称（1~30 字符，数据集内唯一）
     * @param shortcut 快捷键（可选，同数据集内不得与已有 Tag 冲突）
     * @return 标签 ID（失败返回空字符串）
     */
    Q_INVOKABLE QString addTag(const QString &datasetId, const QString &name,
                                const QString &shortcut = QString());

    /**
     * @brief 删除标签（内置 Tag 拒绝删除）
     * @param tagId 标签 ID
     * @return 是否删除成功
     */
    Q_INVOKABLE bool removeTag(const QString &tagId);

    /**
     * @brief 列出数据集的所有标签（含 builtin 标志）
     * @param datasetId 数据集 ID
     * @return 标签列表（QVariantList of QVariantMap）
     */
    Q_INVOKABLE QVariantList listTags(const QString &datasetId) const;

    /**
     * @brief 重命名标签（内置 Tag 拒绝改名）
     * @param tagId 标签 ID
     * @param newName 新名称
     * @return 是否成功
     */
    Q_INVOKABLE bool renameTag(const QString &tagId, const QString &newName);

    /**
     * @brief 内置评审 Tag 播种：数据集尚无任何 Tag 时写入 6 个内置 Tag
     * @param datasetId 数据集 ID
     * @return 数据集已有 Tag 或播种成功返回 true
     */
    Q_INVOKABLE bool ensureBuiltinTags(const QString &datasetId);

    /**
     * @brief 设置单个样本的 Tag（tagId 传空串表示清除）
     * @param sampleId 样本 ID
     * @param tagId Tag ID（可空）
     * @return 是否成功
     */
    Q_INVOKABLE bool setSampleTag(const QString &sampleId, const QString &tagId);

    /**
     * @brief 批量设置样本 Tag（tagId 传空串表示清除）
     * @param sampleIds 样本 ID 列表
     * @param tagId Tag ID（可空）
     * @return 实际更新的样本数
     */
    Q_INVOKABLE int setSamplesTag(const QVariantList &sampleIds, const QString &tagId);

    /**
     * @brief 查询样本当前 Tag ID
     * @param sampleId 样本 ID
     * @return Tag ID，未打标返回空字符串
     */
    Q_INVOKABLE QString getSampleTagId(const QString &sampleId);

signals:
    /// 标签变更信号（添加/删除/重命名时发射，UI 据此刷新）
    void tagsChanged(const QString &datasetId);

    /// 样本 Tag 指派变更信号（参数为样本所属数据集 ID）
    void sampleTagsChanged(const QString &datasetId);

private:
    /// 检查标签名称是否已存在（同一数据集内唯一）
    bool tagExists(const QString &datasetId, const QString &name) const;

    /// 检查快捷键是否与同数据集内其它 Tag 冲突
    bool shortcutExists(const QString &datasetId, const QString &shortcut) const;

    /// 查询样本所属数据集 ID（用于发射信号）
    QString sampleDatasetId(const QString &sampleId) const;
};

#endif // TAGSERVICE_H
