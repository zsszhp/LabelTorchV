qt_add_qml_module(labeltorch_shell
    URI LabelTorch.Shell
    STATIC
    QML_FILES
        qml/Main.qml
        qml/NavTree.qml
        qml/StatusBar.qml
        qml/TaskPanel.qml
        qml/Theme.qml
        qml/LogPanel.qml
    SINGLETONS
        qml/Theme.qml
)
