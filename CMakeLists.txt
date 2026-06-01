cmake_minimum_required(VERSION 3.22.1)

set(EXECUTABLE_NAME LabelTorchV)

project(${EXECUTABLE_NAME} VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_DISABLE_FIND_PACKAGE_WrapVulkanHeaders ON)

add_definitions(-DWIN32_LEAN_AND_MEAN)

# Custom CMake module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

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
