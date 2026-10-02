#ifndef TAXONOMYMODEL_H
#define TAXONOMYMODEL_H

#include <QAbstractListModel>
#include <QString>

class TaxonomyModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString taxonomyId READ taxonomyId WRITE setTaxonomyId NOTIFY taxonomyIdChanged)

public:
    explicit TaxonomyModel(QObject *parent = nullptr);

    // DeprecatedRole：该 class_id 是否已废弃（类名为空的占位）
    enum Roles { ClassNameRole = Qt::UserRole + 1, IndexRole, DeprecatedRole };

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString taxonomyId() const { return m_taxonomyId; }
    void setTaxonomyId(const QString &id);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool addClass(const QString &className);

    /**
     * @brief 删除类别（废弃语义，与 TaxonomyService::removeClass 默认路径一致）。
     *
     * 不做物理删除：YOLO 标签按 class_id（即本列表下标）引用类别，
     * 物理删除会使后续 class_id 整体前移，导致已有标签全部错类。
     * 此处将该索引位置为废弃占位（类名清空），数组长度不变，后续 class_id 不变。
     *
     * @param index 类别索引（class_id）。
     * @return true 成功；false 索引越界或该位已废弃。
     */
    Q_INVOKABLE bool removeClass(int index);

    /**
     * @brief 重命名类别。对废弃占位调用等价于复活该 class_id。
     */
    Q_INVOKABLE bool renameClass(int index, const QString &newName);

    /**
     * @brief 设置类别样式（颜色/快捷键），持久化到 taxonomy_class_styles 表。
     * @param index 类别索引（class_id）。
     * @param color 颜色（#RRGGBB，空串清除）。
     * @param shortcut 快捷键字符（空串清除）。
     */
    Q_INVOKABLE bool setClassStyle(int index, const QString &color, const QString &shortcut);

    /**
     * @brief 读取类别样式，返回 {color, shortcut}（未设置时两项为空串）。
     */
    Q_INVOKABLE QVariantMap getClassStyle(int index) const;

signals:
    void taxonomyIdChanged();

private:
    QString m_taxonomyId;
    QStringList m_classes;
};

#endif
