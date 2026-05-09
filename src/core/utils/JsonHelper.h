#ifndef JSONHELPER_H
#define JSONHELPER_H

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>

namespace JsonHelper {

inline QByteArray toByteArray(const QJsonObject &obj) {
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

inline QJsonObject fromByteArray(const QByteArray &data) {
    return QJsonDocument::fromJson(data).object();
}

} // namespace JsonHelper

#endif // JSONHELPER_H
