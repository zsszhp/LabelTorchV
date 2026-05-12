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

## Reference Projects

LabelTorch's architecture design and interaction logic reference the following excellent open-source projects. We only learn from their design concepts and architecture patterns, without directly copying code. Special thanks to:

### First Priority (Core References)

| Project | Tech Stack | Reference Value |
|---|---|---|
| **X-AnyLabeling** | PySide6 + YOLO | YOLO ecosystem annotation integration, model inference integration, annotation interaction architecture |
| **ultralytics** | Python | Training API design, model export流程, configuration parameter system |
| **labelme** | PyQt5 | QGraphicsView annotation canvas implementation, lightweight annotation interaction |
| **ImageViewer-Qt6** | Qt6 + C++ | Qt6 high-performance image viewer, zoom/pan architecture reference |

### Second Priority (Training & Format References)

| Project | Tech Stack | Reference Value |
|---|---|---|
| **yolov5** | Python | YOLOv5 training configuration organization, data loading |
| **JIETStudio** | Python GUI | End-to-end YOLO training desktop GUI workflow reference |
| **cvat** | Web/Docker | Complex annotation workflow organization, annotation review, AI-assisted annotation approach |

### Third Priority (OBB & Special Scenarios)

| Project | Tech Stack | Reference Value |
|---|---|---|
| **roLabelImg** | PyQt5 | OBB rotating box annotation interaction, rotating box data format |
| **mmrotate** | PyTorch | OBB training framework design, rotating detection pipeline |
| **DOTA_devkit** | Python | OBB data format specification (DOTA format) |
| **anylabeling** | PySide6 | Annotation and inference integration approach |

### Usage Suggestions

1. Annotation canvas & interaction: Focus on **X-AnyLabeling**, **labelme**, **ImageViewer-Qt6**
2. Training orchestration & YOLO API: Focus on **ultralytics**, **yolov5**
3. OBB rotating boxes: Phase 3 focus on **roLabelImg**, **mmrotate**, **DOTA_devkit**
4. Project management & workflow design: Reference **cvat** (annotation review workflow), **JIETStudio** (training GUI workflow)
5. Reference technical routes and architecture patterns, do not directly copy functional code

### Open Source Acknowledgments

| Project | Repository | License |
|---|---|---|
| X-AnyLabeling | https://github.com/CVHub520/X-AnyLabeling | GPL-3.0 |
| Ultralytics YOLO | https://github.com/ultralytics/ultralytics | AGPL-3.0 |
| labelme | https://github.com/wkentaro/labelme | GPL-3.0 |
| ImageViewer-Qt6 | https://github.com/p-ranav/ImageViewer-Qt6 | MIT |
| YOLOv5 | https://github.com/ultralytics/yolov5 | AGPL-3.0 |
| JIETStudio | https://github.com/hazegreleases/JIETStudio | — |
| CVAT | https://github.com/cvat-ai/cvat | MIT |
| roLabelImg | https://github.com/cgvict/roLabelImg | MIT |
| mmrotate | https://github.com/open-mmlab/mmrotate | Apache-2.0 |
| DOTA_devkit | https://github.com/CAPTAIN-WHU/DOTA_devkit | GPL-3.0 |
| AnyLabeling | https://github.com/vietanhdev/anylabeling | GPL-3.0 |

> **Note**: This project uses MIT license. The above reference projects are only for architecture design reference, and all LabelTorch code is independently implemented.
> If specific implementation patterns from a project are referenced, the source will be noted in the corresponding source file header comments.

## License

MIT License
