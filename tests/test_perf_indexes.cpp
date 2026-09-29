/**
 * @file test_perf_indexes.cpp
 * @brief P1-23 数据库索引测试
 *
 * 验证 Schema::createIndexStatements 生成的索引能被 SQLite 创建，
 * 且关键查询路径（dataset_samples / run_metrics / training_runs）走索引。
 */
#include <QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTemporaryDir>

#include "database/Database.h"
#include "database/Schema.h"

class TestPerfIndexes : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // 使用独立临时库，避免污染真实 AppData 库
        QVERIFY(m_tempDir.isValid());
        const QString dbPath = m_tempDir.filePath(QStringLiteral("perf_test.db"));
        QVERIFY(Database::instance().open(dbPath));
        QVERIFY(Database::instance().initializeSchema());
    }

    void cleanupTestCase()
    {
        Database::instance().close();
    }

    /// 索引应全部创建成功（幂等可重复执行）
    void test_indexes_created()
    {
        QSqlQuery q(Database::instance().database());
        QVERIFY(q.exec("SELECT name FROM sqlite_master WHERE type='index' AND name LIKE 'idx_%'"));
        QStringList names;
        while (q.next()) {
            names << q.value(0).toString();
        }
        // 至少包含核心性能索引
        QVERIFY(names.contains(QStringLiteral("idx_dataset_samples_dataset_id")));
        QVERIFY(names.contains(QStringLiteral("idx_dataset_samples_hash")));
        QVERIFY(names.contains(QStringLiteral("idx_dataset_samples_split")));
        QVERIFY(names.contains(QStringLiteral("idx_annot_rev_dataset")));
        QVERIFY(names.contains(QStringLiteral("idx_task_events_task")));
        QVERIFY(names.contains(QStringLiteral("idx_run_metrics_run")));
        QVERIFY(names.contains(QStringLiteral("idx_training_runs_project")));
        QVERIFY(names.contains(QStringLiteral("idx_training_runs_snapshot")));
        QVERIFY(names.contains(QStringLiteral("idx_model_versions_run")));
        qDebug() << "indexes found:" << names.size();
    }

    /// 重复执行 initializeSchema 不应报错（幂等）
    void test_indexes_idempotent()
    {
        QVERIFY(Database::instance().initializeSchema());
    }

    /// dataset_samples 按 dataset_id 查询应使用索引（EXPLAIN QUERY PLAN 含 USING INDEX）
    void test_sample_query_uses_index()
    {
        QSqlQuery q(Database::instance().database());
        QVERIFY(q.exec("EXPLAIN QUERY PLAN "
                       "SELECT id, image_path FROM dataset_samples "
                       "WHERE dataset_id = 'x' ORDER BY image_path LIMIT 100"));
        QString plan;
        while (q.next()) {
            plan += q.value(3).toString() + QLatin1Char(' ');
        }
        // SQLite 查询计划中出现 USING INDEX 或 USING COVERING INDEX 说明走了索引
        const bool usesIndex = plan.contains(QStringLiteral("USING"))
                               && plan.contains(QStringLiteral("INDEX"));
        if (!usesIndex) {
            qWarning() << "query plan:" << plan;
        }
        QVERIFY2(usesIndex, qPrintable(QStringLiteral("expected index scan, got: ") + plan));
    }

    /// run_metrics 按 run_id 查询应使用索引
    void test_run_metrics_query_uses_index()
    {
        QSqlQuery q(Database::instance().database());
        QVERIFY(q.exec("EXPLAIN QUERY PLAN "
                       "SELECT metric_name, metric_value FROM run_metrics "
                       "WHERE run_id = 'x'"));
        QString plan;
        while (q.next()) {
            plan += q.value(3).toString() + QLatin1Char(' ');
        }
        const bool usesIndex = plan.contains(QStringLiteral("USING"))
                               && plan.contains(QStringLiteral("INDEX"));
        if (!usesIndex) {
            qWarning() << "query plan:" << plan;
        }
        QVERIFY2(usesIndex, qPrintable(QStringLiteral("expected index scan, got: ") + plan));
    }

private:
    QTemporaryDir m_tempDir;
};

QTEST_MAIN(TestPerfIndexes)
#include "test_perf_indexes.moc"
