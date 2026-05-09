# 模型注册与评估规范 v2

## 1. 目标

模型注册系统负责管理模型版本、实验来源、评估指标、导出物和版本关系，确保任何模型产物可追溯、可比较、可发布。

## 2. 模型版本最小元数据

1. `model_version_id`
2. `run_id`
3. `snapshot_id`
4. `parent_model_version_id`
5. `model_family`
6. `created_at`
7. `best_weight_path`
8. `last_weight_path`
9. `metrics_summary`
10. `exports`

## 3. 版本关系

### 3.1 父子关系

1. 基于历史权重继续训练得到的新模型必须记录父版本。
2. 父子关系用于显示增量训练链。

### 3.2 版本标签

版本可附加标签：

1. `baseline`
2. `best-so-far`
3. `production-candidate`
4. `exported`

## 4. 评估体系

### 4.1 核心指标

1. `mAP50`
2. `mAP50-95`
3. `precision`
4. `recall`
5. `fitness`（若后端提供）

### 4.2 细分指标

成熟版必须支持：

1. 按类别评估。
2. 按目标尺寸评估。
3. 按场景子集评估。

## 5. 模型对比能力

至少应支持：

1. 两个版本的指标对比。
2. 相同数据快照下的横向对比。
3. 同一父版本链下的纵向对比。

## 6. 导出物注册

### 6.1 当前必须支持

1. `pt`
2. `onnx`

### 6.2 后续支持

1. `TensorRT`
2. `OpenVINO`

### 6.3 ONNX 默认参数

1. `opset=13`
2. `dynamic=True`
3. `simplify=True`

### 6.4 导出后验证

1. 导出完成后必须进行格式级基本校验。
2. ONNX 导出后必须至少验证模型可被加载。

## 7. 成熟版要求

1. 支持候选发布版本标记。
2. 支持版本说明与变更摘要。
3. 支持导出物与 Release 产物关联。
