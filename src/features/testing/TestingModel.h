#ifndef TESTINGMODEL_H
#define TESTINGMODEL_H

#include <QAbstractListModel>
#include <QVariantList>

/**
 * @brief 测试任务列表模型
 *
 * 为QML ListView提供测试任务数据
 */
class TestingModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        ProjectIdRole,
        ModelVersionIdRole,
        SnapshotIdRole,
        ConfigRole,
        StatusRole,
        MetricsRole,
        CreatedAtRole,
        StartedAtRole,
        FinishedAtRole
    };

    explicit TestingModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void setProjectId(const QString &projectId);
    Q_INVOKABLE void refresh();
    int count() const;

signals:
    void countChanged();

private:
    QVariantList m_tasks;
    QString m_projectId;
};

#endif // TESTINGMODEL_H
