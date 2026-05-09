# src/features/annotation/CMakeLists.txt
# 标注引擎功能模块

qt_add_qml_module(labeltorch_annotation
    URI LabelTorch.Annotation
    STATIC
    QML_FILES
        qml/AnnotationPage.qml
        qml/AnnotCanvas.qml
        qml/ClassPanel.qml
        qml/SampleList.qml
    SOURCES
        geometry/Geometry.h
        geometry/AxisAlignedBox.h
        geometry/RotatedBox.h
        geometry/RotatedBox.cpp
        geometry/Polygon.h
        canvas/CanvasController.h
        canvas/CanvasController.cpp
        canvas/RenderLayer.h
        canvas/InteractionManager.h
        canvas/InteractionManager.cpp
        labelio/YoloTxtReader.h
        labelio/YoloTxtReader.cpp
        labelio/YoloTxtWriter.h
        labelio/YoloTxtWriter.cpp
        AnnotationService.h
        AnnotationService.cpp
        AnnotationModel.h
        AnnotationModel.cpp
)

target_link_libraries(labeltorch_annotation PUBLIC
    labeltorch_core
    Qt6::Quick
    Qt6::Sql
)
