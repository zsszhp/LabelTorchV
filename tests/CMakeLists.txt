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
target_include_directories(test_labelio PRIVATE
    ${CMAKE_SOURCE_DIR}/src/features/annotation
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
target_include_directories(test_geometry PRIVATE
    ${CMAKE_SOURCE_DIR}/src/features/annotation
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
target_include_directories(test_taxonomy PRIVATE
    ${CMAKE_SOURCE_DIR}/src/features/project
    ${CMAKE_SOURCE_DIR}/src/core/database
    ${CMAKE_SOURCE_DIR}/src/core/filesystem
    ${CMAKE_SOURCE_DIR}/src/core/utils
)
add_test(NAME TaxonomyTest COMMAND test_taxonomy)
