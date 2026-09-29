# tests/CMakeLists.txt
# C++ 测试

enable_testing()

find_package(Qt6 REQUIRED COMPONENTS Test Sql)

# 数据库测试
add_executable(test_database
    test_database.cpp
)
target_link_libraries(test_database PRIVATE
    labeltorch_core
    labeltorch_dataset
    labeltorch_project
    Qt6::Test
    Qt6::Sql
)
add_test(NAME DatabaseTest COMMAND test_database)

# 标签IO测试
add_executable(test_labelio
    test_labelio.cpp
)
target_link_libraries(test_labelio PRIVATE
    labeltorch_core
    labeltorch_annotation
    Qt6::Test
)
add_test(NAME LabelIOTest COMMAND test_labelio)

# 几何内核测试
add_executable(test_geometry
    test_geometry.cpp
)
target_link_libraries(test_geometry PRIVATE
    labeltorch_core
    labeltorch_annotation
    Qt6::Test
)
add_test(NAME GeometryTest COMMAND test_geometry)

# IPC测试
add_executable(test_ipc
    test_ipc.cpp
)
target_link_libraries(test_ipc PRIVATE
    labeltorch_core
    Qt6::Test
)
add_test(NAME IpcTest COMMAND test_ipc)

# 类别体系测试
add_executable(test_taxonomy
    test_taxonomy.cpp
)
target_link_libraries(test_taxonomy PRIVATE
    labeltorch_core
    labeltorch_project
    Qt6::Test
    Qt6::Sql
)
add_test(NAME TaxonomyTest COMMAND test_taxonomy)

# 快照测试
add_executable(test_snapshot
    test_snapshot.cpp
)
target_link_libraries(test_snapshot PRIVATE
    labeltorch_core
    labeltorch_training
    Qt6::Test
    Qt6::Sql
)
add_test(NAME SnapshotTest COMMAND test_snapshot)

# 训练服务测试
add_executable(test_training
    test_training.cpp
)
target_link_libraries(test_training PRIVATE
    labeltorch_core
    labeltorch_training
    Qt6::Test
    Qt6::Sql
)
add_test(NAME TrainingTest COMMAND test_training)

# 模型服务测试
add_executable(test_model
    test_model.cpp
)
target_link_libraries(test_model PRIVATE
    labeltorch_core
    labeltorch_model
    Qt6::Test
    Qt6::Sql
)
add_test(NAME ModelTest COMMAND test_model)

# 推理/辅助标注服务测试
add_executable(test_inference
    test_inference.cpp
)
target_link_libraries(test_inference PRIVATE
    labeltorch_core
    labeltorch_inference
    labeltorch_model
    Qt6::Test
    Qt6::Sql
)
add_test(NAME InferenceTest COMMAND test_inference)

# 导出服务测试
add_executable(test_export
    test_export.cpp
)
target_link_libraries(test_export PRIVATE
    labeltorch_core
    labeltorch_export
    labeltorch_model
    Qt6::Test
    Qt6::Sql
)
add_test(NAME ExportTest COMMAND test_export)

# 分类功能端到端测试
add_executable(test_classify
    test_classify.cpp
)
target_link_libraries(test_classify PRIVATE
    labeltorch_core
    labeltorch_dataset
    labeltorch_training
    Qt6::Test
    Qt6::Sql
)
add_test(NAME ClassifyTest COMMAND test_classify)

# ========== P0 稳定性回归测试 ==========

# P0-1 快照哈希冻结
add_executable(test_snapshot_hash
    test_snapshot_hash.cpp
)
target_link_libraries(test_snapshot_hash PRIVATE
    labeltorch_core
    labeltorch_training
    Qt6::Test
    Qt6::Sql
    Qt6::Gui
)
add_test(NAME SnapshotHashTest COMMAND test_snapshot_hash)

# P0-2 类别删除防错位
add_executable(test_taxonomy_delete
    test_taxonomy_delete.cpp
)
target_link_libraries(test_taxonomy_delete PRIVATE
    labeltorch_core
    labeltorch_project
    Qt6::Test
    Qt6::Sql
)
add_test(NAME TaxonomyDeleteTest COMMAND test_taxonomy_delete)

# P0-3 导出验证闭环
add_executable(test_export_verify
    test_export_verify.cpp
)
target_link_libraries(test_export_verify PRIVATE
    labeltorch_core
    labeltorch_export
    labeltorch_model
    Qt6::Test
    Qt6::Sql
)
add_test(NAME ExportVerifyTest COMMAND test_export_verify)

# P0-4 标签原子写入
add_executable(test_labelio_atomic
    test_labelio_atomic.cpp
)
target_link_libraries(test_labelio_atomic PRIVATE
    labeltorch_core
    labeltorch_annotation
    Qt6::Test
)
add_test(NAME LabelIOAtomicTest COMMAND test_labelio_atomic)

# P0-5 项目删除级联
add_executable(test_project_cascade
    test_project_cascade.cpp
)
target_link_libraries(test_project_cascade PRIVATE
    labeltorch_core
    labeltorch_project
    Qt6::Test
    Qt6::Sql
)
add_test(NAME ProjectCascadeTest COMMAND test_project_cascade)

# P0-6 hash 落库与去重
add_executable(test_dataset_hash
    test_dataset_hash.cpp
)
target_link_libraries(test_dataset_hash PRIVATE
    labeltorch_core
    labeltorch_dataset
    Qt6::Test
    Qt6::Sql
    Qt6::Gui
)
add_test(NAME DatasetHashTest COMMAND test_dataset_hash)

# P0-8 后端进程重启计数/退避
add_executable(test_ipc_restart
    test_ipc_restart.cpp
)
target_link_libraries(test_ipc_restart PRIVATE
    labeltorch_core
    Qt6::Test
)
add_test(NAME IpcRestartTest COMMAND test_ipc_restart)

# P0-11 ThumbnailGenerator 生命周期
add_executable(test_thumbnail_lifecycle
    test_thumbnail_lifecycle.cpp
)
target_link_libraries(test_thumbnail_lifecycle PRIVATE
    labeltorch_core
    Qt6::Test
    Qt6::Gui
)
add_test(NAME ThumbnailLifecycleTest COMMAND test_thumbnail_lifecycle)

# P1-C 主工作流闭环：辅助标注回写/主动学习落库/训练失败诊断/评估透传/导出交付目录
add_executable(test_p1_workflow
    test_p1_workflow.cpp
)
target_link_libraries(test_p1_workflow PRIVATE
    labeltorch_core
    labeltorch_annotation
    labeltorch_inference
    labeltorch_training
    labeltorch_model
    labeltorch_testing
    labeltorch_export
    Qt6::Test
    Qt6::Sql
)
add_test(NAME P1WorkflowTest COMMAND test_p1_workflow)

# P1-21 ThumbnailCache 字节限容
add_executable(test_thumbnail_cache
    test_thumbnail_cache.cpp
)
target_link_libraries(test_thumbnail_cache PRIVATE
    labeltorch_core
    Qt6::Test
    Qt6::Gui
)
add_test(NAME ThumbnailCacheTest COMMAND test_thumbnail_cache)

# P1-23 数据库索引
add_executable(test_perf_indexes
    test_perf_indexes.cpp
)
target_link_libraries(test_perf_indexes PRIVATE
    labeltorch_core
    Qt6::Test
    Qt6::Sql
    Qt6::Gui
)
add_test(NAME PerfIndexesTest COMMAND test_perf_indexes)

# P2-12 审计落地 + 指标历史双路兼容
add_executable(test_audit_log
    test_audit_log.cpp
)
target_link_libraries(test_audit_log PRIVATE
    labeltorch_core
    labeltorch_project
    labeltorch_dataset
    labeltorch_training
    labeltorch_model
    Qt6::Test
    Qt6::Sql
)
add_test(NAME AuditLogTest COMMAND test_audit_log)
