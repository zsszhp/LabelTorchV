# src/features/training/CMakeLists.txt
# 训练编排功能模块

qt_add_qml_module(labeltorch_training
    URI LabelTorch.Training
    STATIC
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

target_link_libraries(labeltorch_training PUBLIC
    labeltorch_core
    Qt6::Quick
)

target_include_directories(labeltorch_training PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core/database
)
