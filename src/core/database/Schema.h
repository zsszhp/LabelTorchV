#ifndef SCHEMA_H
#define SCHEMA_H

#include <QStringList>

/**
 * @brief 数据库DDL常量定义
 *
 * 定义17张核心表的建表语句
 */
class Schema
{
public:
    static QStringList createTableStatements();

    /// P1-23：核心查询路径索引（幂等创建，随 initializeSchema 一并执行）
    static QStringList createIndexStatements();

    // 表名常量
    static constexpr const char *PROJECTS = "projects";
    static constexpr const char *TAXONOMIES = "taxonomies";
    static constexpr const char *DATASETS = "datasets";
    static constexpr const char *DATASET_SAMPLES = "dataset_samples";
    static constexpr const char *IMPORTED_LABEL_SCHEMAS = "imported_label_schemas";
    static constexpr const char *CLASS_MAPPING_REVISIONS = "class_mapping_revisions";
    static constexpr const char *ANNOTATION_REVISIONS = "annotation_revisions";
    static constexpr const char *DATASET_SNAPSHOTS = "dataset_snapshots";
    static constexpr const char *TRAINING_RUNS = "training_runs";
    static constexpr const char *MODEL_VERSIONS = "model_versions";
    static constexpr const char *ASSISTED_LABEL_BATCHES = "assisted_label_batches";
    static constexpr const char *EXPORT_ARTIFACTS = "export_artifacts";
    static constexpr const char *TASK_EVENTS = "task_events";
    static constexpr const char *RUN_METRICS = "run_metrics";
    static constexpr const char *TESTING_RUNS = "testing_runs";
    static constexpr const char *DATASET_TAGS = "dataset_tags";  ///< 数据集标签表（A6）
    static constexpr const char *ACTIVE_LEARNING_ITEMS = "active_learning_items";  ///< 主动学习队列持久化表（P1-14）
};

#endif // SCHEMA_H
