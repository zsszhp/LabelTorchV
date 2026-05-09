# 参考项目链接清单

## 说明

本文档列出供技术路线和项目管理参考的优秀开源项目链接。实现 AI 可在需要时自行 clone 到本地参考，不必预先下载。

## 通用检测/分割/多边形标注

1. **CVAT**
   - https://github.com/cvat-ai/cvat
   - 借鉴：复杂标注流程组织方式、标注审核思路

2. **Label Studio**
   - https://github.com/HumanSignal/label-studio
   - 借鉴：可配置任务流设计思想、数据管理与标注任务抽象

3. **Labelme**
   - https://github.com/wkentaro/labelme
   - 借鉴：轻量本地标注工作流、快捷键与基础交互

4. **LabelImg**
   - https://github.com/HumanSignal/labelImg
   - 借鉴：轻量本地标注工作流、基础交互

5. **VoTT**
   - https://github.com/microsoft/VoTT
   - 借鉴：微软标注工具设计思路

6. **make-sense**
   - https://github.com/SkalskiP/make-sense
   - 借鉴：Web 端标注交互

## YOLO 与工业场景相关

1. **X-AnyLabeling**
   - https://github.com/CVHub520/X-AnyLabeling
   - 借鉴：YOLO 生态标注集成思路、标注工具交互组织

2. **AnyLabeling**
   - https://github.com/vietanhdev/anylabeling
   - 借鉴：标注与推理集成思路

3. **Ultralytics**
   - https://github.com/ultralytics/ultralytics
   - 借鉴：训练 API 设计、模型导出流程

4. **YOLOv5**
   - https://github.com/ultralytics/yolov5
   - 借鉴：训练配置组织方式

## OBB/旋转框相关参考

1. **roLabelImg**
   - https://github.com/cgvict/roLabelImg
   - 借鉴：OBB 与旋转框表达、旋转框标注交互

2. **DOTA_devkit**
   - https://github.com/CAPTAIN-WHU/DOTA_devkit
   - 借鉴：OBB 数据格式规范

3. **mmrotate**
   - https://github.com/open-mmlab/mmrotate
   - 借鉴：OBB 训练框架设计

## 建议参考优先级

1. **第一优先**：X-AnyLabeling、CVAT、Label Studio、Labelme
2. **第二优先**：roLabelImg、mmrotate（OBB 与旋转框）
3. **第三优先**：make-sense、VoTT（交互与工程实现参考）

## 使用建议

1. 不必全部 clone，按当前实现阶段选择相关项目
2. 重点参考技术路线和项目管理方式，不盲目复制功能
3. 标注交互细节可参考 Labelme、X-AnyLabeling
4. 训练编排可参考 Ultralytics 官方实现
