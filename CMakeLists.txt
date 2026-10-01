cmake_minimum_required(VERSION 3.22.1)

set(EXECUTABLE_NAME LabelTorchV)

# 版本单一源：所有对外版本号（安装器 / 后端包 / 文档 / 绿色包）以本行为准，
# 由 scripts/sync_version.ps1 同步。升级版本时只改这里。
project(${EXECUTABLE_NAME} VERSION 0.2.1 LANGUAGES CXX)

# MSVC：源码统一按 UTF-8 读取（本仓库全部源文件为无 BOM UTF-8），
# 否则中文字面量被按系统码页(GBK)解释，QString(const char*) 出现乱码。
# 版本单一源：APP_VERSION_* 以 project(VERSION) 注入，供全部子目录目标使用
if(MSVC)
    add_compile_options(/utf-8)
    add_compile_definitions(
        APP_VERSION_STR="${PROJECT_VERSION}"
        APP_VERSION_WSTR=L"${PROJECT_VERSION}"
    )
endif()

# Qt 路径覆盖：preset 的 CMAKE_PREFIX_PATH 可来自 QT_ROOT 环境变量；
# 为空时回退到本机常见安装位置，亦可用 -DCMAKE_PREFIX_PATH= 直接指定。
if(NOT CMAKE_PREFIX_PATH)
    if(DEFINED ENV{QT_ROOT} AND NOT "$ENV{QT_ROOT}" STREQUAL "")
        file(TO_CMAKE_PATH "$ENV{QT_ROOT}" CMAKE_PREFIX_PATH)
    elseif(DEFINED ENV{QT_MINGW_ROOT} AND NOT "$ENV{QT_MINGW_ROOT}" STREQUAL "")
        file(TO_CMAKE_PATH "$ENV{QT_MINGW_ROOT}" CMAKE_PREFIX_PATH)
    elseif(EXISTS "C:/Qt/6.11.1/msvc2022_64")
        set(CMAKE_PREFIX_PATH "C:/Qt/6.11.1/msvc2022_64")
    elseif(EXISTS "C:/Qt/6.11.1/mingw_64")
        set(CMAKE_PREFIX_PATH "C:/Qt/6.11.1/mingw_64")
    endif()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_DISABLE_FIND_PACKAGE_WrapVulkanHeaders ON)

add_definitions(-DWIN32_LEAN_AND_MEAN)

# Custom CMake module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# 解析 Python 环境位置（支持 -DLABELTORCH_PYTHON_ENV / 环境变量覆盖），
# 结果写入缓存供打包脚本与后续自定义命令复用
include(FindPythonEnv)

# Qt 6 core setup
find_package(Qt6 6.11 REQUIRED COMPONENTS Core Quick QuickControls2 QuickDialogs2 Sql Concurrent)
qt_standard_project_setup(REQUIRES 6.11)

# Enable QTP0004 policy to suppress QML module OUTPUT_DIRECTORY warnings
qt_policy(SET QTP0004 NEW)

# QML module output directory - each module output is derived as ${CMAKE_BINARY_DIR}/${target_path}
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Submodules
add_subdirectory(src)
enable_testing()
add_subdirectory(tests)
