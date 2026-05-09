# src/features/dataset/CMakeLists.txt
# 数据集管理功能模块

qt_add_qml_module(labeltorch_dataset
    URI LabelTorch.Dataset
    STATIC
    QML_FILES
        qml/ImportPage.qml
        qml/ClassMappingPage.qml
        qml/DatasetStatsView.qml
    SOURCES
        DatasetService.h
        DatasetService.cpp
        ImportScanner.h
        ImportScanner.cpp
        ClassMappingService.h
        ClassMappingService.cpp
        DatasetModel.h
        DatasetModel.cpp
)

target_link_libraries(labeltorch_dataset PUBLIC
    labeltorch_core
    Qt6::Quick
)
