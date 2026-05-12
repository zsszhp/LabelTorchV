#include "AnnotationModel.h"
#include "AnnotationService.h"
#include "utils/Id.h"
#include "utils/Log.h"

AnnotationModel::AnnotationModel(QObject *parent)
    : QAbstractListModel(parent)
{
    ltTrace(LT_LOG_ANNOTATION()) << "parent=" << parent;
}

int AnnotationModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_annotations.size();
}

QVariant AnnotationModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_annotations.size())
        return {};

    const AnnotationEntry &ann = m_annotations[index.row()];

    switch (role) {
    case IdRole:          return ann.id;
    case ClassIndexRole:  return ann.classIndex;
    case ClassNameRole:   return ann.className;
    case CxRole:          return ann.cx;
    case CyRole:          return ann.cy;
    case WRole:           return ann.w;
    case HRole:           return ann.h;
    case AngleRole:       return ann.angle;
    case ConfidenceRole:  return ann.confidence;
    case SourceTypeRole:  return ann.sourceType;
    case IsConfirmedRole: return ann.isConfirmed;
    case IsSelectedRole:  return ann.isSelected;
    default:              return {};
    }
}

bool AnnotationModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    ltTrace(LT_LOG_ANNOTATION()) << "row=" << (index.isValid() ? index.row() : -1)
                                 << "role=" << role << "value=" << value;

    if (!index.isValid() || index.row() < 0 || index.row() >= m_annotations.size())
        return false;

    AnnotationEntry &ann = m_annotations[index.row()];

    switch (role) {
    case ClassIndexRole:  ann.classIndex  = value.toInt();    break;
    case ClassNameRole:   ann.className   = value.toString(); break;
    case CxRole:          ann.cx          = value.toFloat();  break;
    case CyRole:          ann.cy          = value.toFloat();  break;
    case WRole:           ann.w           = value.toFloat();  break;
    case HRole:           ann.h           = value.toFloat();  break;
    case AngleRole:       ann.angle       = value.toFloat();  break;
    case ConfidenceRole:  ann.confidence  = value.toFloat();  break;
    case SourceTypeRole:  ann.sourceType  = value.toString(); break;
    case IsConfirmedRole: ann.isConfirmed = value.toBool();   break;
    case IsSelectedRole:  ann.isSelected  = value.toBool();   break;
    default:              return false;
    }

    emit dataChanged(index, index, {role});
    return true;
}

QHash<int, QByteArray> AnnotationModel::roleNames() const
{
    return {
        {IdRole,          "id"},
        {ClassIndexRole,  "classIndex"},
        {ClassNameRole,   "className"},
        {CxRole,          "cx"},
        {CyRole,          "cy"},
        {WRole,           "w"},
        {HRole,           "h"},
        {AngleRole,       "angle"},
        {ConfidenceRole,  "confidence"},
        {SourceTypeRole,  "sourceType"},
        {IsConfirmedRole, "isConfirmed"},
        {IsSelectedRole,  "isSelected"}
    };
}

int AnnotationModel::count() const
{
    return m_annotations.size();
}

void AnnotationModel::loadFromLabel(const QString &labelPath)
{
    ltTrace(LT_LOG_ANNOTATION()) << "labelPath=" << labelPath;

    AnnotationService svc;
    QVariantList loaded = svc.loadAnnotations(labelPath);

    beginResetModel();
    m_annotations.clear();
    m_annotations.reserve(loaded.size());

    for (const QVariant &item : loaded) {
        QVariantMap m = item.toMap();
        AnnotationEntry entry;
        entry.id          = m[QStringLiteral("id")].toString();
        entry.classIndex  = m[QStringLiteral("classIndex")].toInt();
        entry.className   = m[QStringLiteral("className")].toString();
        entry.cx          = static_cast<float>(m[QStringLiteral("cx")].toDouble());
        entry.cy          = static_cast<float>(m[QStringLiteral("cy")].toDouble());
        entry.w           = static_cast<float>(m[QStringLiteral("w")].toDouble());
        entry.h           = static_cast<float>(m[QStringLiteral("h")].toDouble());
        entry.angle       = static_cast<float>(m[QStringLiteral("angle")].toDouble());
        entry.confidence  = static_cast<float>(m[QStringLiteral("confidence")].toDouble());
        entry.sourceType  = m[QStringLiteral("sourceType")].toString();
        entry.isConfirmed = m[QStringLiteral("isConfirmed")].toBool();
        entry.isSelected  = false;
        m_annotations.append(entry);
    }

    endResetModel();
    emit countChanged();

    ltInfo(LT_LOG_ANNOTATION()) << "Loaded" << m_annotations.size()
                                << "annotations from" << labelPath;
}

void AnnotationModel::addAnnotation(int classIndex, const QString &className,
                                    float cx, float cy, float w, float h)
{
    ltTrace(LT_LOG_ANNOTATION()) << "classIndex=" << classIndex << "className=" << className
                                 << "cx=" << cx << "cy=" << cy << "w=" << w << "h=" << h;
    addOBBAnnotation(classIndex, className, cx, cy, w, h, 0.0f);
}

void AnnotationModel::addOBBAnnotation(int classIndex, const QString &className,
                                        float cx, float cy, float w, float h, float angle)
{
    ltTrace(LT_LOG_ANNOTATION()) << "classIndex=" << classIndex << "className=" << className
                                 << "cx=" << cx << "cy=" << cy << "w=" << w << "h=" << h
                                 << "angle=" << angle;

    int newRow = m_annotations.size();
    beginInsertRows(QModelIndex(), newRow, newRow);

    AnnotationEntry entry;
    entry.id          = Id::generate();
    entry.classIndex  = classIndex;
    entry.className   = className;
    entry.cx          = cx;
    entry.cy          = cy;
    entry.w           = w;
    entry.h           = h;
    entry.angle       = angle;
    entry.confidence  = 0.0f;
    entry.sourceType  = QStringLiteral("manual");
    entry.isConfirmed = false;
    entry.isSelected  = false;

    m_annotations.append(entry);

    endInsertRows();
    emit countChanged();

    ltInfo(LT_LOG_ANNOTATION()) << "Added annotation id=" << entry.id
                                << "class=" << className << "row=" << newRow;
}

void AnnotationModel::removeAnnotation(int row)
{
    ltTrace(LT_LOG_ANNOTATION()) << "row=" << row;

    if (row < 0 || row >= m_annotations.size())
        return;

    QString removedId = m_annotations[row].id;
    beginRemoveRows(QModelIndex(), row, row);
    m_annotations.removeAt(row);
    endRemoveRows();
    emit countChanged();

    ltInfo(LT_LOG_ANNOTATION()) << "Removed annotation id=" << removedId << "row=" << row;
}

void AnnotationModel::updateGeometry(int row, float cx, float cy, float w, float h)
{
    ltTrace(LT_LOG_ANNOTATION()) << "row=" << row << "cx=" << cx << "cy=" << cy
                                 << "w=" << w << "h=" << h;

    if (row < 0 || row >= m_annotations.size())
        return;

    AnnotationEntry &ann = m_annotations[row];
    ann.cx = cx;
    ann.cy = cy;
    ann.w  = w;
    ann.h  = h;

    emitDataChanged(row);
}

void AnnotationModel::updateOBBGeometry(int row, float cx, float cy, float w, float h, float angle)
{
    ltTrace(LT_LOG_ANNOTATION()) << "row=" << row << "cx=" << cx << "cy=" << cy
                                 << "w=" << w << "h=" << h << "angle=" << angle;

    if (row < 0 || row >= m_annotations.size())
        return;

    AnnotationEntry &ann = m_annotations[row];
    ann.cx    = cx;
    ann.cy    = cy;
    ann.w     = w;
    ann.h     = h;
    ann.angle = angle;

    emitDataChanged(row);
}

void AnnotationModel::setClassIndex(int row, int classIndex, const QString &className)
{
    ltTrace(LT_LOG_ANNOTATION()) << "row=" << row << "classIndex=" << classIndex
                                 << "className=" << className;

    if (row < 0 || row >= m_annotations.size())
        return;

    AnnotationEntry &ann = m_annotations[row];
    ann.classIndex = classIndex;
    ann.className  = className;

    emitDataChanged(row);

    ltInfo(LT_LOG_ANNOTATION()) << "Changed class for row=" << row << "to" << className;
}

void AnnotationModel::setSelected(int row, bool selected)
{
    ltTrace(LT_LOG_ANNOTATION()) << "row=" << row << "selected=" << selected;

    if (row < 0 || row >= m_annotations.size())
        return;

    AnnotationEntry &ann = m_annotations[row];
    if (ann.isSelected == selected)
        return;

    ann.isSelected = selected;

    QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {IsSelectedRole});
}

QVariantList AnnotationModel::toVariantList() const
{
    ltTrace(LT_LOG_ANNOTATION()) << "count=" << m_annotations.size();

    QVariantList result;
    result.reserve(m_annotations.size());

    for (const AnnotationEntry &ann : m_annotations) {
        QVariantMap m;
        m[QStringLiteral("id")]          = ann.id;
        m[QStringLiteral("classIndex")]  = ann.classIndex;
        m[QStringLiteral("className")]   = ann.className;
        m[QStringLiteral("cx")]          = ann.cx;
        m[QStringLiteral("cy")]          = ann.cy;
        m[QStringLiteral("w")]           = ann.w;
        m[QStringLiteral("h")]           = ann.h;
        m[QStringLiteral("angle")]       = ann.angle;
        m[QStringLiteral("confidence")]  = ann.confidence;
        m[QStringLiteral("sourceType")]  = ann.sourceType;
        m[QStringLiteral("isConfirmed")] = ann.isConfirmed;
        result.append(m);
    }

    return result;
}

void AnnotationModel::emitDataChanged(int row)
{
    QModelIndex idx = index(row);
    emit dataChanged(idx, idx);
}
