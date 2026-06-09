#include "Database.h"
#include "Schema.h"
#include "utils/Log.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QFileInfo>

Database &Database::instance()
{
    static Database db;
    return db;
}

Database::Database(QObject *parent)
    : QObject(parent)
{
}

Database::~Database()
{
    close();
}

bool Database::open(const QString &dbPath)
{
    ltTrace(LT_LOG_DB()) << "Opening database at" << dbPath;

    m_dbPath = dbPath;
    m_db = QSqlDatabase::addDatabase("QSQLITE", "labeltorch");
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        ltError(LT_LOG_DB()) << "Failed to open database:" << dbPath << m_db.lastError().text();
        return false;
    }

    // 启用WAL模式和外键约束
    QSqlQuery query(m_db);
    query.exec("PRAGMA journal_mode=WAL");
    query.exec("PRAGMA foreign_keys=ON");

    ltInfo(LT_LOG_DB()) << "Database opened:" << dbPath;
    return true;
}

void Database::close()
{
    if (m_db.isOpen()) {
        m_db.close();
        ltInfo(LT_LOG_DB()) << "Database closed:" << m_dbPath;
    }
}

bool Database::isOpen() const
{
    return m_db.isOpen();
}

bool Database::initializeSchema()
{
    ltTrace(LT_LOG_DB()) << "Initializing schema";

    if (!m_db.isOpen()) {
        ltError(LT_LOG_DB()) << "Cannot initialize schema: database not open";
        return false;
    }

    // 始终确保 schema_version 表存在
    QSqlQuery query(m_db);
    if (!query.exec("CREATE TABLE IF NOT EXISTS schema_version ("
                     "version INTEGER PRIMARY KEY"
                     ")")) {
        ltError(LT_LOG_DB()) << "Failed to create schema_version table:" << query.lastError().text();
        return false;
    }

    // 始终对 Schema 中定义的所有核心表强制执行 CREATE TABLE IF NOT EXISTS。
    // 这能确保即使已存在旧的数据库，新增的表（如 run_metrics）也会在下次启动时自动创建补齐。
    const auto &statements = Schema::createTableStatements();
    for (const auto &sql : statements) {
        if (!query.exec(sql)) {
            ltError(LT_LOG_DB()) << "Failed to execute DDL:" << query.lastError().text();
            return false;
        }
    }

    int version = currentSchemaVersion();
    ltDebug(LT_LOG_DB()) << "Current schema version:" << version;

    if (version == 0) {
        query.exec("INSERT INTO schema_version (version) VALUES (1)");
    }

    return migrate();
}

bool Database::migrate()
{
    ltTrace(LT_LOG_DB()) << "Running migrations";

    int version = currentSchemaVersion();
    QSqlQuery query(m_db);

    // 迁移 V1→V2：model_versions 表新增 source 和 import_source_json 字段，放宽 run_id NOT NULL 约束
    if (version < 2) {
        ltInfo(LT_LOG_DB()) << "Running migration V1→V2: model_versions add source/import_source_json";

        // SQLite 不支持 ALTER COLUMN，需用重建表方式迁移
        // 先关闭外键约束，避免重建过程中外键检查失败
        query.exec("PRAGMA foreign_keys=OFF");

        bool ok = true;

        // 创建新表
        ok = ok && query.exec(
            "CREATE TABLE IF NOT EXISTS model_versions_new ("
            "  id TEXT PRIMARY KEY,"
            "  run_id TEXT REFERENCES training_runs(id),"
            "  parent_model_version_id TEXT REFERENCES model_versions(id),"
            "  best_weight_path TEXT,"
            "  last_weight_path TEXT,"
            "  metrics_snapshot_json TEXT,"
            "  export_registry_json TEXT,"
            "  source TEXT NOT NULL DEFAULT 'trained',"
            "  project_id TEXT REFERENCES projects(id),"
            "  import_source_json TEXT,"
            "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
            ")"
        );

        // 迁移旧数据（训练模型通过 run_id 关联 training_runs 获取 project_id）
        ok = ok && query.exec(
            "INSERT OR IGNORE INTO model_versions_new "
            "(id, run_id, parent_model_version_id, best_weight_path, last_weight_path, "
            "metrics_snapshot_json, export_registry_json, source, project_id, import_source_json, created_at) "
            "SELECT mv.id, mv.run_id, mv.parent_model_version_id, mv.best_weight_path, mv.last_weight_path, "
            "mv.metrics_snapshot_json, mv.export_registry_json, 'trained', tr.project_id, NULL, mv.created_at "
            "FROM model_versions mv "
            "LEFT JOIN training_runs tr ON mv.run_id = tr.id"
        );

        // 替换旧表
        ok = ok && query.exec("DROP TABLE IF EXISTS model_versions");
        ok = ok && query.exec("ALTER TABLE model_versions_new RENAME TO model_versions");

        // 恢复外键约束
        query.exec("PRAGMA foreign_keys=ON");

        if (!ok) {
            ltError(LT_LOG_DB()) << "Migration V1→V2 failed";
            query.exec("PRAGMA foreign_keys=ON");
            return false;
        }

        // 更新 schema 版本
        query.exec("INSERT INTO schema_version (version) VALUES (2)");
        ltInfo(LT_LOG_DB()) << "Migration V1→V2 completed successfully";
    }

    return true;
}

QSqlDatabase Database::database() const
{
    return m_db;
}

QString Database::dbPath() const
{
    return m_dbPath;
}

bool Database::createTables()
{
    ltTrace(LT_LOG_DB()) << "Creating tables";

    QSqlQuery query(m_db);

    // 创建schema_version表
    if (!query.exec("CREATE TABLE IF NOT EXISTS schema_version ("
                     "version INTEGER PRIMARY KEY"
                     ")")) {
        ltError(LT_LOG_DB()) << "Failed to create schema_version table:" << query.lastError().text();
        return false;
    }

    // 创建所有核心表
    const auto &statements = Schema::createTableStatements();
    for (const auto &sql : statements) {
        if (!query.exec(sql)) {
            ltError(LT_LOG_DB()) << "Failed to execute DDL:" << query.lastError().text();
            return false;
        }
    }

    // 记录当前schema版本
    query.exec("INSERT INTO schema_version (version) VALUES (1)");
    ltInfo(LT_LOG_DB()) << "Schema initialized successfully";
    return true;
}

int Database::currentSchemaVersion()
{
    QSqlQuery query(m_db);
    if (!query.exec("SELECT version FROM schema_version ORDER BY version DESC LIMIT 1")) {
        return 0; // 表不存在，说明未初始化
    }
    if (query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}
