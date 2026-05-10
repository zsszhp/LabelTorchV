---
phase: code-review
reviewed: 2026-05-10T08:17:00Z
depth: standard
files_reviewed: 26
files_reviewed_list:
  - src/features/annotation/AnnotationService.h
  - src/features/annotation/AnnotationService.cpp
  - src/features/annotation/geometry/RotatedBox.h
  - src/features/annotation/geometry/RotatedBox.cpp
  - src/features/annotation/labelio/YoloTxtReader.h
  - src/features/annotation/labelio/YoloTxtReader.cpp
  - src/features/annotation/labelio/YoloTxtWriter.h
  - src/features/annotation/labelio/YoloTxtWriter.cpp
  - src/features/annotation/AnnotationModel.h
  - src/features/annotation/AnnotationModel.cpp
  - src/features/dataset/DatasetService.h
  - src/features/dataset/DatasetService.cpp
  - src/features/dataset/ImportScanner.h
  - src/features/dataset/ImportScanner.cpp
  - src/features/dataset/ClassMappingService.h
  - src/features/dataset/ClassMappingService.cpp
  - src/features/model/MetricService.h
  - src/features/model/MetricService.cpp
  - src/features/inference/AssistedLabelService.h
  - src/features/inference/AssistedLabelService.cpp
  - src/features/project/ProjectService.h
  - src/features/project/ProjectService.cpp
  - src/features/training/TrainingService.h
  - src/features/training/TrainingService.cpp
  - src/features/training/SnapshotService.h
  - src/features/training/SnapshotService.cpp
  - src/core/database/Database.h
  - src/core/database/Database.cpp
findings:
  critical: 2
  warning: 8
  info: 7
  total: 17
status: issues_found
---

# Code Review Report

**Reviewed:** 2026-05-10T08:17:00Z
**Depth:** standard
**Files Reviewed:** 26
**Status:** issues_found

## Summary

Reviewed 26 C++ source files across the LabelTorchV application, covering annotation services (HBB/OBB/classification/anomaly), dataset management, import scanning, class mapping, metrics, assisted labeling, project management, training, snapshots, and database core.

The codebase is well-structured with consistent patterns (parameterized SQL queries, atomic file writes, proper Qt model/view implementation). However, several issues were found:

- **2 Critical issues**: A race condition in `ClassMappingService::remapLabelFile` where the file is read then written without atomic write semantics (no temp+rename), risking data loss on crash; and a `const_cast` hack in `ProjectService::getCurrentProject` that breaks const-correctness and could cause undefined behavior.
- **8 Warnings**: Thread safety concerns with `m_shapeType` in `AnnotationService`, missing null checks on `Database::instance().database()`, non-transactional multi-step DB operations that can leave inconsistent state, an `ImportScanner` that only validates HBB (5-col) format and silently skips OBB (9-col) lines, and a `DatasetService::deleteDataset` that doesn't clean up annotation_revisions.
- **7 Info items**: Redundant JSON serialization in `createRevision`, unnecessary `keyMetrics` variable in `MetricService::getMetrics`, inconsistent use of `Id::generate()` vs `QUuid::createUuid()`, missing error handling on PRAGMA execution, and duplicated code patterns in annotation load/save methods.

## Critical Issues

### CR-01: Non-atomic file write in ClassMappingService::remapLabelFile — data loss risk

**File:** `src/features/dataset/ClassMappingService.cpp:560-573`
**Issue:** The `remapLabelFile` method reads a label file, modifies it in memory, then writes it back in-place using `QIODevice::Truncate`. If the process crashes or the system loses power between truncating the file and writing the new content, the label file will be left empty or partially written, causing permanent data loss. Every other file-write operation in the codebase (YoloTxtWriter, AnnotationService saveClassificationLabels/saveAnomalyLabels) correctly uses the atomic temp-file+rename pattern, but this method does not.
**Fix:**
```cpp
// Replace the in-place write at lines 560-573 with atomic write:
const QString tempPath = filePath + QStringLiteral(".tmp");
{
    QFile tempFile(tempPath);
    if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning() << "Cannot open temp file for remap:" << tempPath;
        return false;
    }
    QTextStream out(&tempFile);
    for (int i = 0; i < outputLines.size(); ++i) {
        out << outputLines[i];
        if (i < outputLines.size() - 1) {
            out << "\n";
        }
    }
    out.flush();
    if (!tempFile.flush()) {
        QFile::remove(tempPath);
        return false;
    }
} // tempFile closed
if (QFile::exists(filePath) && !QFile::remove(filePath)) {
    QFile::remove(tempPath);
    return false;
}
if (!QFile::rename(tempPath, filePath)) {
    QFile::remove(tempPath);
    return false;
}
```

### CR-02: const_cast to call non-const method — undefined behavior risk

**File:** `src/features/project/ProjectService.cpp:98`
**Issue:** `getCurrentProject()` is a `const` method but uses `const_cast<ProjectService*>(this)->getTaskType(...)` to call a non-const method. `getTaskType` is non-const because it calls `ensureTaskTypeColumn()` which may mutate the database schema (ALTER TABLE). This breaks const-correctness — callers of `getCurrentProject()` expect no side effects, but this can modify the database. If `ProjectService` is accessed through a const reference in a multi-threaded context, the const_cast leads to a data race.
**Fix:**
```cpp
// Make getTaskType/ensureTaskTypeColumn const:
QString getTaskType(const QString &projectId) const;
bool ensureTaskTypeColumn() const;

// In ensureTaskTypeColumn, the QSqlQuery operations don't modify
// member state, so they can be const. Then remove the const_cast:
p["taskType"] = getTaskType(m_currentProjectId);
```

## Warnings

### WR-01: Thread-unsafe access to m_shapeType in AnnotationService

**File:** `src/features/annotation/AnnotationService.h:189` and `src/features/annotation/AnnotationService.cpp:29,59`
**Issue:** `m_shapeType` is a plain `int` member accessed without synchronization. Since `AnnotationService` is a `QObject` with `Q_INVOKABLE` methods callable from QML, concurrent calls to `loadAnnotations`/`saveAnnotations` and `setShapeType` from different threads could read a partially-written `m_shapeType` value, leading to the wrong format being used (e.g., OBB data written in HBB format).
**Fix:** Either make `m_shapeType` an `std::atomic<int>`, or protect it with a `QMutex`, or ensure `setShapeType` and the load/save methods are only called from the same thread.

### WR-02: Missing null/isOpen check on Database access in multiple services

**File:** `src/features/annotation/AnnotationService.cpp:492,517,547`; `src/features/dataset/DatasetService.cpp:34,94,166`; `src/features/project/ProjectService.cpp:18,41,57,86`
**Issue:** Many service methods call `Database::instance().database()` and immediately construct a `QSqlQuery` without checking if the database is actually open. If the database failed to open or hasn't been opened yet, the query will silently fail. Only `MetricService`, `TrainingService`, and `SnapshotService` consistently check `db.isOpen()` before proceeding. The inconsistency means some services will produce empty results and log errors, while others may crash or behave unpredictably.
**Fix:**
```cpp
// Add this pattern to all service methods that access the database:
auto db = Database::instance().database();
if (!db.isOpen()) {
    qWarning() << "Database is not open";
    return {}; // or false, as appropriate
}
```

### WR-03: Non-transactional multi-step database operations

**File:** `src/features/dataset/DatasetService.cpp:164-197` (deleteDataset); `src/features/dataset/DatasetService.cpp:23-108` (importDataset); `src/features/dataset/ClassMappingService.cpp:253-426` (applyMapping)
**Issue:** `deleteDataset` performs 3 DELETE operations (schemas, samples, dataset) without a transaction. If the process crashes after deleting schemas and samples but before deleting the dataset record, the database is left in an inconsistent state: the dataset record exists but all its children are gone. Similarly, `importDataset` performs INSERT operations in multiple steps without a transaction. `applyMapping` updates both taxonomy and label files without a transaction — a partial failure leaves the taxonomy updated but some label files unmapped.
**Fix:**
```cpp
// Wrap multi-step operations in a transaction:
QSqlDatabase db = Database::instance().database();
if (!db.transaction()) return false;
// ... perform operations ...
if (allSucceeded) {
    db.commit();
} else {
    db.rollback();
    return false;
}
```

### WR-04: ImportScanner::parseLabelFile rejects valid OBB (9-column) lines

**File:** `src/features/dataset/ImportScanner.cpp:159`
**Issue:** `parseLabelFile` validates that each label line has exactly 5 values (`parts.size() != 5`). This means OBB-format label files (9 values per line) are always flagged as invalid during import, even though the rest of the application supports OBB. The scanner has a separate `validateOBBLine` static method, but `parseLabelFile` (called during `scan()`) doesn't use it. When an OBB dataset is imported, every line in every label file is marked as a parse error, and all samples get `invalid_label` status.
**Fix:**
```cpp
// Replace the strict 5-column check with format-aware validation:
if (parts.size() != 5 && parts.size() != 9) {
    errors.append(QStringLiteral("Line %1: expected 5 (HBB) or 9 (OBB) values, got %2")
                      .arg(lineNumber).arg(parts.size()));
    allValid = false;
    continue;
}
// Also validate coordinate range differently for OBB (8 coords vs 4)
```

### WR-05: DatasetService::deleteDataset doesn't delete annotation_revisions

**File:** `src/features/dataset/DatasetService.cpp:164-197`
**Issue:** `deleteDataset` deletes `imported_label_schemas`, `dataset_samples`, and `datasets` records, but doesn't delete `annotation_revisions` for the dataset's samples. This leaves orphaned revision records in the database. Over time, these accumulate and consume storage. Additionally, if foreign key constraints are enforced (PRAGMA foreign_keys=ON is set), the deletion may fail silently if annotation_revisions has a foreign key referencing dataset_samples or datasets.
**Fix:**
```cpp
// Add before the schemas deletion:
QSqlQuery revisionQuery(db);
revisionQuery.prepare("DELETE FROM annotation_revisions WHERE dataset_id = ?");
revisionQuery.addBindValue(datasetId);
if (!revisionQuery.exec()) {
    qWarning() << "Failed to delete annotation revisions:" << revisionQuery.lastError().text();
    return false;
}
```

### WR-06: RotatedBox::isValid has redundant w/h check

**File:** `src/features/annotation/geometry/RotatedBox.cpp:55-63`
**Issue:** `isValid()` checks `w > 0.0f && h > 0.0f` on line 57, then redundantly checks `w >= 0.0f && w <= 1.0f` and `h >= 0.0f && h <= 1.0f` on lines 60-61. The second pair of checks is partially redundant since `w > 0` already implies `w >= 0`. More importantly, the `> 0` check and `<= 1` check together mean valid w/h is in (0, 1], but `w = 0` fails the first check while passing the second — this is correct behavior but confusing. The real issue is that if `w > 0` passes but `w > 1.0f` (due to a bug elsewhere), the check on line 60 would catch it, but only because the `&&` short-circuits — the logic is hard to reason about.
**Fix:** Simplify to a single clear check:
```cpp
return cx >= 0.0f && cx <= 1.0f
    && cy >= 0.0f && cy <= 1.0f
    && w  >  0.0f && w  <= 1.0f
    && h  >  0.0f && h  <= 1.0f
    && angle >= -360.0f && angle <= 360.0f;
```

### WR-07: SnapshotService::listSnapshots issues N+1 queries per snapshot

**File:** `src/features/training/SnapshotService.cpp:126-175`
**Issue:** For each snapshot returned by the main query, `listSnapshots` executes 2 additional queries (one for the manifest, one for the split) — an N+1 query pattern. With 100 snapshots, this produces 201 database queries. The main query already selects `sample_manifest_json` and `split_manifest_json` in `getSnapshot()` but not in `listSnapshots()`. This is also a data integrity concern: if the manifest changes between the main query and the sub-queries, the sample counts won't match the snapshot record.
**Fix:** Select `sample_manifest_json` and `split_manifest_json` in the main query and parse them inline, just like `getSnapshot` does.

### WR-08: YoloTxtReader::detectFormat double-close and possible resource leak

**File:** `src/features/annotation/labelio/YoloTxtReader.cpp:176-205`
**Issue:** In `detectFormat`, when a non-empty line is found, `file.close()` is called at line 192 inside the loop, and then the function returns. If the `while` loop completes without finding any non-empty line, `file.close()` is called at line 203. This is not a bug per se (QFile destructor closes), but the pattern is fragile. More importantly, if `file.close()` at line 192 fails, the function still returns a valid result, and the later `file.close()` at line 203 is never reached. This is minor, but the pattern should use RAII (QFile destructor) instead of manual close.
**Fix:** Remove the explicit `file.close()` calls — let QFile's destructor handle it. Alternatively, use a consistent pattern: close in all paths or none.

## Info

### IN-01: Redundant serialization in AnnotationService::createRevision

**File:** `src/features/annotation/AnnotationService.cpp:444-510`
**Issue:** `createRevision` serializes `beforeSnapshot` and `afterSnapshot` to JSON by iterating over each item, converting to a QJsonObject with specific keys. But the caller already provides QVariantList where each item is a QVariantMap. The classification and anomaly label save methods construct snapshots with different key structures (e.g., `labelType`, `classId`, `isAnomalous`), but the serialization hardcodes HBB/OBB keys (cx, cy, w, h, angle). This means classification/anomaly revision snapshots get serialized with default values (0.0) for geometry fields that don't exist in their maps.
**Fix:** Use `QJsonDocument::fromVariant()` for generic serialization, or add a flag/switch to handle different snapshot types.

### IN-02: Unused keyMetrics variable in MetricService::getMetrics

**File:** `src/features/model/MetricService.cpp:44-47`
**Issue:** The `keyMetrics` QStringList is declared but never used. Lines 54-58 manually check for specific keys instead of iterating over `keyMetrics`. The variable adds confusion about the intended behavior.
**Fix:** Remove the unused `keyMetrics` variable, or use it in the loop below to make the code consistent.

### IN-03: Inconsistent ID generation: Id::generate() vs QUuid::createUuid()

**File:** `src/features/annotation/AnnotationService.cpp:449` (uses `Id::generate`); `src/features/annotation/labelio/YoloTxtReader.cpp:50,122` (uses `QUuid::createUuid`); `src/features/training/TrainingService.cpp:35` (uses `QUuid::createUuid`); `src/features/training/SnapshotService.cpp:106` (uses `QUuid::createUuid`)
**Issue:** The codebase uses two different UUID generation approaches. Some files use `Id::generate()` (a project utility) while others use `QUuid::createUuid().toString(QUuid::WithoutBraces)` directly. This inconsistency makes it harder to change the ID format globally and suggests the code was written at different times without reconciliation.
**Fix:** Standardize on `Id::generate()` everywhere and update `Id::generate()` if the current implementation differs from `QUuid::createUuid().toString(QUuid::WithoutBraces)`.

### IN-04: Database::open doesn't check PRAGMA execution results

**File:** `src/core/database/Database.cpp:37-38`
**Issue:** `query.exec("PRAGMA journal_mode=WAL")` and `query.exec("PRAGMA foreign_keys=ON")` are called but their return values are not checked. If either PRAGMA fails (e.g., the SQLite driver doesn't support WAL mode), the application proceeds without the intended guarantees.
**Fix:** Check the return values and log warnings on failure.

### IN-05: Duplicated QVariantMap construction in annotation load/save

**File:** `src/features/annotation/AnnotationService.cpp:38-51,69-83,111-124,137-151`
**Issue:** The conversion between domain objects (AxisAlignedBox, RotatedBox) and QVariantMap is repeated 4 times with nearly identical code. This is a maintenance burden — adding a new field requires updating all 4 locations.
**Fix:** Create a helper function like `axisAlignedBoxToVariantMap()` and `rotatedBoxToVariantMap()` and reuse them in both load and save paths.

### IN-06: AnnotationModel::loadFromLabel creates a new AnnotationService per call

**File:** `src/features/annotation/AnnotationModel.cpp:94`
**Issue:** `loadFromLabel` creates a local `AnnotationService svc;` on each call. This is wasteful because it allocates/deallocates a QObject each time. While not a correctness issue, it's inconsistent with how other services are used (typically as singletons or injected dependencies).
**Fix:** Either store an `AnnotationService` as a member of `AnnotationModel`, or make the service methods static since `AnnotationService` has no significant state beyond `m_shapeType` (which isn't used by `loadAnnotations` when it delegates to `loadOBBAnnotations`).

### IN-07: DatasetService::getSampleStats and getClassDistribution duplicate label-parsing logic

**File:** `src/features/dataset/DatasetService.cpp:266-289,504-523`
**Issue:** Both `getSampleStats` and `getClassDistribution` contain nearly identical label file parsing code (open file, read lines, split, parse classId, count). This duplicated logic should be extracted into a shared utility.
**Fix:** Extract a helper like `parseClassIdsFromLabelFile(const QString &labelPath, QMap<int, int> &classCounts)` and reuse it.

---

_Reviewed: 2026-05-10T08:17:00Z_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
