# src/features/project/CMakeLists.txt
# 项目管理功能模块

qt_add_qml_module(labeltorch_project
    URI LabelTorch.Project
    STATIC
    QML_FILES
        qml/ProjectPage.qml
        qml/ProjectCard.qml
        qml/TaxonomyPage.qml
    SOURCES
        ProjectService.h
        ProjectService.cpp
        TaxonomyService.h
        TaxonomyService.cpp
        ProjectModel.h
        ProjectModel.cpp
        TaxonomyModel.h
        TaxonomyModel.cpp
)

target_link_libraries(labeltorch_project PUBLIC
    labeltorch_core
    Qt6::Quick
    Qt6::QuickDialogs2
)
