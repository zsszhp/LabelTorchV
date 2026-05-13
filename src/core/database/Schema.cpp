#include "Schema.h"
#include "utils/Log.h"

QStringList Schema::createTableStatements()
{
    ltTrace(LT_LOG_DB()) << "Generating create table statements";

    return {
        // 项目表
        "CREATE TABLE IF NOT EXISTS projects ("
        "  id TEXT PRIMARY KEY,"
        "  name TEXT NOT NULL,"
        "  root_path TEXT NOT NULL UNIQUE,"
        "  default_device TEXT DEFAULT 'auto',"
        "  default_model_family TEXT DEFAULT 'yolov8',"
        "  task_type TEXT DEFAULT 'detect',"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  updated_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ")",

        // 类别体系表
        "CREATE TABLE IF NOT EXISTS taxonomies ("
        "  id TEXT PRIMARY KEY,"
        "  project_id TEXT NOT NULL REFERENCES projects(id),"
        "  name TEXT NOT NULL,"
        "  version INTEGER NOT NULL DEFAULT 1,"
        "  class_definitions_json TEXT NOT NULL,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ")",

        // 数据集表
        "CREATE TABLE IF NOT EXISTS datasets ("
        "  id TEXT PRIMARY KEY,"
        "  project_id TEXT NOT NULL REFERENCES projects(id),"
        "  name TEXT NOT NULL,"
        "  image_root TEXT NOT NULL,"
        "  label_root TEXT NOT NULL,"
        "  format TEXT NOT NULL DEFAULT 'yolo_txt',"
        "  sample_count INTEGER DEFAULT 0,"
        "  import_status TEXT NOT NULL DEFAULT 'idle',"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ")",

        // 数据集样本表
        "CREATE TABLE IF NOT EXISTS dataset_samples ("
        "  id TEXT PRIMARY KEY,"
        "  dataset_id TEXT NOT NULL REFERENCES datasets(id),"
        "  image_path TEXT NOT NULL,"
        "  label_path TEXT,"
        "  width INTEGER,"
        "  height INTEGER,"
        "  hash TEXT,"
        "  validation_status TEXT DEFAULT 'valid',"
        "  split TEXT DEFAULT 'train',"
        "  error_code TEXT"
        ")",

        // 导入标签类别表
        "CREATE TABLE IF NOT EXISTS imported_label_schemas ("
        "  id TEXT PRIMARY KEY,"
        "  dataset_id TEXT NOT NULL REFERENCES datasets(id),"
        "  raw_class_names_json TEXT NOT NULL,"
        "  raw_class_order_json TEXT NOT NULL,"
        "  source_format TEXT NOT NULL DEFAULT 'yolo_txt'"
        ")",

        // 类别映射修订表
        "CREATE TABLE IF NOT EXISTS class_mapping_revisions ("
        "  id TEXT PRIMARY KEY,"
        "  dataset_id TEXT NOT NULL REFERENCES datasets(id),"
        "  source_schema_id TEXT NOT NULL REFERENCES imported_label_schemas(id),"
        "  target_taxonomy_id TEXT NOT NULL REFERENCES taxonomies(id),"
        "  mapping_rules_json TEXT NOT NULL,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ")",

        // 标注修订表
        "CREATE TABLE IF NOT EXISTS annotation_revisions ("
        "  id TEXT PRIMARY KEY,"
        "  dataset_id TEXT NOT NULL REFERENCES datasets(id),"
        "  sample_id TEXT NOT NULL REFERENCES dataset_samples(id),"
        "  source_type TEXT NOT NULL DEFAULT 'manual',"
        "  before_snapshot_json TEXT,"
        "  after_snapshot_json TEXT NOT NULL,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ")",

        // 数据快照表
        "CREATE TABLE IF NOT EXISTS dataset_snapshots ("
        "  id TEXT PRIMARY KEY,"
        "  dataset_id TEXT NOT NULL REFERENCES datasets(id),"
        "  sample_manifest_json TEXT NOT NULL,"
        "  split_manifest_json TEXT,"
        "  taxonomy_version TEXT,"
        "  annotation_revision_boundary TEXT,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ")",

        // 训练运行表
        "CREATE TABLE IF NOT EXISTS training_runs ("
        "  id TEXT PRIMARY KEY,"
        "  project_id TEXT NOT NULL REFERENCES projects(id),"
        "  snapshot_id TEXT NOT NULL REFERENCES dataset_snapshots(id),"
        "  config_snapshot_json TEXT NOT NULL,"
        "  runtime_env_snapshot_json TEXT,"
        "  status TEXT NOT NULL DEFAULT 'draft',"
        "  log_uri TEXT,"
        "  started_at DATETIME,"
        "  finished_at DATETIME"
        ")",

        // 模型版本表
        "CREATE TABLE IF NOT EXISTS model_versions ("
        "  id TEXT PRIMARY KEY,"
        "  run_id TEXT NOT NULL REFERENCES training_runs(id),"
        "  parent_model_version_id TEXT REFERENCES model_versions(id),"
        "  best_weight_path TEXT,"
        "  last_weight_path TEXT,"
        "  metrics_snapshot_json TEXT,"
        "  export_registry_json TEXT,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ")",

        // 辅助标注批次表
        "CREATE TABLE IF NOT EXISTS assisted_label_batches ("
        "  id TEXT PRIMARY KEY,"
        "  model_version_id TEXT NOT NULL REFERENCES model_versions(id),"
        "  dataset_id TEXT NOT NULL REFERENCES datasets(id),"
        "  target_sample_scope TEXT NOT NULL,"
        "  conf_threshold REAL NOT NULL DEFAULT 0.25,"
        "  iou_threshold REAL NOT NULL DEFAULT 0.45,"
        "  candidate_snapshot_json TEXT,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ")",

        // 导出产物表
        "CREATE TABLE IF NOT EXISTS export_artifacts ("
        "  id TEXT PRIMARY KEY,"
        "  model_version_id TEXT NOT NULL REFERENCES model_versions(id),"
        "  format TEXT NOT NULL,"
        "  options_snapshot_json TEXT,"
        "  output_path TEXT NOT NULL,"
        "  validation_result TEXT,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ")",

        // 任务事件表
        "CREATE TABLE IF NOT EXISTS task_events ("
        "  id TEXT PRIMARY KEY,"
        "  task_type TEXT NOT NULL,"
        "  task_id TEXT NOT NULL,"
        "  event_type TEXT NOT NULL,"
        "  payload_json TEXT,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ")"
    };
}
