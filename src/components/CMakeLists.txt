# ToastBus 为全局提示总线单例，页面统一调用 ToastBus.success/error/info
set_source_files_properties(qml/ToastBus.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

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
        qml/ToastBus.qml
        qml/Toast.qml
        qml/EmptyState.qml
        qml/ConfirmDialog.qml
        qml/FormField.qml
)

target_link_libraries(labeltorch_components PUBLIC
    labeltorch_theme
)
