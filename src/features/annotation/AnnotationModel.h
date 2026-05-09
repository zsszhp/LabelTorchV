#ifndef ANNOTATIONMODEL_H
#define ANNOTATIONMODEL_H

#include <QAbstractListModel>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

struct AxisAlignedBox;

/**
 * @brief QAbstractListModel for the current image's annotations.
 *
 * Each row represents one AxisAlignedBox annotation with roles for all
 * annotation properties. Provides QML-invokable methods for loading,
 * adding, removing, updating, and exporting annotations.
 */
class AnnotationModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        IdRole          = Qt::UserRole + 1,
        ClassIndexRole,
        ClassNameRole,
        CxRole,
        CyRole,
        WRole,
        HRole,
        ConfidenceRole,
        SourceTypeRole,
        IsConfirmedRole,
        IsSelectedRole
    };
    Q_ENUM(Roles)

    explicit AnnotationModel(QObject *parent = nullptr);

    // --- QAbstractListModel interface ---
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QHash<int, QByteArray> roleNames() const override;

    // --- Property ---
    int count() const;

    // --- QML-invokable API ---
    Q_INVOKABLE void loadFromLabel(const QString &labelPath);
    Q_INVOKABLE void addAnnotation(int classIndex, const QString &className,
                                   float cx, float cy, float w, float h);
    Q_INVOKABLE void removeAnnotation(int row);
    Q_INVOKABLE void updateGeometry(int row, float cx, float cy, float w, float h);
    Q_INVOKABLE void setClassIndex(int row, int classIndex, const QString &className);
    Q_INVOKABLE void setSelected(int row, bool selected);
    Q_INVOKABLE QVariantList toVariantList() const;

signals:
    void countChanged();

private:
    struct AnnotationEntry {
        QString id;
        int     classIndex  = -1;
        QString className;
        float   cx          = 0.0f;
        float   cy          = 0.0f;
        float   w           = 0.0f;
        float   h           = 0.0f;
        float   confidence  = 0.0f;
        QString sourceType  = QStringLiteral("manual");
        bool    isConfirmed = true;
        bool    isSelected  = false;
    };

    void emitDataChanged(int row);

    QVector<AnnotationEntry> m_annotations;
};

#endif // ANNOTATIONMODEL_H
