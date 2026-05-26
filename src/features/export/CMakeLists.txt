# src/features/export/CMakeLists.txt
# 导出中心功能模块

qt_add_qml_module(labeltorch_export
    URI LabelTorch.Export
    STATIC
    RESOURCE_PREFIX /qt/qml
    QML_FILES
        qml/ExportPage.qml
        qml/OnnxConfigPanel.qml
    SOURCES
        ExportService.h
        ExportService.cpp
)

target_include_directories(labeltorch_export PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(labeltorch_export PUBLIC
    labeltorch_core
    labeltorch_theme
    Qt6::Quick
)
