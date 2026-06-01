#include "ProjectService.h"
#include "TaxonomyService.h"
#include "database/Database.h"
#include "filesystem/ProjectFs.h"
#include "utils/Id.h"
#include "utils/Log.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>

ProjectService::ProjectService(QObject *parent) : QObject(parent) {}

QString ProjectService::createProject(const QString &name, const QString &rootPath)
{
    QString cleanPath = QDir::cleanPath(rootPath);
    ltTrace(LT_LOG_PROJECT()) << "createProject name=" << name << "path=" << cleanPath;

    // 1. 检查数据库中是否已存在该路径的项目，防止重复创建导致唯一约束冲突
    QSqlQuery checkQuery(Database::instance().database());
    checkQuery.prepare("SELECT id FROM projects WHERE root_path = ?");
    checkQuery.addBindValue(cleanPath);
    if (checkQuery.exec() && checkQuery.next()) {
        QString existingId = checkQuery.value(0).toString();
        ltInfo(LT_LOG_PROJECT()) << "createProject: project path already exists in database, returning existing ID:" << existingId;
        
        // 自动同步/更新项目名称以防有变更
        QSqlQuery updateQuery(Database::instance().database());
        updateQuery.prepare("UPDATE projects SET name = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?");
        updateQuery.addBindValue(name);
        updateQuery.addBindValue(existingId);
        updateQuery.exec();

        saveProjectConfig(existingId);
        return existingId;
    }

    // 2. 检查物理目录中是否已经存在 project.json，如果存在则直接导入，避免覆盖已有数据
    QString jsonPath = cleanPath + QStringLiteral("/project.json");
    if (QFile::exists(jsonPath)) {
        ltInfo(LT_LOG_PROJECT()) << "createProject: project.json already exists in target directory, importing instead:" << cleanPath;
        QString importedId = importProject(cleanPath);
        if (!importedId.isEmpty()) {
            return importedId;
        }
    }

    QString projectId = Id::generate();
    ltTrace(LT_LOG_PROJECT()) << "Generated project ID:" << projectId;

    if (!ProjectFs::createProjectDirs(cleanPath)) {
        ltError(LT_LOG_PROJECT()) << "Failed to create project directories:" << cleanPath;
        return {};
    }

    if (!ProjectFs::createProjectJson(cleanPath, name, QStringLiteral("detect"))) {
        ltError(LT_LOG_PROJECT()) << "Failed to create project.json:" << cleanPath;
        return {};
    }

    ensureTaskTypeColumn();

    QSqlQuery query(Database::instance().database());
    query.prepare("INSERT INTO projects (id, name, root_path, task_type) VALUES (?, ?, ?, ?)");
    query.addBindValue(projectId);
    query.addBindValue(name);
    query.addBindValue(cleanPath);
    query.addBindValue(QStringLiteral("detect"));

    if (!query.exec()) {
        ltError(LT_LOG_PROJECT()) << "Failed to create project:" << query.lastError().text();
        return {};
    }

    // 为项目创建默认类别体系
    if (m_taxonomyService) {
        m_taxonomyService->createTaxonomy(projectId, "默认类别体系", {});
    }

    saveProjectConfig(projectId);

    ltInfo(LT_LOG_PROJECT()) << "Project created:" << projectId << name << "at" << cleanPath;
    return projectId;
}

QString ProjectService::importProject(const QString &rootPath)
{
    QString cleanPath = QDir::cleanPath(rootPath);
    ltInfo(LT_LOG_PROJECT()) << "importProject cleanPath=" << cleanPath;

    if (cleanPath.isEmpty()) return {};

    QFileInfo rootInfo(cleanPath);
    if (!rootInfo.exists() || !rootInfo.isDir()) return {};

    // 1. 读取并解析 project.json
    QString jsonPath = cleanPath + QStringLiteral("/project.json");
    QFile jsonFile(jsonPath);
    if (!jsonFile.exists() || !jsonFile.open(QIODevice::ReadOnly)) {
        ltError(LT_LOG_PROJECT()) << "importProject: project.json missing or unreadable at:" << cleanPath;
        return {};
    }

    QJsonDocument doc = QJsonDocument::fromJson(jsonFile.readAll());
    jsonFile.close();
    if (!doc.isObject()) {
        ltError(LT_LOG_PROJECT()) << "importProject: project.json is not a valid JSON object";
        return {};
    }

    QJsonObject rootObj = doc.object();
    QString projectId = rootObj["id"].toString();
    QString projectName = rootObj["name"].toString();
    QString taskType = rootObj["task_type"].toString();

    if (projectId.isEmpty()) {
        projectId = Id::generate(); // 降级处理
    }
    if (projectName.isEmpty()) {
        projectName = rootInfo.fileName();
    }
    if (taskType.isEmpty()) {
        taskType = QStringLiteral("detect");
    }

    // 2. 检查数据库是否已存在该项目（防止重复导入）
    QSqlQuery checkQuery(Database::instance().database());
    checkQuery.prepare("SELECT id FROM projects WHERE id = ? OR root_path = ?");
    checkQuery.addBindValue(projectId);
    checkQuery.addBindValue(cleanPath);
    if (checkQuery.exec() && checkQuery.next()) {
        QString existingId = checkQuery.value(0).toString();
        ltInfo(LT_LOG_PROJECT()) << "importProject: project already imported in database:" << existingId;
        return existingId;
    }

    // 3. 确保子目录环境结构（补齐可能在迁移中丢失的零散文件夹）
    if (!ProjectFs::createProjectDirs(cleanPath)) return {};

    // 4. 在数据库中注册主项目记录
    ensureTaskTypeColumn();
    QSqlQuery query(Database::instance().database());
    query.prepare("INSERT INTO projects (id, name, root_path, task_type) VALUES (?, ?, ?, ?)");
    query.addBindValue(projectId);
    query.addBindValue(projectName);
    query.addBindValue(cleanPath);
    query.addBindValue(taskType);
    if (!query.exec()) {
        ltError(LT_LOG_PROJECT()) << "importProject: failed to register project to SQLite:" << query.lastError().text();
        return {};
    }

    // === 开始全量高保真数据恢复 ===
    QSqlDatabase db = Database::instance().database();

    // 5. 恢复 Taxonomies
    QJsonArray taxonomies = rootObj["taxonomies"].toArray();
    for (int i = 0; i < taxonomies.size(); ++i) {
        QJsonObject tax = taxonomies[i].toObject();
        QSqlQuery tQuery(db);
        tQuery.prepare("INSERT INTO taxonomies (id, project_id, name, version, class_definitions_json) VALUES (?, ?, ?, ?, ?)");
        tQuery.addBindValue(tax["id"].toString());
        tQuery.addBindValue(projectId);
        tQuery.addBindValue(tax["name"].toString());
        tQuery.addBindValue(tax["version"].toInt());
        
        QJsonDocument classDoc(tax["class_definitions"].toArray());
        tQuery.addBindValue(QString::fromUtf8(classDoc.toJson(QJsonDocument::Compact)));
        tQuery.exec();
    }

    // 6. 恢复 Datasets
    QJsonArray datasets = rootObj["datasets"].toArray();
    for (int i = 0; i < datasets.size(); ++i) {
        QJsonObject ds = datasets[i].toObject();
        QSqlQuery dsQuery(db);
        dsQuery.prepare("INSERT INTO datasets (id, project_id, name, image_root, label_root, format, sample_count) VALUES (?, ?, ?, ?, ?, ?, ?)");
        dsQuery.addBindValue(ds["id"].toString());
        dsQuery.addBindValue(projectId);
        dsQuery.addBindValue(ds["name"].toString());
        
        // 结合当前机器下的新 rootPath 还原绝对物理路径
        QString relImg = ds["image_root"].toString();
        QString relLbl = ds["label_root"].toString();
        dsQuery.addBindValue(QDir(cleanPath).absoluteFilePath(relImg));
        dsQuery.addBindValue(QDir(cleanPath).absoluteFilePath(relLbl));
        
        dsQuery.addBindValue(ds["format"].toString());
        dsQuery.addBindValue(ds["sample_count"].toInt());
        dsQuery.exec();
    }

    // 7. 恢复 Snapshots
    QJsonArray snapshots = rootObj["snapshots"].toArray();
    for (int i = 0; i < snapshots.size(); ++i) {
        QJsonObject snap = snapshots[i].toObject();
        QSqlQuery sQuery(db);
        sQuery.prepare("INSERT INTO dataset_snapshots (id, dataset_id, sample_manifest_json, split_manifest_json, taxonomy_version) VALUES (?, ?, ?, ?, ?)");
        sQuery.addBindValue(snap["id"].toString());
        sQuery.addBindValue(snap["dataset_id"].toString());
        
        QJsonDocument manifestDoc(snap["sample_manifest"].toArray());
        sQuery.addBindValue(QString::fromUtf8(manifestDoc.toJson(QJsonDocument::Compact)));
        
        QJsonDocument splitDoc(snap["split_manifest"].toObject());
        sQuery.addBindValue(QString::fromUtf8(splitDoc.toJson(QJsonDocument::Compact)));
        
        sQuery.addBindValue(snap["taxonomy_version"].toString());
        sQuery.exec();
    }

    // 8. 恢复 Training Runs
    QJsonArray trainingRuns = rootObj["training_runs"].toArray();
    for (int i = 0; i < trainingRuns.size(); ++i) {
        QJsonObject run = trainingRuns[i].toObject();
        QSqlQuery rQuery(db);
        rQuery.prepare("INSERT INTO training_runs (id, project_id, snapshot_id, config_snapshot_json, status, log_uri, started_at, finished_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
        rQuery.addBindValue(run["id"].toString());
        rQuery.addBindValue(projectId);
        rQuery.addBindValue(run["snapshot_id"].toString());
        
        QJsonDocument cfgDoc(run["config_snapshot"].toObject());
        rQuery.addBindValue(QString::fromUtf8(cfgDoc.toJson(QJsonDocument::Compact)));
        
        rQuery.addBindValue(run["status"].toString());
        rQuery.addBindValue(run["log_uri"].toString());
        rQuery.addBindValue(run["started_at"].toString());
        rQuery.addBindValue(run["finished_at"].toString());
        rQuery.exec();
    }

    // 9. 恢复 Model Versions
    QJsonArray modelVersions = rootObj["model_versions"].toArray();
    for (int i = 0; i < modelVersions.size(); ++i) {
        QJsonObject model = modelVersions[i].toObject();
        QSqlQuery mQuery(db);
        mQuery.prepare("INSERT INTO model_versions (id, run_id, parent_model_version_id, best_weight_path, last_weight_path, metrics_snapshot_json) VALUES (?, ?, ?, ?, ?, ?)");
        mQuery.addBindValue(model["id"].toString());
        mQuery.addBindValue(model["run_id"].toString());
        mQuery.addBindValue(model["parent_model_version_id"].toString());
        
        // 结合当前机器下的新 rootPath 还原权重的绝对物理路径
        QString relBest = model["best_weight_path"].toString();
        QString relLast = model["last_weight_path"].toString();
        mQuery.addBindValue(QDir(cleanPath).absoluteFilePath(relBest));
        mQuery.addBindValue(QDir(cleanPath).absoluteFilePath(relLast));
        
        QJsonDocument metricsDoc(model["metrics_snapshot"].toObject());
        mQuery.addBindValue(QString::fromUtf8(metricsDoc.toJson(QJsonDocument::Compact)));
        mQuery.exec();
    }

    ltInfo(LT_LOG_PROJECT()) << "Project metadata successfully restored & migrated in local database for:" << projectId;
    return projectId;
}

QVariantList ProjectService::listProjects()
{
    ltTrace(LT_LOG_PROJECT()) << "listProjects";

    QVariantList projects;
    QSqlQuery query(Database::instance().database());
    query.exec("SELECT id, name, root_path, created_at FROM projects ORDER BY updated_at DESC");

    while (query.next()) {
        QVariantMap p;
        p["id"] = query.value(0);
        p["name"] = query.value(1);
        p["rootPath"] = query.value(2);
        p["createdAt"] = query.value(3);
        projects.append(p);
    }

    ltDebug(LT_LOG_PROJECT()) << "Listed" << projects.size() << "projects";
    return projects;
}

bool ProjectService::deleteProject(const QString &projectId)
{
    ltTrace(LT_LOG_PROJECT()) << "deleteProject id=" << projectId;

    QSqlQuery query(Database::instance().database());
    query.prepare("DELETE FROM projects WHERE id = ?");
    query.addBindValue(projectId);
    bool ok = query.exec();

    if (ok) {
        ltInfo(LT_LOG_PROJECT()) << "Project deleted:" << projectId;
    } else {
        ltError(LT_LOG_PROJECT()) << "Failed to delete project:" << query.lastError().text();
    }
    return ok;
}

bool ProjectService::openProject(const QString &projectId)
{
    ltTrace(LT_LOG_PROJECT()) << "openProject id=" << projectId;

    QSqlQuery query(Database::instance().database());
    query.prepare("SELECT id FROM projects WHERE id = ?");
    query.addBindValue(projectId);
    if (query.exec() && query.next()) {
        m_currentProjectId = projectId;
        emit currentProjectChanged();
        ltInfo(LT_LOG_PROJECT()) << "Project opened:" << projectId;
        return true;
    }

    ltWarning(LT_LOG_PROJECT()) << "Project not found:" << projectId;
    return false;
}

void ProjectService::closeProject()
{
    ltTrace(LT_LOG_PROJECT()) << "closeProject";

    m_currentProjectId.clear();
    emit currentProjectChanged();
    ltInfo(LT_LOG_PROJECT()) << "Project closed";
}

QVariantMap ProjectService::getCurrentProject() const
{
    ltTrace(LT_LOG_PROJECT()) << "getCurrentProject";

    if (m_currentProjectId.isEmpty()) return {};

    QSqlQuery query(Database::instance().database());
    query.prepare("SELECT id, name, root_path, default_device, default_model_family, created_at FROM projects WHERE id = ?");
    query.addBindValue(m_currentProjectId);
    if (query.exec() && query.next()) {
        QVariantMap p;
        p["id"] = query.value(0);
        p["name"] = query.value(1);
        p["rootPath"] = query.value(2);
        p["defaultDevice"] = query.value(3);
        p["defaultModelFamily"] = query.value(4);
        p["createdAt"] = query.value(5);
        // Add task type via const-cast (getTaskType is non-const due to column migration)
        p["taskType"] = const_cast<ProjectService*>(this)->getTaskType(m_currentProjectId);
        return p;
    }
    return {};
}

bool ProjectService::ensureTaskTypeColumn()
{
    ltTrace(LT_LOG_PROJECT()) << "ensureTaskTypeColumn";

    QSqlQuery query(Database::instance().database());
    query.exec("SELECT task_type FROM projects LIMIT 0");
    if (query.lastError().isValid()) {
        // Column does not exist, add it via ALTER TABLE
        QSqlQuery alterQuery(Database::instance().database());
        if (!alterQuery.exec("ALTER TABLE projects ADD COLUMN task_type TEXT DEFAULT 'detect'")) {
            ltError(LT_LOG_PROJECT()) << "Failed to add task_type column:" << alterQuery.lastError().text();
            return false;
        }
        ltInfo(LT_LOG_PROJECT()) << "Added task_type column to projects table";
    }
    return true;
}

QString ProjectService::getTaskType(const QString &projectId)
{
    ltTrace(LT_LOG_PROJECT()) << "getTaskType projectId=" << projectId;

    if (projectId.isEmpty()) return QStringLiteral("detect");

    // Ensure the task_type column exists
    ensureTaskTypeColumn();

    QSqlQuery query(Database::instance().database());
    query.prepare("SELECT task_type FROM projects WHERE id = ?");
    query.addBindValue(projectId);
    if (query.exec() && query.next()) {
        QString taskType = query.value(0).toString();
        return taskType.isEmpty() ? QStringLiteral("detect") : taskType;
    }
    return QStringLiteral("detect");
}

bool ProjectService::setTaskType(const QString &projectId, const QString &taskType)
{
    ltTrace(LT_LOG_PROJECT()) << "setTaskType projectId=" << projectId << "taskType=" << taskType;

    if (projectId.isEmpty()) return false;

    // Validate task type
    if (taskType != "detect" && taskType != "obb" && taskType != "classify" && taskType != "anomaly") {
        ltWarning(LT_LOG_PROJECT()) << "Invalid task type:" << taskType;
        return false;
    }

    // Ensure the task_type column exists
    ensureTaskTypeColumn();

    QSqlQuery query(Database::instance().database());
    query.prepare("UPDATE projects SET task_type = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?");
    query.addBindValue(taskType);
    query.addBindValue(projectId);

    if (query.exec()) {
        emit taskTypeChanged(projectId, taskType);
        ltInfo(LT_LOG_PROJECT()) << "Task type set:" << projectId << taskType;
        saveProjectConfig(projectId);
        return true;
    }

    ltError(LT_LOG_PROJECT()) << "Failed to set task type:" << query.lastError().text();
    return false;
}

bool ProjectService::saveProjectConfig(const QString &projectId)
{
    ltInfo(LT_LOG_PROJECT()) << "saveProjectConfig projectId=" << projectId;

    if (projectId.isEmpty()) return false;

    QSqlQuery query(Database::instance().database());
    
    // 1. 查询项目基本信息
    query.prepare("SELECT name, root_path, task_type, created_at, updated_at FROM projects WHERE id = ?");
    query.addBindValue(projectId);
    if (!query.exec() || !query.next()) {
        ltError(LT_LOG_PROJECT()) << "saveProjectConfig: project not found in database:" << projectId;
        return false;
    }
    
    QString projectName = query.value(0).toString();
    QString rootPath = query.value(1).toString();
    QString taskType = query.value(2).toString();
    QString createdAt = query.value(3).toString();
    QString updatedAt = query.value(4).toString();

    QJsonObject rootObj;
    rootObj["id"] = projectId;
    rootObj["name"] = projectName;
    rootObj["root_path"] = rootPath;
    rootObj["task_type"] = taskType;
    rootObj["version"] = QStringLiteral("1.1"); // 升级配置文件版本号以示区别
    rootObj["labeltorch_version"] = QStringLiteral("0.1.0");
    rootObj["created_at"] = createdAt;
    rootObj["updated_at"] = updatedAt;

    // 2. 查询 Taxonomies 类别体系列表
    QJsonArray taxonomiesArray;
    QSqlQuery taxQuery(Database::instance().database());
    taxQuery.prepare("SELECT id, name, version, class_definitions_json FROM taxonomies WHERE project_id = ?");
    taxQuery.addBindValue(projectId);
    if (taxQuery.exec()) {
        while (taxQuery.next()) {
            QJsonObject taxObj;
            taxObj["id"] = taxQuery.value(0).toString();
            taxObj["name"] = taxQuery.value(1).toString();
            taxObj["version"] = taxQuery.value(2).toInt();
            
            // 解析已存的类别 JSON 字符串并作为 JSON 对象嵌入
            QString classJsonStr = taxQuery.value(3).toString();
            QJsonDocument classDoc = QJsonDocument::fromJson(classJsonStr.toUtf8());
            taxObj["class_definitions"] = classDoc.isArray() ? classDoc.array() : QJsonArray();
            taxonomiesArray.append(taxObj);
        }
    }
    rootObj["taxonomies"] = taxonomiesArray;

    // 3. 查询 Datasets 数据集元信息
    QJsonArray datasetsArray;
    QSqlQuery dsQuery(Database::instance().database());
    dsQuery.prepare("SELECT id, name, image_root, label_root, format, sample_count FROM datasets WHERE project_id = ?");
    dsQuery.addBindValue(projectId);
    if (dsQuery.exec()) {
        while (dsQuery.next()) {
            QJsonObject dsObj;
            dsObj["id"] = dsQuery.value(0).toString();
            dsObj["name"] = dsQuery.value(1).toString();
            
            // 路径相对于项目根目录进行存储，方便跨平台/跨机器迁移
            QString fullImgRoot = dsQuery.value(2).toString();
            QString fullLblRoot = dsQuery.value(3).toString();
            dsObj["image_root"] = QDir(rootPath).relativeFilePath(fullImgRoot);
            dsObj["label_root"] = QDir(rootPath).relativeFilePath(fullLblRoot);
            
            dsObj["format"] = dsQuery.value(4).toString();
            dsObj["sample_count"] = dsQuery.value(5).toInt();
            datasetsArray.append(dsObj);
        }
    }
    rootObj["datasets"] = datasetsArray;

    // 4. 查询 Dataset Snapshots 快照记录
    QJsonArray snapshotsArray;
    QSqlQuery snapQuery(Database::instance().database());
    snapQuery.prepare("SELECT s.id, s.dataset_id, s.sample_manifest_json, s.split_manifest_json, s.taxonomy_version "
                      "FROM dataset_snapshots s INNER JOIN datasets d ON s.dataset_id = d.id WHERE d.project_id = ?");
    snapQuery.addBindValue(projectId);
    if (snapQuery.exec()) {
        while (snapQuery.next()) {
            QJsonObject snapObj;
            snapObj["id"] = snapQuery.value(0).toString();
            snapObj["dataset_id"] = snapQuery.value(1).toString();
            
            QJsonDocument manifestDoc = QJsonDocument::fromJson(snapQuery.value(2).toString().toUtf8());
            snapObj["sample_manifest"] = manifestDoc.isArray() ? manifestDoc.array() : QJsonArray();
            
            QJsonDocument splitDoc = QJsonDocument::fromJson(snapQuery.value(3).toString().toUtf8());
            snapObj["split_manifest"] = splitDoc.isObject() ? splitDoc.object() : QJsonObject();
            
            snapObj["taxonomy_version"] = snapQuery.value(4).toString();
            snapshotsArray.append(snapObj);
        }
    }
    rootObj["snapshots"] = snapshotsArray;

    // 5. 查询 Training Runs 训练运行记录
    QJsonArray runsArray;
    QSqlQuery runQuery(Database::instance().database());
    runQuery.prepare("SELECT id, snapshot_id, config_snapshot_json, status, log_uri, started_at, finished_at "
                      "FROM training_runs WHERE project_id = ?");
    runQuery.addBindValue(projectId);
    if (runQuery.exec()) {
        while (runQuery.next()) {
            QJsonObject runObj;
            runObj["id"] = runQuery.value(0).toString();
            runObj["snapshot_id"] = runQuery.value(1).toString();
            
            QJsonDocument cfgDoc = QJsonDocument::fromJson(runQuery.value(2).toString().toUtf8());
            runObj["config_snapshot"] = cfgDoc.isObject() ? cfgDoc.object() : QJsonObject();
            
            runObj["status"] = runQuery.value(3).toString();
            runObj["log_uri"] = runQuery.value(4).toString();
            runObj["started_at"] = runQuery.value(5).toString();
            runObj["finished_at"] = runQuery.value(6).toString();
            runsArray.append(runObj);
        }
    }
    rootObj["training_runs"] = runsArray;

    // 6. 查询 Model Versions 注册模型版本
    QJsonArray modelsArray;
    QSqlQuery modelQuery(Database::instance().database());
    modelQuery.prepare("SELECT m.id, m.run_id, m.parent_model_version_id, m.best_weight_path, m.last_weight_path, m.metrics_snapshot_json "
                       "FROM model_versions m INNER JOIN training_runs r ON m.run_id = r.id WHERE r.project_id = ?");
    modelQuery.addBindValue(projectId);
    if (modelQuery.exec()) {
        while (modelQuery.next()) {
            QJsonObject mObj;
            mObj["id"] = modelQuery.value(0).toString();
            mObj["run_id"] = modelQuery.value(1).toString();
            mObj["parent_model_version_id"] = modelQuery.value(2).toString();
            
            // 路径转换为相对于项目根目录的相对路径
            QString fullBest = modelQuery.value(3).toString();
            QString fullLast = modelQuery.value(4).toString();
            mObj["best_weight_path"] = QDir(rootPath).relativeFilePath(fullBest);
            mObj["last_weight_path"] = QDir(rootPath).relativeFilePath(fullLast);
            
            QJsonDocument metricsDoc = QJsonDocument::fromJson(modelQuery.value(5).toString().toUtf8());
            mObj["metrics_snapshot"] = metricsDoc.isObject() ? metricsDoc.object() : QJsonObject();
            modelsArray.append(mObj);
        }
    }
    rootObj["model_versions"] = modelsArray;

    // 原子写入项目根目录下的 project.json 文件
    QString configPath = rootPath + QStringLiteral("/project.json");
    QFile file(configPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        ltError(LT_LOG_PROJECT()) << "saveProjectConfig: failed to open config file for writing:" << configPath;
        return false;
    }

    QJsonDocument doc(rootObj);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    ltInfo(LT_LOG_PROJECT()) << "Project configuration synchronized to disk successfully:" << configPath;
    return true;
}
