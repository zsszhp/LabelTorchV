# CHANGELOG

## v1.0.0 (2026-05-10)

### Phase 0: Platform Foundation
- Project management (create/open/delete/recent list)
- SQLite database with 13 core tables and migration mechanism
- YOLO txt data import with validation and class schema extraction
- Class mapping with preview, execution, and history tracking
- Annotation engine with geometry kernel, canvas rendering, and interaction
- Annotation save state machine with revision tracking
- IPC protocol with Python backend (JSON-RPC over stdin/stdout)
- Ultralytics training adapter with async execution
- Task scheduling and global status management

### Phase 1: Detection Closed Loop
- Data snapshot service with train/val split
- Training workbench with config panel and log viewer
- Model version registry with metrics and tag management
- Inference service with assisted labeling and candidate review
- Incremental training with parent version lineage
- pt/onnx export with verification

### Phase 2: Training Enhancement
- AMP support with UI toggle
- Resume training (checkpoint detection + continuation)
- Multi-version experiment comparison (horizontal + vertical)
- Data quality analysis (sample stats, class distribution, anomaly detection)

### Phase 3: OBB Version
- OBB annotation editing with RotatedBox geometry
- DOTA format YOLO txt read/write
- HBB/OBB mode switching
- OBB data validation and class mapping
- yolov8_obb model family training
- OBB dataset detection (isOBBDataset)

### Phase 4: Active Learning
- Low-confidence sample collection with configurable threshold
- False positive/negative queue management
- Hard-case queue with priority sorting (FN > low-conf > FP)
- Priority review prompts and annotation navigation

### Phase 5: Multi-Task Platform
- Classification task support (single-label/multi-label)
- Anomaly detection task support (normal/anomalous labeling)
- Multi-task unified workbench with task type switching
- Task-type-aware annotation and training panels

### Phase 6: Mature Product
- Plugin-based trainer registry (TrainingAdapterRegistry)
- Windows deployment scripts (windeployqt + Python venv)
- CPU green package build script
- Environment self-check on first launch
- Chinese and English README
