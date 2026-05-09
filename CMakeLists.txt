cmake_minimum_required(VERSION 3.21)

project(LabelTorch VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# 自定义CMake模块路径
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Qt 6 基础设置
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Quick QuickControls2 QuickDialogs2 Sql)
qt_standard_project_setup(REQUIRES 6.5)

# 子模块
add_subdirectory(src)
enable_testing()
add_subdirectory(tests)
