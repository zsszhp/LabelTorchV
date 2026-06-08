#include "TestingService.h"
#include "Database.h"
#include "ipc/IpcClient.h"
#include "ipc/IpcProtocol.h"
#include "ModelRegistry.h"
#include "SnapshotService.h"
#include "utils/Log.h"
#include "utils/Id.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtConcurrent>

TestingService::TestingService(QObject *parent)
    : QObject(parent)
{
}

void TestingService::setIpcClient(IpcClient *client)
{
    m_ipcClient = client;
    if (m_ipcClient) {
        connect(m_ipcClient, &IpcClient::eventReceived, this, [this](const QJsonObject &event) {
            QString eventType = event["event_type"].toString();
            if (eventType.startsWith("test.")) {
                QVariantMap ev;
                ev["event_type"] = eventType;
                ev["task_id"] = event["task_id"].toString();
                ev["payload"] = event["payload"].toVariant().toMap();
                handleTestingEvent(ev);
            }
        });
        connect(m_ipcClient, &IpcClient::responseReceived, this, &TestingService::onResponseReceived);
    }
}

void TestingService::setModelRegistry(ModelRegistry *registry)
{
    m_modelRegistry = registry;
}

QString TestingService::createTestTask(const QString &projectId,
                                        const QString &modelVersionId,
                                        const QString &snapshotId,
                                        const QString &config)
{
    auto db = Database::instance().database();
    if (!db.isOpen()) {
        ltWarning(LT_LOG_TESTING()) << "Database not open, cannot create test task";
        return {};
    }

    QString taskId = Id::generate();
    QSqlQuery query(db);
    query.prepare("INSERT INTO testing_runs (id, project_id, model_version_id, snapshot_id, config_json, status, created_at) "
                  "VALUES (?, ?, ?, ?, ?, 'draft', datetime('now'))");
    query.addBindValue(taskId);
    query.addBindValue(projectId);
    query.addBindValue(modelVersionId);
    query.addBindValue(snapshotId);
    query.addBindValue(config);

    if (query.exec()) {
        ltInfo(LT_LOG_TESTING()) << "Test task created:" << taskId;
        return taskId;
    } else {
        ltWarning(LT_LOG_TESTING()) << "Failed to create test task:" << query.lastError().text();
        return {};
    }
}

bool TestingService::startTestTask(const QString &taskId)
{
    if (!m_ipcClient || !m_ipcClient->connected()) {
        ltWarning(LT_LOG_TESTING()) << "IPC not connected, cannot start test task";
        return false;
    }

    auto db = Database::instance().database();
    if (!db.isOpen()) return false;

    // S4: 状态前置检查，只有 draft 状态才能启动
    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT tr.status, tr.model_version_id, tr.snapshot_id, tr.config_json, p.task_type "
                       "FROM testing_runs tr JOIN projects p ON tr.project_id = p.id WHERE tr.id = ?");
    checkQuery.addBindValue(taskId);
    if (!checkQuery.exec() || !checkQuery.next()) {
        ltWarning(LT_LOG_TESTING()) << "Test task not found:" << taskId;
        return false;
    }

    QString currentStatus = checkQuery.value(0).toString();
    if (currentStatus != "draft") {
        ltWarning(LT_LOG_TESTING()) << "Cannot start test task in status:" << currentStatus;
        return false;
    }

    QString modelVersionId = checkQuery.value(1).toString();
    QString snapshotId = checkQuery.value(2).toString();
    QString configJson = checkQuery.value(3).toString();
    QString taskType = checkQuery.value(4).toString();

    // S1: 查询模型权重路径
    QSqlQuery modelQuery(db);
    modelQuery.prepare("SELECT best_weight_path, last_weight_path FROM model_versions WHERE id = ?");
    modelQuery.addBindValue(modelVersionId);
    if (!modelQuery.exec() || !modelQuery.next()) {
        ltWarning(LT_LOG_TESTING()) << "Model version not found:" << modelVersionId;
        return false;
    }

    QString bestWeightPath = modelQuery.value(0).toString();
    QString lastWeightPath = modelQuery.value(1).toString();

    // 根据配置选择权重（0=最佳权重，1=最末权重）
    QJsonObject configObj = QJsonDocument::fromJson(configJson.toUtf8()).object();
    int weightIndex = configObj.value("weight_index").toInt(0);
    QString weightPath = (weightIndex == 1) ? lastWeightPath : bestWeightPath;

    if (weightPath.isEmpty()) {
        ltWarning(LT_LOG_TESTING()) << "No weight path available for model version:" << modelVersionId;
        return false;
    }

    // 立即更新状态为 preparing，防止重复点击
    updateTestTaskStatus(taskId, "preparing");

    // S1: 在后台线程准备快照物理目录（可能涉及文件拷贝，不能在UI线程执行）
    QtConcurrent::run([this, taskId, snapshotId, weightPath, configObj, taskType]() {
        SnapshotService snapshotService;
        QString dataYamlPath = snapshotService.prepareSnapshotPhysicalDir(snapshotId);

        // 回到UI线程处理结果
        QMetaObject::invokeMethod(this, [this, taskId, weightPath, dataYamlPath, configObj, taskType]() {
            if (dataYamlPath.isEmpty()) {
                ltError(LT_LOG_TESTING()) << "Failed to prepare snapshot directories for test:" << taskId;
                updateTestTaskStatus(taskId, "failed");
                return;
            }

            // 构建IPC payload，包含 weight_path 和 data_path
            QJsonObject payload;
            payload["task_id"] = taskId;

            QJsonObject config = configObj;
            config["weight_path"] = weightPath;
            config["data_path"] = dataYamlPath;
            config["task_type"] = taskType;
            payload["config"] = config;

            m_ipcClient->sendRequest(IpcProtocol::CMD_TESTING_START, payload);
            ltInfo(LT_LOG_TESTING()) << "Test task start requested:" << taskId;
        }, Qt::QueuedConnection);
    });

    return true;
}

bool TestingService::stopTestTask(const QString &taskId)
{
    if (!m_ipcClient || !m_ipcClient->connected()) return false;

    // M2: 不立即设置 cancelled，等待 Python 确认后由 test.stopped 事件处理
    QJsonObject payload;
    payload["task_id"] = taskId;
    m_ipcClient->sendRequest(IpcProtocol::CMD_TESTING_STOP, payload);

    ltInfo(LT_LOG_TESTING()) << "Test task stop requested:" << taskId;
    return true;
}

QVariantMap TestingService::getTestResults(const QString &taskId)
{
    auto db = Database::instance().database();
    QSqlQuery query(db);
    query.prepare("SELECT id, project_id, model_version_id, snapshot_id, config_json, status, "
                  "metrics_json, confusion_matrix_json, pr_curve_json, created_at, started_at, finished_at "
                  "FROM testing_runs WHERE id = ?");
    query.addBindValue(taskId);

    if (query.exec() && query.next()) {
        QVariantMap result;
        result["taskId"] = query.value(0).toString();
        result["projectId"] = query.value(1).toString();
        result["modelVersionId"] = query.value(2).toString();
        result["snapshotId"] = query.value(3).toString();
        result["configJson"] = query.value(4).toString();
        result["status"] = query.value(5).toString();
        result["metricsJson"] = query.value(6).toString();
        result["confusionMatrixJson"] = query.value(7).toString();
        result["prCurveJson"] = query.value(8).toString();
        result["createdAt"] = query.value(9).toString();
        result["startedAt"] = query.value(10).toString();
        result["finishedAt"] = query.value(11).toString();
        return result;
    }
    return {};
}

QVariantMap TestingService::getConfusionMatrix(const QString &taskId)
{
    auto db = Database::instance().database();
    QSqlQuery query(db);
    query.prepare("SELECT confusion_matrix_json FROM testing_runs WHERE id = ?");
    query.addBindValue(taskId);

    if (query.exec() && query.next()) {
        QString json = query.value(0).toString();
        if (!json.isEmpty()) {
            return QJsonDocument::fromJson(json.toUtf8()).toVariant().toMap();
        }
    }
    return {};
}

QVariantList TestingService::getPRCurveData(const QString &taskId)
{
    auto db = Database::instance().database();
    QSqlQuery query(db);
    query.prepare("SELECT pr_curve_json FROM testing_runs WHERE id = ?");
    query.addBindValue(taskId);

    if (query.exec() && query.next()) {
        QString json = query.value(0).toString();
        if (!json.isEmpty()) {
            return QJsonDocument::fromJson(json.toUtf8()).toVariant().toList();
        }
    }
    return {};
}

bool TestingService::deleteTestTask(const QString &taskId)
{
    auto db = Database::instance().database();
    QSqlQuery query(db);
    query.prepare("DELETE FROM testing_runs WHERE id = ? AND status IN ('draft', 'cancelled', 'failed', 'succeeded')");
    query.addBindValue(taskId);

    if (query.exec() && query.numRowsAffected() > 0) {
        ltInfo(LT_LOG_TESTING()) << "Test task deleted:" << taskId;
        return true;
    }
    return false;
}

bool TestingService::updateTestTaskStatus(const QString &taskId, const QString &status)
{
    auto db = Database::instance().database();
    QSqlQuery query(db);

    if (status == "running") {
        query.prepare("UPDATE testing_runs SET status = ?, started_at = datetime('now') WHERE id = ?");
    } else if (status == "succeeded" || status == "failed") {
        query.prepare("UPDATE testing_runs SET status = ?, finished_at = datetime('now') WHERE id = ?");
    } else {
        query.prepare("UPDATE testing_runs SET status = ? WHERE id = ?");
    }

    query.addBindValue(status);
    query.addBindValue(taskId);

    if (query.exec()) {
        emit testTaskStatusChanged(taskId, status);
        return true;
    }
    return false;
}

int TestingService::reconcileStaleTasks()
{
    auto db = Database::instance().database();

    // M6: 先查询陈旧任务，逐个更新并发射信号
    QSqlQuery selectQuery(db);
    selectQuery.prepare("SELECT id FROM testing_runs WHERE status IN ('running', 'preparing')");
    if (!selectQuery.exec()) return 0;

    QStringList staleIds;
    while (selectQuery.next()) {
        staleIds << selectQuery.value(0).toString();
    }

    if (staleIds.isEmpty()) return 0;

    // 批量更新状态
    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE testing_runs SET status = 'cancelled', finished_at = datetime('now') "
                        "WHERE status IN ('running', 'preparing')");
    if (!updateQuery.exec()) return 0;

    // 逐个发射信号
    for (const QString &id : staleIds) {
        emit testTaskStatusChanged(id, "cancelled");
    }

    ltInfo(LT_LOG_TESTING()) << "Reconciled" << staleIds.size() << "stale test tasks";
    return staleIds.size();
}

void TestingService::handleTestingEvent(const QVariantMap &event)
{
    QString eventType = event["event_type"].toString();
    QString taskId = event["task_id"].toString();
    QVariantMap payload = event["payload"].toMap();

    if (eventType == "test.started") {
        updateTestTaskStatus(taskId, "running");
    } else if (eventType == "test.progress") {
        int current = payload["current"].toInt();
        int total = payload["total"].toInt();
        emit testProgress(taskId, current, total, payload["metrics"].toMap());
    } else if (eventType == "test.succeeded") {
        // M1: 使用事务保存测试结果，检查执行结果
        auto db = Database::instance().database();
        db.transaction();
        {
            QSqlQuery query(db);
            query.prepare("UPDATE testing_runs SET metrics_json = ?, confusion_matrix_json = ?, pr_curve_json = ? WHERE id = ?");
            query.addBindValue(QJsonDocument::fromVariant(payload["metrics"]).toJson());
            query.addBindValue(QJsonDocument::fromVariant(payload["confusion_matrix"]).toJson());
            query.addBindValue(QJsonDocument::fromVariant(payload["pr_curve"]).toJson());
            query.addBindValue(taskId);
            if (!query.exec()) {
                ltWarning(LT_LOG_TESTING()) << "Failed to save test results:" << query.lastError().text();
                db.rollback();
                updateTestTaskStatus(taskId, "failed");
                return;
            }
        }
        db.commit();
        updateTestTaskStatus(taskId, "succeeded");
    } else if (eventType == "test.failed") {
        updateTestTaskStatus(taskId, "failed");
    } else if (eventType == "test.stopped") {
        updateTestTaskStatus(taskId, "cancelled");
    } else if (eventType == "test.log") {
        // M3: 统一使用 test.log 事件类型
        emit testLog(taskId, payload["message"].toString());
    }
}

void TestingService::onResponseReceived(const QJsonObject &response)
{
    QString command = response["command"].toString();
    if (command != IpcProtocol::CMD_TESTING_START &&
        command != IpcProtocol::CMD_TESTING_STOP &&
        command != IpcProtocol::CMD_TESTING_STATUS) {
        return;
    }

    if (!response["success"].toBool()) {
        QString error = response["error"].toObject()["message"].toString();
        ltWarning(LT_LOG_TESTING()) << "IPC response error for" << command << ":" << error;

        if (command == IpcProtocol::CMD_TESTING_START) {
            QSqlQuery query(Database::instance().database());
            query.prepare(
                "SELECT id FROM testing_runs WHERE status IN ('preparing', 'running') ORDER BY created_at DESC LIMIT 1"
            );
            if (query.exec() && query.next()) {
                QString taskId = query.value(0).toString();
                updateTestTaskStatus(taskId, "failed");
                emit testLog(taskId, error.isEmpty() ? QStringLiteral("测试启动失败") : error);
            }
        }
    }
}
