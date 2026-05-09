# src/shell/CMakeLists.txt
# 桌面壳模块

qt_add_qml_module(labeltorch_shell
    URI LabelTorch.Shell
    STATIC
    QML_FILES
        qml/Main.qml
        qml/NavTree.qml
        qml/StatusBar.qml
        qml/TaskPanel.qml
)
