#ifndef USERACTION_H
#define USERACTION_H

#include <QString>
#include <QVariantMap>

// 用户关键业务动作日志（lt.ui 类别），统一格式：action=<名> target=<对象> <k=v ...>
// 埋点约定：Service/Controller 层公共入口各记一处，不在 QML 按钮上埋——
// 改 UI 不丢日志，一条业务路径一处记录。崩溃时这些行同时进入
// Breadcrumb 的"最后用户动作"环，随崩溃摘要落盘。
namespace UserAction {

void log(const QString &action, const QString &target = {}, const QVariantMap &detail = {});

} // namespace UserAction

#endif // USERACTION_H
