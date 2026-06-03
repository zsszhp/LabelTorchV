qt_add_qml_module(labeltorch_testing
    URI LabelTorch.Testing
    STATIC
    RESOURCE_PREFIX /qt/qml
    QML_FILES
        qml/TestingPage.qml
    SOURCES
        TestingService.h
        TestingService.cpp
        TestingModel.h
        TestingModel.cpp
)

target_include_directories(labeltorch_testing PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(labeltorch_testing PUBLIC
    labeltorch_core
    labeltorch_theme
    labeltorch_model
    labeltorch_training
    Qt6::Quick
)
