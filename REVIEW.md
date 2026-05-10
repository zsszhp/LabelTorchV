---
phase: manual-code-review
reviewed: 2026-05-10T12:00:00Z
depth: deep
files_reviewed: 18
files_reviewed_list:
  - src/features/annotation/qml/AnnotationPage.qml
  - src/features/training/qml/ConfigPanel.qml
  - src/features/training/qml/TrainingPage.qml
  - src/features/model/qml/ComparePage.qml
  - src/features/model/qml/ModelPage.qml
  - src/features/dataset/qml/DatasetStatsView.qml
  - src/features/inference/qml/AssistedLabelPanel.qml
  - src/features/inference/qml/LowConfQueue.qml
  - src/features/inference/qml/HardCaseQueue.qml
  - src/features/project/qml/TaskTypeSwitcher.qml
  - src/shell/qml/Main.qml
  - backend/labeltorch_backend/handlers/training.py
  - backend/labeltorch_backend/adapters/ultralytics_adapter.py
  - backend/labeltorch_backend/adapters/registry.py
  - backend/labeltorch_backend/adapters/base.py
  - backend/labeltorch_backend/server.py
  - backend/labeltorch_backend/protocol.py
  - scripts/deploy_windows.bat
  - scripts/package_backend.py
  - scripts/build_cpu_package.bat
  - scripts/check_environment.py
findings:
  critical: 3
  warning: 8
  info: 6
  total: 17
status: issues_found
---

# Phase: Code Review Report

**Reviewed:** 2026-05-10T12:00:00Z
**Depth:** deep
**Files Reviewed:** 18
**Status:** issues_found

## Summary

Reviewed 18 QML, Python, and batch script files across the LabelTorchV codebase. Found **3 critical issues**, **8 warnings**, and **6 info items**.

The most severe problems are:
1. **IPC payload field name mismatch** — C++ sends `run_id` but Python reads `task_id`, causing all training commands to fail silently.
2. **IPC config key name mismatch** — QML sends `img_size` but Python reads `imgsz`, causing image size config to be silently ignored.
3. **Race condition in training stop** — C++ optimistically marks the DB as "cancelled" before the Python backend confirms stop, and the Python `stop_training()` is a stub.

---

## Critical Issues

### CR-01: IPC Payload Field Name Mismatch — `run_id` vs `task_id`

**File:** `backend/labeltorch_backend/handlers/training.py:32,62,72` and `src/features/training/TrainingService.cpp:106,138`

**Issue:** The C++ `TrainingService` sends IPC payloads with the field `"run_id"` (line 106: `payload["run_id"] = runId`), but the Python `handle_start`, `handle_stop`, and `handle_status` handlers read from `payload.get("task_id", ...)`. This means:
- `handle_start` always gets `task_id = "unknown"` instead of the actual run ID
- `handle_stop` always gets `task_id = ""`, so it can never find the active task
- `handle_status` always gets `task_id = ""`, so it always returns "not_found"

The entire IPC training pipeline is broken — no training can be started, stopped, or queried through the backend.

**Fix:**
```python
# In training.py, change all occurrences:
# Before:
task_id = payload.get("task_id", "unknown")  # line 32
task_id = payload.get("task_id", "")          # lines 62, 72

# After:
task_id = payload.get("run_id", payload.get("task_id", "unknown"))  # line 32
task_id = payload.get("run_id", payload.get("task_id", ""))         # lines 62, 72
```
Or align C++ to use `task_id` — but changing Python to accept both is safer for backward compat.

---

### CR-02: Config Key Name Mismatch — `img_size` vs `imgsz`

**File:** `src/features/training/qml/ConfigPanel.qml:29` vs `backend/labeltorch_backend/adapters/ultralytics_adapter.py:66`

**Issue:** ConfigPanel's `getConfigJson()` serializes the image size as `"img_size"` (line 29: `"img_size": imgSizeSpin.value`), but the Ultralytics adapter reads `config.get("imgsz", 640)` (line 66). Since the key names don't match, the backend always uses the default value of 640 regardless of what the user configured in the UI. The user's image size setting is silently ignored.

**Fix:**
```javascript
// In ConfigPanel.qml, line 29, change:
"img_size": imgSizeSpin.value
// To:
"imgsz": imgSizeSpin.value
```

---

### CR-03: Training Stop Race Condition + Missing Backend Implementation

**File:** `src/features/training/TrainingService.cpp:136-156` and `backend/labeltorch_backend/adapters/ultralytics_adapter.py:140-144`

**Issue:** Two related problems:
1. **Race condition in C++:** `TrainingService::stopTraining()` sends the `train.stop` IPC command (line 139), then immediately updates the database to "cancelled" (line 143-148) and emits `runStatusChanged` (line 155) — all before receiving a response from the Python backend. If the backend fails to stop the training, the DB state is now inconsistent with reality.
2. **Missing Python implementation:** `UltralyticsAdapter.stop_training()` is a stub that just sets `self._status = "stopping"` and returns without actually interrupting the training process (line 140-144). The training continues running in the thread pool executor, consuming resources, but the UI shows it as "cancelled".

**Fix:**
For the race condition, the C++ side should wait for the IPC response before updating the DB:
```cpp
// Use a synchronous or callback-based approach to wait for the response
// before updating the DB status.
```
For the Python stub, implement actual stop logic:
```python
async def stop_training(self) -> dict:
    self._status = "stopping"
    if self._model and hasattr(self._model, 'trainer'):
        # Signal the trainer to stop
        self._model.trainer.stop = True
    return {"status": "stopping"}
```

---

## Warnings

### WR-01: AnnotationPage Uses `annotationModel.count` Instead of `annotationModel.rowCount()`

**File:** `src/features/annotation/qml/AnnotationPage.qml:1006,1091`

**Issue:** The code uses `annotationModel.count` to read the number of annotations. Since `annotationModel` is used with `annotationModel.index(i, 0)` and `annotationModel.data(idx, role)` elsewhere (which are QAbstractItemModel APIs), this is likely a C++-backed model, not a QML ListModel. QAbstractItemModel does not have a `count` property — the correct API is `rowCount()`. If `annotationModel` is a ListModel, `.count` is correct, but then using `.index(i, 0)` and `.data()` with integer roles would be inconsistent. This will either produce `undefined` (causing the "X 个标注" label to show "undefined 个标注") or a binding error.

**Fix:**
```javascript
// Change:
annotationModel.count + " 个标注"
// To:
annotationModel.rowCount() + " 个标注"

// And:
annotationModel.count > 0
// To:
annotationModel.rowCount() > 0
```

---

### WR-02: `annotationService.listSamples()` Always Receives Empty String

**File:** `src/features/annotation/qml/AnnotationPage.qml:91`

**Issue:** The sample list model is set to `annotationService.listSamples(appController.currentProjectId ? "" : "")`. The ternary expression always evaluates to `""` regardless of `currentProjectId`'s value — both branches return an empty string. The intent was likely `appController.currentProjectId` when a project is open, or `""` when not.

**Fix:**
```javascript
// Change:
model: annotationService.listSamples(appController.currentProjectId ? "" : "")
// To:
model: annotationService.listSamples(appController.currentProjectId || "")
```

---

### WR-03: `DatasetStatsView` ListModel `ids` Property Type — `ListElement` Cannot Hold JS Arrays

**File:** `src/features/dataset/qml/DatasetStatsView.qml:421-438`

**Issue:** The `anomalyModel` ListModel defines `ids: []` in its ListElements. In QML, `ListElement` values must be simple types (string, number, bool) — arrays/objects are not supported and will cause a runtime error. Later in `detectAnomalies()` (line 473), the code tries to assign `anomalyModel.get(0).ids = currentAnomalies.emptyLabels` which won't work as expected because ListModel property assignment for complex types is unsupported.

**Fix:** Store the anomaly data in a separate JavaScript property and reference it by index:
```javascript
property var anomalyData: ({emptyLabels: [], classErrors: [], sizeAnomalies: [], duplicateImages: []})

// In detectAnomalies():
anomalyData = currentAnomalies
// Update the model with just count and label:
anomalyModel.setProperty(0, "count", currentAnomalies.emptyLabels.length)
// Reference anomalyData directly in the delegate
```

---

### WR-04: LowConfQueue Slider Creates Circular Binding

**File:** `src/features/inference/qml/LowConfQueue.qml:65-69`

**Issue:** The Slider has `value: root.confThreshold` (binding from property to slider) and `onValueChanged: { root.confThreshold = value }` (binding from slider to property). This creates a circular binding: when `confThreshold` changes → slider value updates → `onValueChanged` fires → `confThreshold` is set again. In Qt Quick, this typically works due to value comparison optimization, but it can cause spurious `refresh()` calls if any consumer watches `confThreshold`, and it's fragile. The same pattern exists in AssistedLabelPanel.

**Fix:**
```javascript
Slider {
    id: thresholdSlider
    // Remove: value: root.confThreshold
    Component.onCompleted: value = root.confThreshold
    onMoved: root.confThreshold = value  // Use onMoved instead of onValueChanged
}
```

---

### WR-05: HardCaseQueue Hex Color Parsing — `substring` on Wrong Length

**File:** `src/features/inference/qml/HardCaseQueue.qml:241-246,276-280`

**Issue:** The code parses hex colors like `"#f38ba8"` using `substring(1,3)`, `substring(3,5)`, `substring(5,7)` and divides by 255. However, the `getPriorityColor()` function returns 7-character strings (`#f38ba8`), and `substring` with those indices correctly extracts `f3`, `8b`, `a8`. However, `parseFloat("f3")` returns `NaN` because `parseFloat` expects decimal numbers, not hex. The `parseInt(hex, 16)` function should be used instead. This will cause `Qt.rgba(NaN, NaN, NaN, 0.3)` which produces a transparent or black color, making the border invisible.

**Fix:**
```javascript
// Replace:
Qt.rgba(
    parseFloat(getPriorityColor(model.reason).substring(1, 3)) / 255,
    parseFloat(getPriorityColor(model.reason).substring(3, 5)) / 255,
    parseFloat(getPriorityColor(model.reason).substring(5, 7)) / 255,
    0.3
)

// With:
Qt.rgba(
    parseInt(getPriorityColor(model.reason).substring(1, 3), 16) / 255,
    parseInt(getPriorityColor(model.reason).substring(3, 5), 16) / 255,
    parseInt(getPriorityColor(model.reason).substring(5, 7), 16) / 255,
    0.3
)
```

---

### WR-06: `IpcProtocol.h` Missing `train.list_adapters` and `train.check_resume` Commands

**File:** `src/core/ipc/IpcProtocol.h:20-28`

**Issue:** The C++ `IpcProtocol.h` defines `CMD_TRAIN_START`, `CMD_TRAIN_STOP`, and `CMD_TRAIN_STATUS` but does NOT define `CMD_TRAIN_LIST_ADAPTERS` or `CMD_TRAIN_CHECK_RESUME`. However, the Python server registers handlers for `"train.list_adapters"` (server.py:26) and the training handler has `handle_check_resume` (training.py:14). If the C++ side ever needs to call these (e.g., the `trainingService.listAdapters()` method in TrainingService.cpp:270 currently returns a hardcoded list instead of querying the backend), the command name must be manually typed and could be misspelled without compile-time checking.

**Fix:** Add the missing constants to `IpcProtocol.h`:
```cpp
constexpr const char *CMD_TRAIN_LIST_ADAPTERS = "train.list_adapters";
constexpr const char *CMD_TRAIN_CHECK_RESUME = "train.check_resume";
```

---

### WR-07: `TrainingService.listAdapters()` Returns Hardcoded List Instead of Querying Backend

**File:** `src/features/training/TrainingService.cpp:270-275`

**Issue:** `TrainingService::listAdapters()` returns a hardcoded `["ultralytics"]` instead of querying the Python backend via IPC (`train.list_adapters`). The QML TrainingPage actually calls `trainingService.listAdapters()` to populate the adapter combo box (line 195), so it will always show only "ultralytics" even if custom adapters are registered. This defeats the purpose of the plugin registry system.

**Fix:**
```cpp
QStringList TrainingService::listAdapters()
{
    if (m_ipcClient) {
        // Send train.list_adapters request and parse the response
        // Cache the result for QML consumption
    }
    return {"ultralytics"}; // fallback
}
```

---

### WR-08: `deploy_windows.bat` Hardcoded Paths and Missing Error Handling

**File:** `scripts/deploy_windows.bat:8,24`

**Issue:** 
1. Line 8: Hardcoded MSVC path `C:\Program Files\Microsoft Visual Studio\2022\Community\...` — this fails on machines with VS Professional/Enterprise editions or different installation paths.
2. Line 24: Hardcoded Qt path `F:\A\QT\6.11.0\msvc2022_64\bin\windeployqt.exe` — this is a machine-specific path that will fail on any other developer's machine.
3. No error checking after `cmake --build` — if the build fails, the script continues to copy a non-existent executable.

**Fix:** Use environment variables or detect paths dynamically:
```batch
REM Use CMAKE_PREFIX_PATH or Qt installation detection
if not defined Qt6_DIR (
    echo ERROR: Qt6_DIR not set. Please set it to your Qt installation path.
    exit /b 1
)

if errorlevel 1 (
    echo ERROR: Build failed!
    exit /b 1
)
```

---

## Info

### IN-01: `ComparePage` OBB Indicator Label Does Not Account for `yolov8_cls` or `anomaly`

**File:** `src/features/training/qml/TrainingPage.qml:268`

**Issue:** The OBB task type indicator only shows when `configPanel.modelFamily === "yolov8_obb"`. There are no similar indicators for `yolov8_cls` (classification) or `anomaly` modes. Users might benefit from a mode indicator for classification ("CLS: Image Classification mode") and anomaly ("AD: Anomaly Detection mode") as well.

**Fix:** Add similar indicator labels for the other special model families.

---

### IN-02: `DatasetStatsView` Uses Chinese UI Strings While Other Pages Use English

**File:** `src/features/dataset/qml/DatasetStatsView.qml`

**Issue:** This file uses Chinese labels ("数据统计", "刷新统计", "样本统计", etc.) while most other QML files use English labels. This is an inconsistency in the UI language. If the app is intended to be bilingual, these should use `qsTr()` for translation.

**Fix:** Wrap all user-visible strings in `qsTr()` for i18n support.

---

### IN-03: `check_environment.py` Does Not Verify `ultralytics` Version Compatibility

**File:** `scripts/check_environment.py:21-27`

**Issue:** The environment check only verifies that `ultralytics` is importable, but doesn't check the version. Since the code uses features like OBB training and classification that require recent Ultralytics versions, an outdated installation could pass the check but fail at runtime.

**Fix:**
```python
import ultralytics
version = ultralytics.__version__
# Check minimum version
```

---

### IN-04: `package_backend.py` Uses Hardcoded `pip.exe` Path

**File:** `scripts/package_backend.py:22`

**Issue:** The pip path is hardcoded as `os.path.join(venv_dir, "Scripts", "pip.exe")`. While this works on Windows, the script would fail on Linux/macOS where the path is `bin/pip` instead of `Scripts/pip.exe`. This is a portability concern even though the project is Windows-focused.

**Fix:**
```python
import sys
pip = os.path.join(venv_dir, "Scripts" if sys.platform == "win32" else "bin", "pip.exe" if sys.platform == "win32" else "pip")
```

---

### IN-05: `build_cpu_package.bat` Calls `deploy_windows.bat` Then Moves Files

**File:** `scripts/build_cpu_package.bat:15-18`

**Issue:** The script calls `deploy_windows.bat` (which already creates `deploy\LabelTorch\`), then does `move deploy\LabelTorch\* %DEPLOY_DIR%\`. The `move` command with wildcards may not move subdirectories correctly on all Windows versions. Using `xcopy` or `robocopy` would be more reliable.

**Fix:** Replace the `move` command with `xcopy /e /i /y` or `robocopy`.

---

### IN-06: `UltralyticsAdapter.stop_training()` is a Stub

**File:** `backend/labeltorch_backend/adapters/ultralytics_adapter.py:140-144`

**Issue:** (Related to CR-03 but lower severity since it's already flagged.) The `stop_training()` method just sets status to "stopping" without actually interrupting the running training. Similarly, `parse_logs()` and `collect_metrics()` return empty/placeholder data.

**Fix:** Implement actual stop logic using Ultralytics' trainer API: `self._model.trainer.stop = True`.

---

_Reviewed: 2026-05-10T12:00:00Z_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: deep_
