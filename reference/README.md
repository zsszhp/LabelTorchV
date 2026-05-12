# 参考项目清单

本目录包含标炬（LabelTorch）项目的参考开源项目。所有仓库均使用 `--depth 1` 浅克隆。

## 参考项目列表

### 第一优先（核心参考）

| 项目 | 技术栈 | 参考价值 |
|---|---|---|
| **X-AnyLabeling** | PySide6 + YOLO | YOLO 生态标注集成、模型推理集成、标注交互架构 |
| **ultralytics** | Python | 训练 API 设计、模型导出流程、配置参数体系 |
| **labelme** | PyQt5 | QGraphicsView 标注画布实现、轻量标注交互 |
| **ImageViewer-Qt6** | Qt6 + C++ | Qt6 高性能图像浏览器、缩放平移架构参考 |

### 第二优先（训练与格式参考）

| 项目 | 技术栈 | 参考价值 |
|---|---|---|
| **yolov5** | Python | YOLOv5 训练配置组织方式、数据加载 |
| **JIETStudio** | Python GUI | 端到端 YOLO 训练桌面 GUI 工作流参考 |
| **cvat** | Web/Docker | 复杂标注流程组织、标注审核、AI辅助标注思路 |

### 第三优先（OBB 与特殊场景）

| 项目 | 技术栈 | 参考价值 |
|---|---|---|
| **roLabelImg** | PyQt5 | OBB 旋转框标注交互、旋转框数据格式 |
| **mmrotate** | PyTorch | OBB 训练框架设计、旋转检测 pipeline |
| **DOTA_devkit** | Python | OBB 数据格式规范（DOTA 格式） |
| **anylabeling** | PySide6 | 标注与推理集成思路 |

## 参考使用建议

1. 标注画布与交互：重点参考 **X-AnyLabeling**、**labelme**、**ImageViewer-Qt6**
2. 训练编排与 YOLO API：重点参考 **ultralytics**、**yolov5**
3. OBB 旋转框：Phase 3 阶段重点参考 **roLabelImg**、**mmrotate**、**DOTA_devkit**
4. 工程管理与流程设计：参考 **cvat**（标注审核流程）、**JIETStudio**（训练 GUI 工作流）
5. 参考技术路线和架构模式，不直接复制功能代码

## 开源致谢与许可证声明

标炬（LabelTorch）的架构设计和交互逻辑参考了以下优秀开源项目。我们仅学习其设计思路和架构模式，不直接复制代码。特此致谢：

| 项目 | 仓库地址 | 许可证 |
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

> **注意**：本项目使用 MIT 许可证。以上参考项目仅作架构设计参考，标炬的所有代码均为独立实现。
> 如参考了特定项目的具体实现模式，将在对应源文件头部注释中标注来源。

