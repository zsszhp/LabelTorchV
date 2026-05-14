# src/features/inference/CMakeLists.txt
# 辅助标注/推理功能模块

qt_add_qml_module(labeltorch_inference
    URI LabelTorch.Inference
    STATIC
    QML_FILES
        qml/AssistedLabelPanel.qml
        qml/ReviewDialog.qml
        qml/LowConfQueue.qml
        qml/HardCaseQueue.qml
        qml/ActiveLearningPage.qml
        qml/QueueStatsPanel.qml
    SOURCES
        InferenceService.h
        InferenceService.cpp
        AssistedLabelService.h
        AssistedLabelService.cpp
        ActiveLearningService.h
        ActiveLearningService.cpp
)

target_include_directories(labeltorch_inference PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core/database
    ${CMAKE_SOURCE_DIR}/src/core/ipc
)

target_link_libraries(labeltorch_inference PUBLIC
    labeltorch_core
    Qt6::Quick
)
