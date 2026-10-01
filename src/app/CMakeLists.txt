qt_add_executable(${EXECUTABLE_NAME}
    main.cpp
    AppController.h
    AppController.cpp
    DemoBootstrap.h
    DemoBootstrap.cpp
)

# Windows应用图标资源（.rc文件嵌入exe，资源管理器/任务栏显示）
if(WIN32)
    target_sources(${EXECUTABLE_NAME} PRIVATE app.rc)
endif()

# Qt资源文件（QML窗口图标，多尺寸支持高DPI）
qt_add_resources(${EXECUTABLE_NAME} app_icons
    PREFIX /
    FILES
        icons/labeltorch.png
        icons/labeltorch_16x16.png
        icons/labeltorch_24x24.png
        icons/labeltorch_32x32.png
        icons/labeltorch_48x48.png
        icons/labeltorch_64x64.png
        icons/labeltorch_128x128.png
        icons/labeltorch_256x256.png
)

# 演示模式：内置合成缺陷示例数据集（scripts/gen_demo_data.py 生成）
file(GLOB LT_DEMO_IMAGES ${CMAKE_CURRENT_SOURCE_DIR}/../../assets/demo/images/*.jpg)
file(GLOB LT_DEMO_LABELS ${CMAKE_CURRENT_SOURCE_DIR}/../../assets/demo/labels/*.txt)
foreach(f ${LT_DEMO_IMAGES})
    get_filename_component(name ${f} NAME)
    set_source_files_properties(${f} PROPERTIES QT_RESOURCE_ALIAS images/${name})
    list(APPEND LT_DEMO_FILES ${f})
endforeach()
foreach(f ${LT_DEMO_LABELS})
    get_filename_component(name ${f} NAME)
    set_source_files_properties(${f} PROPERTIES QT_RESOURCE_ALIAS labels/${name})
    list(APPEND LT_DEMO_FILES ${f})
endforeach()
qt_add_resources(${EXECUTABLE_NAME} demo_assets
    PREFIX /demo
    FILES ${LT_DEMO_FILES}
)

target_link_libraries(${EXECUTABLE_NAME} PRIVATE
    labeltorch_core
    labeltorch_theme
    labeltorch_themeplugin
    labeltorch_components
    labeltorch_componentsplugin
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
    labeltorch_testing
    labeltorch_testingplugin
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickDialogs2
    Qt6::Sql
    dbghelp
)

if(WIN32)
    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE NOMINMAX)
    target_compile_options(${EXECUTABLE_NAME} PRIVATE /FS /MP)

    # 版本单一源：以根 CMakeLists project(VERSION) 为准注入 main.cpp
    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE
        APP_VERSION_STR="${PROJECT_VERSION}"
        APP_VERSION_WSTR=L"${PROJECT_VERSION}"
    )

    # Release构建使用WIN32子系统（无控制台窗口），Debug构建保留控制台以便调试
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set_target_properties(${EXECUTABLE_NAME} PROPERTIES
            WIN32_EXECUTABLE ON
        )
    endif()

    # windeployqt — automatically copy Qt runtime DLLs and compiler runtimes next to the executable
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_INSTALL_BINS}")

    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                --no-opengl-sw
                --compiler-runtime
                --qmldir "${CMAKE_SOURCE_DIR}/src/theme"
                --qmldir "${CMAKE_SOURCE_DIR}/src/components/qml"
                --qmldir "${CMAKE_SOURCE_DIR}/src/shell/qml"
                --qmldir "${CMAKE_SOURCE_DIR}/src/features/project/qml"
                --qmldir "${CMAKE_SOURCE_DIR}/src/features/dataset/qml"
                --qmldir "${CMAKE_SOURCE_DIR}/src/features/annotation/qml"
                --qmldir "${CMAKE_SOURCE_DIR}/src/features/training/qml"
                --qmldir "${CMAKE_SOURCE_DIR}/src/features/model/qml"
                --qmldir "${CMAKE_SOURCE_DIR}/src/features/inference/qml"
                --qmldir "${CMAKE_SOURCE_DIR}/src/features/export/qml"
                --qmldir "${CMAKE_SOURCE_DIR}/src/features/testing/qml"
                "$<TARGET_FILE:${EXECUTABLE_NAME}>"
            COMMENT "Running windeployqt to deploy Qt DLLs, MSVC runtime and QML modules..."
        )
    else()
        message(WARNING "windeployqt not found - Qt runtime DLLs will not be deployed")
    endif()
endif()

# ==================== Precompiled Headers (PCH) ====================
target_precompile_headers(${EXECUTABLE_NAME} PRIVATE
    <QGuiApplication>
    <QQmlApplicationEngine>
    <QQmlContext>
    <QQuickStyle>
    <QIcon>
    <QStandardPaths>
    <QDir>
    <QString>
    <QList>
    <QMap>
    <QDebug>
    <QObject>
)
