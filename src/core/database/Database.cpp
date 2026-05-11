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

    int version = currentSchemaVersion();
    ltDebug(LT_LOG_DB()) << "Current schema version:" << version;

    if (version == 0) {
        return createTables();
    }

    return migrate();
}

bool Database::migrate()
{
    ltTrace(LT_LOG_DB()) << "Running migrations";
    // 迁移机制：后续版本通过增量SQL执行迁移
    // 当前版本为1，无需迁移
    return true;
}

QSqlDatabase Database::database() const
{
    return m_db;
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
