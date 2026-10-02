#ifndef TAXONOMYSERVICE_H
#define TAXONOMYSERVICE_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class TaxonomyService : public QObject
{
    Q_OBJECT
public:
    explicit TaxonomyService(QObject *parent = nullptr);

    // 类别体系CRUD
    Q_INVOKABLE QString createTaxonomy(const QString &projectId, const QString &name, const QVariantList &classes);
    Q_INVOKABLE QVariantList listTaxonomies(const QString &projectId);
    Q_INVOKABLE QVariantMap getTaxonomy(const QString &taxonomyId);
    Q_INVOKABLE bool deleteTaxonomy(const QString &taxonomyId);

    // 类别操作
    Q_INVOKABLE bool addClass(const QString &taxonomyId, const QString &className);

    /**
     * @brief 删除类别（P0-2：废弃 + 保留 id，防止 YOLO class_id 整体前移）。
     *
     * 默认行为（forcePhysical=false）：将该索引位标记为废弃（置空名），保留 id 空位，
     *   后续类别索引不重排，已有标签中的 class_id 不会错位。
     * 物理删除（forcePhysical=true）：真正从数组移除（索引前移），
     *   但会先检测 dataset_samples / 快照引用，有引用则拒绝并要求先走 ClassMappingService 映射向导。
     *
     * @param taxonomyId 类别体系 ID。
     * @param classIndex 要删除的类别索引（YOLO class_id）。
     * @param forcePhysical 是否强制物理删除（默认 false=标记废弃）。
     * @return true 成功；false 失败（原因见 lastError()）。
     */
    Q_INVOKABLE bool removeClass(const QString &taxonomyId, int classIndex, bool forcePhysical = false);

    /**
     * @brief 设置类别样式（颜色/快捷键），存储于 taxonomy_class_styles 表。
     * @param taxonomyId 类别体系 ID。
     * @param classIndex 类别索引（YOLO class_id）。
     * @param color 颜色（#RRGGBB，空串表示清除）。
     * @param shortcut 快捷键字符（空串表示清除）。
     * @return true 成功。
     */
    Q_INVOKABLE bool setClassStyle(const QString &taxonomyId, int classIndex,
                                   const QString &color, const QString &shortcut);
    /**
     * @brief 读取单个类别样式，返回 {color, shortcut}（未设置时两项为空串）。
     */
    Q_INVOKABLE QVariantMap getClassStyle(const QString &taxonomyId, int classIndex);
    /**
     * @brief 读取全部类别样式，返回 {classIndex: {color, shortcut}} 映射。
     */
    Q_INVOKABLE QVariantMap getAllClassStyles(const QString &taxonomyId);

    Q_INVOKABLE bool renameClass(const QString &taxonomyId, int classIndex, const QString &newName);
    Q_INVOKABLE bool reorderClasses(const QString &taxonomyId, const QVariantList &newOrder);
    Q_INVOKABLE QVariantList getClasses(const QString &taxonomyId);

    /**
     * @brief 查询删除某类别的影响面（供 UI 弹窗展示，P0-2）。
     *
     * @param taxonomyId 类别体系 ID。
     * @param classIndex 要删除的类别索引。
     * @return QVariantMap：
     *   - affectedSampleCount  受影响样本数（标签中含该 class_id 的样本）
     *   - affectedSnapshotCount 受影响快照数（taxonomy_version 引用该体系的快照）
     *   - affectedDatasetCount  受影响数据集数
     *   - className            被删类别名
     *   - classIndex           被删类别索引
     */
    Q_INVOKABLE QVariantMap getRemoveImpact(const QString &taxonomyId, int classIndex);

    /**
     * @brief 获取最近一次操作的错误消息（中文）。
     */
    Q_INVOKABLE QString lastError() const;

    /**
     * @brief 获取最近一次操作的错误码。
     *
     * 已定义错误码：
     * - OK
     * - E_INDEX_INVALID      索引越界
     * - E_ALREADY_DEPRECATED 该索引位已废弃
     * - E_IN_USE              物理删除被拒：仍有样本/快照引用该类别
     * - E_DB_ERROR            数据库错误
     */
    Q_INVOKABLE QString lastErrorCode() const;

    // 版本管理
    Q_INVOKABLE int getTaxonomyVersion(const QString &taxonomyId);

private:
    /// 设置错误码与消息
    void setLastError(const QString &code, const QString &message);
    /// 清空错误状态
    void clearLastError();
    /// 写入 class_mapping_revisions 审计记录（表不存在时静默跳过）
    void writeRemovalAudit(const QString &taxonomyId, int classIndex,
                           const QString &className, bool physical);

    QString m_lastErrorCode = QStringLiteral("OK");
    QString m_lastError;
};

#endif
