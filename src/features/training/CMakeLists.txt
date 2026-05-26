# src/features/training/CMakeLists.txt
# 训练编排功能模块

qt_add_qml_module(labeltorch_training
    URI LabelTorch.Training
    STATIC
    RESOURCE_PREFIX /qt/qml
    QML_FILES
        qml/TrainingPage.qml
        qml/ConfigPanel.qml
        qml/LogView.qml
        qml/SnapshotPage.qml
    SOURCES
        TrainingService.h
        TrainingService.cpp
        SnapshotService.h
        SnapshotService.cpp
        SnapshotModel.h
        SnapshotModel.cpp
        TrainingModel.h
        TrainingModel.cpp
)

target_include_directories(labeltorch_training PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(labeltorch_training PUBLIC
    labeltorch_core
    labeltorch_theme
    labeltorch_model
    Qt6::Quick
)
