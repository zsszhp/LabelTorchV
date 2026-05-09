# src/features/export/CMakeLists.txt
# 导出中心功能模块

qt_add_qml_module(labeltorch_export
    URI LabelTorch.Export
    STATIC
    QML_FILES
        qml/ExportPage.qml
        qml/OnnxConfigPanel.qml
    SOURCES
        ExportService.h
        ExportService.cpp
)

target_link_libraries(labeltorch_export PUBLIC
    labeltorch_core
    Qt6::Quick
)

target_include_directories(labeltorch_export PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core/database
)
