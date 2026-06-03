qt_add_qml_module(labeltorch_shell
    URI LabelTorch.Shell
    STATIC
    RESOURCE_PREFIX /qt/qml
    QML_FILES
        qml/Main.qml
        qml/NavTree.qml
        qml/StatusBar.qml
        qml/TaskPanel.qml
        qml/LogPanel.qml
    RESOURCES
        qml/icons/sidebar-collapse.svg
        qml/icons/sidebar-expand.svg
)

target_link_libraries(labeltorch_shell PUBLIC
    labeltorch_theme
    labeltorch_components
)
