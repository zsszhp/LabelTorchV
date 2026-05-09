#ifndef DATABASE_H
#define DATABASE_H

#include <QObject>
#include <QString>
#include <QSqlDatabase>

/**
 * @brief SQLite数据库封装
 *
 * 管理数据库连接、初始化、迁移
 */
class Database : public QObject
{
    Q_OBJECT

public:
    static Database &instance();

    bool open(const QString &dbPath);
    void close();
    bool isOpen() const;

    bool initializeSchema();
    bool migrate();

    QSqlDatabase database() const;

private:
    explicit Database(QObject *parent = nullptr);
    ~Database() override;

    bool createTables();
    int currentSchemaVersion();

    QSqlDatabase m_db;
    QString m_dbPath;
};

#endif // DATABASE_H
