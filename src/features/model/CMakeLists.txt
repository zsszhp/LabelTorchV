# src/features/model/CMakeLists.txt
# 模型注册与评估功能模块

qt_add_qml_module(labeltorch_model
    URI LabelTorch.Model
    STATIC
    QML_FILES
        qml/ModelPage.qml
        qml/MetricChart.qml
        qml/ComparePage.qml
    SOURCES
        ModelRegistry.h
        ModelRegistry.cpp
        MetricService.h
        MetricService.cpp
        ModelVersionModel.h
        ModelVersionModel.cpp
)

target_link_libraries(labeltorch_model PUBLIC
    labeltorch_core
    Qt6::Quick
)

target_include_directories(labeltorch_model PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core/database
)
