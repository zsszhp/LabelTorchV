qt_add_executable(${EXECUTABLE_NAME}
    main.cpp
    AppController.h
    AppController.cpp
)

target_link_libraries(${EXECUTABLE_NAME} PRIVATE
    labeltorch_core
    labeltorch_shell
    labeltorch_shellplugin
    labeltorch_project
    labeltorch_projectplugin
    labeltorch_dataset
    labeltorch_datasetplugin
    labeltorch_annotation
    labeltorch_annotationplugin
    labeltorch_training
    labeltorch_trainingplugin
    labeltorch_model
    labeltorch_modelplugin
    labeltorch_inference
    labeltorch_inferenceplugin
    labeltorch_export
    labeltorch_exportplugin
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickDialogs2
    Qt6::Sql
    dbghelp
)

target_include_directories(${EXECUTABLE_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src/features/project
    ${CMAKE_SOURCE_DIR}/src/features/dataset
    ${CMAKE_SOURCE_DIR}/src/features/annotation
    ${CMAKE_SOURCE_DIR}/src/features/annotation/canvas
    ${CMAKE_SOURCE_DIR}/src/features/training
    ${CMAKE_SOURCE_DIR}/src/features/model
    ${CMAKE_SOURCE_DIR}/src/features/inference
    ${CMAKE_SOURCE_DIR}/src/features/export
    ${CMAKE_SOURCE_DIR}/src/core/database
    ${CMAKE_SOURCE_DIR}/src/core/utils
)

if(WIN32)
    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE NOMINMAX)
    target_compile_options(${EXECUTABLE_NAME} PRIVATE /FS /MP)

    # Release构建使用WIN32子系统（无控制台窗口），Debug构建保留控制台以便调试
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set_target_properties(${EXECUTABLE_NAME} PROPERTIES
            WIN32_EXECUTABLE ON
        )
    endif()

    # windeployqt — automatically copy Qt runtime DLLs next to the executable
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin")

    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                --no-opengl-sw
                --qmldir "${CMAKE_SOURCE_DIR}/src/shell/qml"
                --qmldir "${CMAKE_SOURCE_DIR}/src/features/project/qml"
                --qmldir "${CMAKE_SOURCE_DIR}/src/features/dataset/qml"
                --qmldir "${CMAKE_SOURCE_DIR}/src/features/annotation/qml"
                --qmldir "${CMAKE_SOURCE_DIR}/src/features/training/qml"
                --qmldir "${CMAKE_SOURCE_DIR}/src/features/model/qml"
                --qmldir "${CMAKE_SOURCE_DIR}/src/features/inference/qml"
                --qmldir "${CMAKE_SOURCE_DIR}/src/features/export/qml"
                "$<TARGET_FILE:${EXECUTABLE_NAME}>"
            COMMENT "Running windeployqt to deploy Qt DLLs, MSVC runtime and QML modules..."
        )
    else()
        message(WARNING "windeployqt not found - Qt runtime DLLs will not be deployed")
    endif()

    # 手动复制MSVC运行时DLL（windeployqt --compiler-runtime在Debug模式下可能不部署）
    if(MSVC)
        get_filename_component(MSVC_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
        set(MSVC_RUNTIME_DLLS
            vcruntime140.dll
            vcruntime140_1.dll
            msvcp140.dll
            msvcp140_1.dll
            msvcp140_2.dll
            msvcp140_codecvt_ids.dll
        )
        foreach(DLL ${MSVC_RUNTIME_DLLS})
            if(EXISTS "${MSVC_BIN_DIR}/${DLL}")
                add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
                    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                        "${MSVC_BIN_DIR}/${DLL}"
                        "$<TARGET_FILE_DIR:${EXECUTABLE_NAME}>"
                    COMMENT "Copying MSVC runtime: ${DLL}"
                )
            endif()
        endforeach()
    endif()
endif()
