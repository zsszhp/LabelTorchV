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
    Qt6::Quick
    Qt6::QuickControls2
)

# Windows平台设置
if(WIN32)
    set_target_properties(labeltorch PROPERTIES
        WIN32_EXECUTABLE ON
    )
endif()
