#ifndef TAGMODEL_H
#define TAGMODEL_H

#include <QAbstractListModel>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

/**
 * @brief 数据集标签列表模型（A6）
 *
 * 暴露给 QML 的标签列表，支持按数据集加载。
 * 通过 setDatasetId() 触发刷新，监听 TagService::tagsChanged 信号自动更新。
 */
class TagService;

class TagModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString datasetId READ datasetId WRITE setDatasetId NOTIFY datasetIdChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,      ///< 标签 ID
        NameRole,                        ///< 标签名称
        ShortcutRole,                    ///< 快捷键
        CreatedAtRole                    ///< 创建时间
    };

    explicit TagModel(QObject *parent = nullptr);

    void setTagService(TagService *service);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString datasetId() const { return m_datasetId; }
    void setDatasetId(const QString &id);

    /// 刷新标签列表（从数据库重新加载）
    Q_INVOKABLE void refresh();

signals:
    void datasetIdChanged();

private slots:
    void onTagsChanged(const QString &datasetId);

private:
    TagService *m_tagService = nullptr;
    QString m_datasetId;
    QVariantList m_tags;  ///< 缓存的标签列表
};

#endif // TAGMODEL_H
