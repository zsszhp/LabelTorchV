# src/core/CMakeLists.txt
# 核心基础设施静态库

add_library(labeltorch_core STATIC
    database/Database.cpp
    database/Schema.cpp
    filesystem/ProjectFs.cpp
    ipc/IpcClient.cpp
    ipc/IpcProtocol.cpp
    cache/ThumbnailCache.cpp
    utils/Log.cpp
)

target_include_directories(labeltorch_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(labeltorch_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Sql
)
