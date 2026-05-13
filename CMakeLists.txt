cmake_minimum_required(VERSION 3.21)

project(LabelTorch VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# 自定义CMake模块路径
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Qt 6 基础设置
find_package(Qt6 6.11 REQUIRED COMPONENTS Core Quick QuickControls2 QuickDialogs2 Sql)
qt_standard_project_setup(REQUIRES 6.11)

# 启用 QTP0004 策略以消除 QML 模块 OUTPUT_DIRECTORY 警告
qt_policy(SET QTP0004 NEW)

# QML 模块输出目录，满足 QTP0004 NEW 策略要求
# 每个模块的输出目录自动派生为 ${CMAKE_BINARY_DIR}/${target_path}
# 例如 LabelTorch.Shell -> ${CMAKE_BINARY_DIR}/LabelTorch/Shell
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 子模块
add_subdirectory(src)
enable_testing()
add_subdirectory(tests)
