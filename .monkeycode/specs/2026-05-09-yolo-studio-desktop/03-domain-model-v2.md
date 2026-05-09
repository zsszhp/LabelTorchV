# 领域模型定义 v2

## 1. 领域建模目标

本文件定义标炬项目的核心业务对象、对象关系和约束规则。所有数据库设计、服务接口设计、页面交互设计都必须以本领域模型为准。

## 2. 一级领域对象

### 2.1 Project

表示一个独立工业视觉工程空间。

属性：

1. project_id
2. name
3. root_path
4. default_device
5. default_model_family
6. created_at
7. updated_at

约束：

1. Project 是所有数据集、模型版本、导出任务的根容器。
2. 一个 Project 只使用一个项目级类别体系主版本作为默认标准。

### 2.2 Taxonomy

表示项目级标准类别体系。

属性：

1. taxonomy_id
2. project_id
3. name
4. version
5. class_definitions
6. created_at

用途：

1. 为所有导入数据建立统一类别语义基础。
2. 支持未来多来源数据映射。

### 2.3 ImportedLabelSchema

表示某次导入数据原始类别体系。

属性：

1. imported_schema_id
2. dataset_id
3. raw_class_order
4. raw_class_names
5. source_format

### 2.4 ClassMappingRevision

表示某次类别映射与重排记录。

属性：

1. mapping_revision_id
2. dataset_id
3. source_schema_id
4. target_taxonomy_id
5. mapping_rules
6. created_at

约束：

1. 每次类别重排都必须产生新的映射版本。
2. 标签重写必须关联明确的映射版本。

### 2.5 Dataset

表示一次导入后的数据集逻辑实体。

属性：

1. dataset_id
2. project_id
3. name
4. image_root
5. label_root
6. format
7. sample_count
8. import_status

### 2.6 DatasetSample

表示单张图片与其对应标签。

属性：

1. sample_id
2. dataset_id
3. image_path
4. label_path
5. width
6. height
7. hash
8. validation_status

### 2.7 AnnotationRevision

表示一次标注编辑会话后的标签修订结果。

属性：

1. annotation_revision_id
2. dataset_id
3. sample_id
4. source_type
5. before_snapshot
6. after_snapshot
7. changed_by
8. created_at

说明：

1. 即使是单用户环境，也必须保留变更链。
2. source_type 可以是 manual、assisted_confirm、mapping_rewrite。

### 2.8 DatasetSnapshot

表示用于训练的一次数据快照。

属性：

1. snapshot_id
2. dataset_id
3. sample_manifest
4. split_manifest
5. taxonomy_version
6. annotation_revision_boundary
7. created_at

约束：

1. 训练任务必须使用 DatasetSnapshot，而不是直接指向 Dataset。
2. 训练结束后必须能复原该快照内容。

### 2.9 TrainConfiguration

表示一次训练任务的配置快照。

属性：

1. model_family
2. model_variant
3. img_size
4. batch
5. epochs
6. patience
7. device
8. workers
9. amp
10. resume
11. pretrained_weights
12. export_defaults

### 2.10 TrainingRun

表示一次训练执行实例。

属性：

1. run_id
2. project_id
3. snapshot_id
4. config_snapshot
5. runtime_env_snapshot
6. status
7. started_at
8. finished_at
9. log_uri

### 2.11 ModelVersion

表示一版模型产物。

属性：

1. model_version_id
2. run_id
3. parent_model_version_id
4. best_weight_path
5. last_weight_path
6. metrics_snapshot
7. export_registry
8. created_at

### 2.12 AssistedLabelBatch

表示一次模型辅助标注推理结果批次。

属性：

1. batch_id
2. model_version_id
3. dataset_id
4. target_sample_scope
5. conf_threshold
6. iou_threshold
7. candidate_snapshot
8. created_at

### 2.13 ExportArtifact

表示一次模型导出结果。

属性：

1. export_id
2. model_version_id
3. format
4. options_snapshot
5. output_path
6. validation_result
7. created_at

## 3. 对象关系

1. Project 1..n Dataset
2. Project 1..n Taxonomy
3. Dataset 1..n DatasetSample
4. Dataset 1..n ClassMappingRevision
5. Dataset 1..n AnnotationRevision
6. Dataset 1..n DatasetSnapshot
7. DatasetSnapshot 1..n TrainingRun
8. TrainingRun 1..1 ModelVersion
9. ModelVersion 1..n ExportArtifact
10. ModelVersion 1..n AssistedLabelBatch

## 4. 领域强约束

1. 训练永远依赖数据快照，不直接依赖当前数据集实时状态。
2. 模型版本必须可追溯到训练任务和数据快照。
3. 类别映射必须版本化。
4. 标签修订必须记录前后差异。
5. 导出物必须登记并校验。
