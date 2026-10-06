#include "UserAction.h"
#include "Log.h"
#include "Breadcrumb.h"

#include <QDateTime>

namespace UserAction {

void log(const QString &action, const QString &target, const QVariantMap &detail)
{
    QString line = QStringLiteral("action=") + action;
    if (!target.isEmpty())
        line += QStringLiteral(" target=") + target;
    for (auto it = detail.constBegin(); it != detail.constEnd(); ++it)
        line += QStringLiteral(" %1=%2").arg(it.key(), it.value().toString());

    // 落盘 + stderr（qCInfo 不带函数名前缀，保持行干净可 grep；noquote 去掉 QString 引号）
    qCInfo(LT_LOG_UI()).noquote() << line;

    // 崩溃摘要"最后用户动作"环：与日志黑匣子相互独立，保证动作序列不被
    // 高频日志冲掉
    Breadcrumb::pushAction(QStringLiteral("[%1] %2")
                               .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                                    line));
}

} // namespace UserAction
