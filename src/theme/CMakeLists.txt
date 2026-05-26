set_source_files_properties(Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(labeltorch_theme
    URI LabelTorch.Theme
    STATIC
    RESOURCE_PREFIX /qt/qml
    QML_FILES
        Theme.qml
)
