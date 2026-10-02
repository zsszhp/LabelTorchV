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

        // 类别样式（颜色/快捷键）：class_definitions_json 只存名称数组，样式单独存储
        "CREATE TABLE IF NOT EXISTS taxonomy_class_styles ("
        "  taxonomy_id TEXT NOT NULL REFERENCES taxonomies(id),"
        "  class_index INTEGER NOT NULL,"
        "  color TEXT,"
        "  shortcut TEXT,"
        "  PRIMARY KEY (taxonomy_id, class_index)"
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
        "  failure_info_json TEXT,"
        "  started_at DATETIME,"
        "  finished_at DATETIME"
        ")",

        // 模型版本表（支持训练产出和外部导入两种来源）
        "CREATE TABLE IF NOT EXISTS model_versions ("
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
        "  status TEXT NOT NULL DEFAULT 'pending',"
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
        ")",

        // 训练指标表（每 epoch 记录）
        "CREATE TABLE IF NOT EXISTS run_metrics ("
        "  id TEXT PRIMARY KEY,"
        "  run_id TEXT NOT NULL REFERENCES training_runs(id),"
        "  epoch INTEGER NOT NULL,"
        "  metric_name TEXT NOT NULL,"
        "  metric_value REAL NOT NULL,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ")",

        // 测试运行表
        "CREATE TABLE IF NOT EXISTS testing_runs ("
        "  id TEXT PRIMARY KEY,"
        "  project_id TEXT NOT NULL REFERENCES projects(id),"
        "  model_version_id TEXT NOT NULL REFERENCES model_versions(id),"
        "  snapshot_id TEXT NOT NULL REFERENCES dataset_snapshots(id),"
        "  config_json TEXT NOT NULL,"
        "  status TEXT NOT NULL DEFAULT 'draft',"
        "  metrics_json TEXT,"
        "  confusion_matrix_json TEXT,"
        "  pr_curve_json TEXT,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  started_at DATETIME,"
        "  finished_at DATETIME"
        ")",

        // 数据集标签表（A6：用于数据集分类与筛选）
        // builtin=1 为内置评审 Tag（默认/良品/漏检/误检/待定/重要），不可改名/删除
        "CREATE TABLE IF NOT EXISTS dataset_tags ("
        "  id TEXT PRIMARY KEY,"
        "  dataset_id TEXT NOT NULL REFERENCES datasets(id) ON DELETE CASCADE,"
        "  name TEXT NOT NULL,"
        "  shortcut TEXT,"
        "  builtin INTEGER NOT NULL DEFAULT 0,"
        "  created_at TEXT NOT NULL,"
        "  UNIQUE(dataset_id, name)"
        ")",

        // 主动学习队列持久化表（P1-14：重启不丢）
        // queue_type: low-confidence / false-positive / false-negative / hard-case
        // status: queued / reviewed / discarded
        "CREATE TABLE IF NOT EXISTS active_learning_items ("
        "  id TEXT PRIMARY KEY,"
        "  queue_type TEXT NOT NULL,"
        "  sample_path TEXT NOT NULL,"
        "  sample_id TEXT,"
        "  dataset_id TEXT,"
        "  project_id TEXT,"
        "  reason TEXT,"
        "  priority INTEGER DEFAULT 0,"
        "  confidence REAL DEFAULT 0.0,"
        "  class_index INTEGER,"
        "  class_name TEXT,"
        "  payload_json TEXT,"
        "  status TEXT NOT NULL DEFAULT 'queued',"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  updated_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ")",
        "CREATE INDEX IF NOT EXISTS idx_al_items_queue ON active_learning_items(queue_type, status)"
    };
}

QStringList Schema::createIndexStatements()
{
    ltTrace(LT_LOG_DB()) << "Generating create index statements";

    // P1-23：按实际查询路径建索引
    // - listSamples / getSampleStats：WHERE dataset_id = ? ORDER BY image_path
    // - hash 去重与数据泄漏检测：WHERE dataset_id = ? AND hash ...
    // - split 划分查询：WHERE dataset_id = ? AND split = ?
    // - 修订审计：WHERE dataset_id / sample_id
    // - 任务事件审计：WHERE task_type / task_id
    // - 训练指标：WHERE run_id（每 epoch 多条）
    // - 训练列表/血缘：WHERE project_id / snapshot_id / run_id
    return {
        "CREATE INDEX IF NOT EXISTS idx_dataset_samples_dataset_id "
        "ON dataset_samples(dataset_id, image_path)",

        "CREATE INDEX IF NOT EXISTS idx_dataset_samples_hash "
        "ON dataset_samples(hash)",

        "CREATE INDEX IF NOT EXISTS idx_dataset_samples_split "
        "ON dataset_samples(dataset_id, split)",

        "CREATE INDEX IF NOT EXISTS idx_annot_rev_dataset "
        "ON annotation_revisions(dataset_id, created_at)",

        "CREATE INDEX IF NOT EXISTS idx_annot_rev_sample "
        "ON annotation_revisions(sample_id)",

        "CREATE INDEX IF NOT EXISTS idx_task_events_task "
        "ON task_events(task_type, task_id)",

        "CREATE INDEX IF NOT EXISTS idx_task_events_created "
        "ON task_events(created_at)",

        "CREATE INDEX IF NOT EXISTS idx_run_metrics_run "
        "ON run_metrics(run_id, epoch)",

        "CREATE INDEX IF NOT EXISTS idx_training_runs_project "
        "ON training_runs(project_id, started_at)",

        "CREATE INDEX IF NOT EXISTS idx_training_runs_snapshot "
        "ON training_runs(snapshot_id)",

        "CREATE INDEX IF NOT EXISTS idx_model_versions_run "
        "ON model_versions(run_id)",

        "CREATE INDEX IF NOT EXISTS idx_model_versions_project "
        "ON model_versions(project_id)",

        "CREATE INDEX IF NOT EXISTS idx_datasets_project "
        "ON datasets(project_id)",

        "CREATE INDEX IF NOT EXISTS idx_taxonomies_project "
        "ON taxonomies(project_id)",

        "CREATE INDEX IF NOT EXISTS idx_dataset_snapshots_dataset "
        "ON dataset_snapshots(dataset_id)",

        "CREATE INDEX IF NOT EXISTS idx_export_artifacts_model "
        "ON export_artifacts(model_version_id)"
    };
}
