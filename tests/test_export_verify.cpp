/**
 * @file test_export_verify.cpp
 * @brief P0-3 导出验证闭环测试
 *
 * 期望行为：
 * 1. validation_result.valid=false 时导出产物状态不得为 succeeded，必须为 failed
 * 2. validation_result.valid=true 且 IPC success 时状态为 succeeded
 * 3. IPC 层失败时状态为 failed
 * 4. validation_result 必须落库，且失败时不得写入 valid:true
 *
 * 通过 QMetaObject::invokeMethod 调用 ExportService 的私有槽 handleIpcResponse
 * 注入伪造的 artifact.verify 响应，验证状态机行为。
 */
#include <QTest>
#include <QCoreApplication>
#include <QSqlQuery>
#include <QFile>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>

#include "Database.h"
#include "ExportService.h"
#include "ModelRegistry.h"
#include "IpcProtocol.h"

class TestExportVerify : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void testValidFalseMustNotSucceed();
    void testValidTrueSucceeds();
    void testIpcFailureMarksFailed();
    void testValidFalseNeverWritesValidTrue();
    void testNoVerifierFormatMarkedUnverified();

private:
    /// 伪造 artifact.verify 响应并派发给 ExportService
    void dispatchVerifyResponse(ExportService &service, const QString &artifactId,
                                bool ipcSuccess, const QJsonObject &result);
    /// 查询导出产物状态
    QString statusOf(const QString &artifactId);
    /// 查询 validation_result 原文
    QString validationResultOf(const QString &artifactId);
    /// 创建一条处于 verifying 状态的导出产物，返回 artifactId
    QString createVerifyingArtifact(const QString &format = QStringLiteral("onnx"));

    QTemporaryDir m_tmpDir;
    QString m_projectId;
    QString m_modelVersionId;
};

void TestExportVerify::initTestCase()
{
    QVERIFY2(m_tmpDir.isValid(), "无法创建临时目录");

    QString dbPath = m_tmpDir.path() + "/test_export_verify.db";
    QFile::remove(dbPath);
    QFile::remove(dbPath + "-journal");
    QFile::remove(dbPath + "-wal");
    Database::instance().open(dbPath);
    Database::instance().initializeSchema();

    auto db = Database::instance().database();
    QSqlQuery q(db);

    m_projectId = "proj-export-verify";
    q.prepare("INSERT INTO projects (id, name, root_path, task_type) VALUES (?, ?, ?, ?)");
    q.addBindValue(m_projectId);
    q.addBindValue("ExportVerifyProject");
    q.addBindValue(m_tmpDir.path() + "/proj");
    q.addBindValue("detect");
    QVERIFY(q.exec());

    q.prepare("INSERT INTO taxonomies (id, project_id, name, version, class_definitions_json) "
              "VALUES (?, ?, ?, ?, ?)");
    q.addBindValue("tax-export-verify");
    q.addBindValue(m_projectId);
    q.addBindValue("Default");
    q.addBindValue(1);
    q.addBindValue("[\"defect\"]");
    QVERIFY(q.exec());

    QString dsId = "ds-export-verify";
    q.prepare("INSERT INTO datasets (id, project_id, name, image_root, label_root, format, "
              "sample_count, import_status) VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(dsId);
    q.addBindValue(m_projectId);
    q.addBindValue("EVData");
    q.addBindValue("/tmp/ev/img");
    q.addBindValue("/tmp/ev/lbl");
    q.addBindValue("yolo_txt");
    q.addBindValue(1);
    q.addBindValue("completed");
    QVERIFY(q.exec());

    QString snapId = "snap-export-verify";
    q.prepare("INSERT INTO dataset_snapshots (id, dataset_id, sample_manifest_json, "
              "split_manifest_json, taxonomy_version, annotation_revision_boundary) "
              "VALUES (?, ?, ?, ?, ?, ?)");
    q.addBindValue(snapId);
    q.addBindValue(dsId);
    q.addBindValue("[\"sample-0\"]");
    q.addBindValue("{\"train\":[\"sample-0\"],\"val\":[]}");
    q.addBindValue("tax-export-verify:v1");
    q.addBindValue("none");
    QVERIFY(q.exec());

    QString runId = "run-export-verify";
    q.prepare("INSERT INTO training_runs (id, project_id, snapshot_id, config_snapshot_json, status) "
              "VALUES (?, ?, ?, ?, 'succeeded')");
    q.addBindValue(runId);
    q.addBindValue(m_projectId);
    q.addBindValue(snapId);
    q.addBindValue(R"({"model_family":"yolov8"})");
    QVERIFY(q.exec());

    ModelRegistry registry;
    m_modelVersionId = registry.registerModelVersion(
        runId, "/weights/best.pt", "/weights/last.pt",
        R"({"mAP50":0.9,"fitness":0.85})");
    QVERIFY(!m_modelVersionId.isEmpty());
}

QString TestExportVerify::createVerifyingArtifact(const QString &format)
{
    ExportService service;
    QString artifactId = service.exportModel(m_modelVersionId, format, "{}");
    // 非 void 函数不能用 QVERIFY/QFAIL（会展开为 return;），改用 QTest::qFail + return
    if (artifactId.isEmpty()) {
        QTest::qFail("导出产物 ID 不应为空", __FILE__, __LINE__);
        return {};
    }
    // 导出产物创建后为 running；先切到 verifying 以模拟自动验证流程
    if (!service.updateExportStatus(artifactId, "verifying")) {
        QTest::qFail("切换导出状态到 verifying 失败", __FILE__, __LINE__);
        return {};
    }
    return artifactId;
}

void TestExportVerify::dispatchVerifyResponse(ExportService &service, const QString &artifactId,
                                              bool ipcSuccess, const QJsonObject &result)
{
    QJsonObject response;
    response[QStringLiteral("type")] = QStringLiteral("response");
    response[QStringLiteral("request_id")] = QStringLiteral("req_verify_test");
    response[QStringLiteral("command")] = QString::fromLatin1(IpcProtocol::CMD_ARTIFACT_VERIFY);
    response[QStringLiteral("success")] = ipcSuccess;
    response[QStringLiteral("result")] = result;
    response[QStringLiteral("error")] = QJsonObject();

    bool invoked = QMetaObject::invokeMethod(
        &service, "handleIpcResponse", Q_ARG(QJsonObject, response));
    QVERIFY2(invoked, "应能通过元对象调用 handleIpcResponse 槽");
}

QString TestExportVerify::statusOf(const QString &artifactId)
{
    QSqlQuery q(Database::instance().database());
    q.prepare("SELECT status FROM export_artifacts WHERE id = ?");
    q.addBindValue(artifactId);
    if (!q.exec() || !q.next()) return {};
    return q.value(0).toString();
}

QString TestExportVerify::validationResultOf(const QString &artifactId)
{
    QSqlQuery q(Database::instance().database());
    q.prepare("SELECT validation_result FROM export_artifacts WHERE id = ?");
    q.addBindValue(artifactId);
    if (!q.exec() || !q.next()) return {};
    return q.value(0).toString();
}

// === P0-3 用例 ===

void TestExportVerify::testValidFalseMustNotSucceed()
{
    ExportService service;
    QString artifactId = createVerifyingArtifact();
    QVERIFY(!artifactId.isEmpty());

    QJsonObject result;
    result[QStringLiteral("artifact_id")] = artifactId;
    result[QStringLiteral("valid")] = false;
    result[QStringLiteral("error")] = QStringLiteral("ONNX validation failed: corrupt model");

    dispatchVerifyResponse(service, artifactId, true, result);

    QString status = statusOf(artifactId);
    QVERIFY2(status != QStringLiteral("succeeded"),
             "validation_result.valid=false 时绝不允许 succeeded");
    QCOMPARE(status, QString("failed"));
}

void TestExportVerify::testValidTrueSucceeds()
{
    ExportService service;
    QString artifactId = createVerifyingArtifact();
    QVERIFY(!artifactId.isEmpty());

    QJsonObject result;
    result[QStringLiteral("artifact_id")] = artifactId;
    result[QStringLiteral("valid")] = true;
    result[QStringLiteral("format")] = QStringLiteral("onnx");

    dispatchVerifyResponse(service, artifactId, true, result);

    QCOMPARE(statusOf(artifactId), QString("succeeded"));
}

void TestExportVerify::testIpcFailureMarksFailed()
{
    ExportService service;
    QString artifactId = createVerifyingArtifact();
    QVERIFY(!artifactId.isEmpty());

    // IPC 层 success=false（例如后端异常）
    QJsonObject result;
    result[QStringLiteral("artifact_id")] = artifactId;
    // 注意：即使 result.valid 缺省或误写 true，IPC 失败也必须落 failed
    result[QStringLiteral("valid")] = true;

    dispatchVerifyResponse(service, artifactId, false, result);

    QCOMPARE(statusOf(artifactId), QString("failed"));
}

void TestExportVerify::testValidFalseNeverWritesValidTrue()
{
    ExportService service;
    QString artifactId = createVerifyingArtifact();
    QVERIFY(!artifactId.isEmpty());

    QJsonObject result;
    result[QStringLiteral("artifact_id")] = artifactId;
    result[QStringLiteral("valid")] = false;
    result[QStringLiteral("error")] = QStringLiteral("bad onnx");

    dispatchVerifyResponse(service, artifactId, true, result);

    // validation_result 落库后不得出现 "valid":true
    QString raw = validationResultOf(artifactId);
    QVERIFY(!raw.isEmpty());
    QVERIFY2(!raw.contains("\"valid\":true") && !raw.contains("\"valid\": true"),
             "失败的验证结果绝不允许写入 valid:true");

    QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
    QJsonObject obj = doc.object();
    // 允许 valid 为 false，或被 error 结构覆盖，但不能为 true
    if (obj.contains(QStringLiteral("valid"))) {
        QVERIFY(obj[QStringLiteral("valid")] != QJsonValue(true));
    }
}

void TestExportVerify::testNoVerifierFormatMarkedUnverified()
{
    ExportService service;
    // tflite 属于无验证器格式
    QString artifactId = createVerifyingArtifact(QStringLiteral("tflite"));
    QVERIFY(!artifactId.isEmpty());

    QJsonObject result;
    result[QStringLiteral("artifact_id")] = artifactId;
    // 无验证器：valid 为 null/false，verified=false
    result[QStringLiteral("valid")] = QJsonValue::Null;
    result[QStringLiteral("verified")] = false;

    dispatchVerifyResponse(service, artifactId, true, result);

    QString raw = validationResultOf(artifactId);
    QVERIFY(!raw.isEmpty());
    // 关键约束：绝不写入 valid:true，UI 据此显示「未验证」
    QVERIFY2(!raw.contains("\"valid\":true") && !raw.contains("\"valid\": true"),
             "无验证器格式绝不允许写入 valid:true");
}

QTEST_MAIN(TestExportVerify)
#include "test_export_verify.moc"
