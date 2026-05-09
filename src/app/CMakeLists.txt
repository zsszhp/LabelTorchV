# src/app/CMakeLists.txt
# 应用入口

qt_add_executable(labeltorch
    main.cpp
    AppController.h
    AppController.cpp
)

target_link_libraries(labeltorch PRIVATE
    labeltorch_core
    labeltorch_shell
    labeltorch_project
    labeltorch_dataset
    labeltorch_annotation
    labeltorch_training
    labeltorch_model
    labeltorch_inference
    labeltorch_export
    Qt6::Quick
    Qt6::QuickControls2
)

target_include_directories(labeltorch PRIVATE
    ${CMAKE_SOURCE_DIR}/src/features/project
    ${CMAKE_SOURCE_DIR}/src/features/dataset
    ${CMAKE_SOURCE_DIR}/src/features/annotation
    ${CMAKE_SOURCE_DIR}/src/features/annotation/canvas
    ${CMAKE_SOURCE_DIR}/src/features/training
    ${CMAKE_SOURCE_DIR}/src/features/model
    ${CMAKE_SOURCE_DIR}/src/core/database
    ${CMAKE_SOURCE_DIR}/src/core/utils
)
if(WIN32)
    set_target_properties(labeltorch PROPERTIES
        WIN32_EXECUTABLE ON
    )
endif()
