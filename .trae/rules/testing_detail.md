---
description: 测试详细规范
globs:
alwaysApply: true
---

# 测试详细规范

## 测试文件对应关系
| 测试文件 | 覆盖模块 |
|----------|----------|
| test_database.cpp | Database, Schema |
| test_labelio.cpp | YoloTxtReader, YoloTxtWriter |
| test_geometry.cpp | AxisAlignedBox, RotatedBox, Polygon |
| test_ipc.cpp | IpcClient, IpcProtocol |
| test_taxonomy.cpp | TaxonomyService, TaxonomyModel |
| test_snapshot.cpp | SnapshotService, SnapshotModel |
| test_training.cpp | TrainingService, TrainingModel |
| test_model.cpp | ModelRegistry, MetricService |
| test_inference.cpp | InferenceService, AnomalyService |
| test_export.cpp | ExportService |

## 测试覆盖三路径
- 正常路径：功能正常工作的场景
- 边界情况：空输入、极大值、零值、空文件等
- 错误路径：文件不存在、格式错误、权限不足等

## 回归防护流程
1. 编写能复现Bug的测试用例（此时测试失败）
2. 修复Bug
3. 确认测试通过
4. 确认原有测试仍然通过（无回归）

## 数据库迁移详细规范
- 迁移脚本位置：src/core/database/migrations/
- 命名格式：M{N}_描述.sql，如M001_add_dataset_hash_column.sql
- 迁移脚本必须幂等（重复执行不报错）
- 应用启动时Database::migrate()自动检测并执行未应用的迁移
- 迁移记录表：schema_migrations（version, applied_at）
- 迁移失败必须回滚当前迁移，不影响已应用的迁移
