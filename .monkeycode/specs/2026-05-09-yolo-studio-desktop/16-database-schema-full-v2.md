# 数据库 Schema 全量规范 v2

## 1. 设计原则

1. 以领域模型为基础。
2. 以追溯、审计、可恢复为导向。
3. 允许后续通过迁移机制扩展。

## 2. 核心表

1. `projects`
2. `taxonomies`
3. `datasets`
4. `dataset_samples`
5. `imported_label_schemas`
6. `class_mapping_revisions`
7. `annotation_revisions`
8. `dataset_snapshots`
9. `training_runs`
10. `model_versions`
11. `assisted_label_batches`
12. `export_artifacts`
13. `task_events`

## 3. 字段级要求

### 3.1 projects

字段：

1. `id`
2. `name`
3. `root_path`
4. `default_device`
5. `default_model_family`
6. `created_at`
7. `updated_at`

### 3.2 taxonomies

字段：

1. `id`
2. `project_id`
3. `name`
4. `version`
5. `class_definitions_json`
6. `created_at`

### 3.3 datasets

字段：

1. `id`
2. `project_id`
3. `name`
4. `image_root`
5. `label_root`
6. `format`
7. `sample_count`
8. `import_status`
9. `created_at`

### 3.4 dataset_samples

字段：

1. `id`
2. `dataset_id`
3. `image_path`
4. `label_path`
5. `width`
6. `height`
7. `hash`
8. `validation_status`
9. `error_code`

### 3.5 imported_label_schemas

字段：

1. `id`
2. `dataset_id`
3. `raw_class_names_json`
4. `raw_class_order_json`
5. `source_format`

### 3.6 class_mapping_revisions

字段：

1. `id`
2. `dataset_id`
3. `source_schema_id`
4. `target_taxonomy_id`
5. `mapping_rules_json`
6. `created_at`

### 3.7 annotation_revisions

字段：

1. `id`
2. `dataset_id`
3. `sample_id`
4. `source_type`
5. `before_snapshot_json`
6. `after_snapshot_json`
7. `created_at`

### 3.8 dataset_snapshots

字段：

1. `id`
2. `dataset_id`
3. `sample_manifest_json`
4. `split_manifest_json`
5. `taxonomy_version`
6. `annotation_revision_boundary`
7. `created_at`

### 3.9 training_runs

字段：

1. `id`
2. `project_id`
3. `snapshot_id`
4. `config_snapshot_json`
5. `runtime_env_snapshot_json`
6. `status`
7. `log_uri`
8. `started_at`
9. `finished_at`

### 3.10 model_versions

字段：

1. `id`
2. `run_id`
3. `parent_model_version_id`
4. `best_weight_path`
5. `last_weight_path`
6. `metrics_snapshot_json`
7. `export_registry_json`
8. `created_at`

## 4. 审计要求

1. 关键表必须有时间字段。
2. 修订表必须保留 before/after。
3. 任务类表必须可关联事件流。
