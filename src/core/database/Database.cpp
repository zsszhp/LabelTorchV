#include "Database.h"
#include "Schema.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
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
    m_dbPath = dbPath;
    m_db = QSqlDatabase::addDatabase("QSQLITE", "labeltorch");
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qWarning() << "Failed to open database:" << dbPath << m_db.lastError().text();
        return false;
    }

    // 启用WAL模式和外键约束
    QSqlQuery query(m_db);
    query.exec("PRAGMA journal_mode=WAL");
    query.exec("PRAGMA foreign_keys=ON");

    qDebug() << "Database opened:" << dbPath;
    return true;
}

void Database::close()
{
    if (m_db.isOpen()) {
        m_db.close();
        qDebug() << "Database closed:" << m_dbPath;
    }
}

bool Database::isOpen() const
{
    return m_db.isOpen();
}

bool Database::initializeSchema()
{
    if (!m_db.isOpen()) return false;

    int version = currentSchemaVersion();
    qDebug() << "Current schema version:" << version;

    if (version == 0) {
        return createTables();
    }

    return migrate();
}

bool Database::migrate()
{
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
    QSqlQuery query(m_db);

    // 创建schema_version表
    if (!query.exec("CREATE TABLE IF NOT EXISTS schema_version ("
                     "version INTEGER PRIMARY KEY"
                     ")")) {
        qWarning() << "Failed to create schema_version table:" << query.lastError().text();
        return false;
    }

    // 创建所有核心表
    const auto &statements = Schema::createTableStatements();
    for (const auto &sql : statements) {
        if (!query.exec(sql)) {
            qWarning() << "Failed to execute:" << sql << query.lastError().text();
            return false;
        }
    }

    // 记录当前schema版本
    query.exec("INSERT INTO schema_version (version) VALUES (1)");
    qDebug() << "Schema initialized successfully";
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
