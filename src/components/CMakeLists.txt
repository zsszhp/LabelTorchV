qt_add_qml_module(labeltorch_components
    URI LabelTorch.Components
    STATIC
    RESOURCE_PREFIX /qt/qml
    QML_FILES
        qml/Stepper.qml
        qml/ToggleSwitch.qml
        qml/CollapsibleSection.qml
        qml/Splitter.qml
        qml/SectionTitle.qml
        qml/ParamRow.qml
        qml/ModalDialog.qml
        qml/ContextMenu.qml
        qml/FilterBar.qml
        qml/RingProgress.qml
        qml/ColorPicker.qml
        qml/StatusTag.qml
)

target_link_libraries(labeltorch_components PUBLIC
    labeltorch_theme
)
