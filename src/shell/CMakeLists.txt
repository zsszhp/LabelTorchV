qt_add_qml_module(labeltorch_shell
    URI LabelTorch.Shell
    STATIC
    QML_FILES
        qml/Main.qml
        qml/NavTree.qml
        qml/StatusBar.qml
        qml/TaskPanel.qml
        qml/LogPanel.qml
        qml/Theme.qml
)
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON TRUE)
