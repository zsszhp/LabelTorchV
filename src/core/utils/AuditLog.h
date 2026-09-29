#ifndef AUDITLOG_H
#define AUDITLOG_H

#include <QString>
#include <QVariantList>
#include <QVariantMap>

/**
 * @brief 统一审计日志：把不可逆/关键操作写入 task_events 表。
 *
 * 所有删除、导入、训练启停、导出等操作经此落库，供事后追溯。
 * 写入失败只记日志不抛出，审计不得阻断主业务流程。
 */
namespace AuditLog {

/**
 * @brief 写入一条审计事件
 * @param taskType 任务类型：project / dataset / training / export / ...
 * @param taskId 关联对象 ID（项目/数据集/训练运行/导出产物等）
 * @param eventType 事件类型：created / deleted / imported / train_start / train_stop / export ...
 * @param payload 附加信息（名称、路径、数量等），序列化存入 payload_json
 * @return true=写入成功
 */
bool record(const QString &taskType, const QString &taskId,
            const QString &eventType, const QVariantMap &payload = {});

/**
 * @brief 查询审计事件
 * @param taskType 任务类型过滤，空串表示不按类型过滤
 * @param taskId 对象 ID 过滤，空串表示不按 ID 过滤
 * @param limit 最大返回条数（按创建时间倒序）
 * @return 事件列表，每项含 id/taskType/taskId/eventType/payload/createdAt
 */
QVariantList listEvents(const QString &taskType = QString(),
                        const QString &taskId = QString(),
                        int limit = 100);

} // namespace AuditLog

#endif // AUDITLOG_H
