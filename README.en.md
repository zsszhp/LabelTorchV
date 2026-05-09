# LabelTorch (标炬)

Industrial defect detection desktop software — an integrated platform for annotation, training, inference, and export.

## Features

- **Project Management**: Create/open projects, recent project list
- **Data Import**: YOLO txt format import with auto-validation
- **Class Mapping**: Class ID remapping, merging, splitting with history tracking
- **Annotation Engine**: HBB/OBB/Classification/Anomaly detection modes
- **Data Snapshots**: Immutable snapshots with train/val auto-split
- **Training Workbench**: Ultralytics YOLO integration, multi-model family support
- **Model Versioning**: Version registration, metric tracking, tag management, lineage chain
- **Assisted Labeling**: Inference candidate review, batch confirm/reject
- **Experiment Comparison**: Horizontal/vertical metric comparison across versions
- **Data Quality**: Sample statistics, class distribution, anomaly detection
- **Active Learning**: Low-confidence feedback, miss/false-positive queues, hard-case prioritization
- **Multi-Task Platform**: Unified workbench for detection/OBB/classification/anomaly
- **Model Export**: pt/onnx format export with ONNX config panel
- **Plugin Trainers**: TrainingAdapter registry for third-party integration

## Tech Stack

| Layer | Technology |
|---|---|
| Desktop Frontend | Qt 6 + QML + C++17 |
| Python Backend | Python 3.11 + asyncio JSON-RPC |
| Training Engine | Ultralytics YOLO (v5/v8/v8-obb/v8-cls/v10/v11) |
| Database | SQLite 3 (13 core tables) |
| IPC | stdin/stdout JSON-RPC protocol |
| Build | CMake + Ninja + MSVC 2022 |

## Build Instructions

### Prerequisites

- Qt 6.11+ (MSVC 2022 64-bit)
- Visual Studio 2022 (MSVC x64)
- CMake 3.21+
- Ninja
- Python 3.11+ (conda env: labeltorch)

### Compile

```bash
# Configure
cmake --preset msvc2022-debug

# Build
cmake --build --preset msvc2022-debug

# Test
cd out/build/msvc2022-debug && ctest --output-on-failure
```

### Python Backend

```bash
cd backend
pip install -r requirements.txt
```

## License

MIT License
