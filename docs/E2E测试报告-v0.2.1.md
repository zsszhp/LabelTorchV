# E2E 全链路测试报告 v0.2.1

> 测试日期：2026-10-01
> 测试环境：Windows 11 x64 / 无 GPU（CPU 训练）/ Qt 6.11.1 / Python 3.x (labeltorch env) / ultralytics + anomalib 2.5.0
> 测试方式：LT_DEBUG_* 自动化钩子驱动真实 UI 进程（非直调后端），运行日志 + SQLite + 截图为证据

---

## 一、测试矩阵与结论总览

| 场景 | 数据 | 任务类型 | 模型 | 训练 | 评估 | 导出 onnx | 结论 |
|------|------|---------|------|------|------|-----------|------|
| S1 | 12 图合成缺陷（YOLO txt） | detect | yolov8n from_scratch | ✅ 27s succeeded | ✅ 混淆矩阵/PR/metrics 落库 | ✅ 验证通过 | **全链路通过** |
| S2 | 48 图合成缺陷（95 框） | detect | yolov8n from_scratch ×2轮 | ✅ ~2min succeeded | ✅ succeeded | ✅ | **全链路通过** |
| S3 | 12 图（8 good + 4 defective） | anomaly | anomalib PatchCore CPU | ✅ succeeded（.ckpt 落盘） | ✅ AUROC=0.5（小数据预期值） | ✅ ckpt→onnx 验证通过 | **全链路通过** |
| S4 | 无项目状态 | — | — | — | — | — | 12 页可浏览、空态引导、0 错误 |
| S5 | 示例项目打开状态 | — | — | — | — | — | 8 页×0 错误、0 崩溃转储 |
| S6 | 24 图分类数据（label_path 存类名） | classify | yolov8n-cls from_scratch | ✅ 17s succeeded | ✅ succeeded | — | **全链路通过** |
| S7 | 16 图 8 角点 OBB 标签 | obb | yolov8n-obb from_scratch | ✅ ~5min succeeded | ✅ succeeded | — | **全链路通过** |
| S8 | 30 帧测试视频 | 视频推理 | S1 训练权重 | — | — | — | 链路通过（30帧处理+标注视频落盘）；按用户要求暂不深测 |

稳定性门禁：ctest **24/24** 通过 · pytest **48/48** 通过 · 运行日志 QML 错误类（ReferenceError/TypeError/Cannot read/无法锚定）清零 · 崩溃转储目录 0 文件。

**任务类型覆盖结论：四种任务类型（detect / anomaly / classify / obb）的训练→评估→导出链路全部真实跑通。**

### 2.9 S6 触发的修复：分类任务 data 参数（P1 级）
- **现象**：ultralytics 分类模型的 `data` 参数需要 ImageFolder 根目录（含 train/ val/），适配器传的是快照 data.yaml 文件路径 → 分类训练/评估必失败。
- **修复**：`ultralytics_adapter.py` 训练与 `handlers/testing.py` 评估两处按 `model.task == "classify"` 分流，传快照根目录。
- **验证**：S6 分类训练 17s 成功、评估成功。

### 2.10 S8 触发的修复：supervision 0.29 API 漂移（P1 级）
- **现象**：`ColorPalette.default` 在 supervision 0.29 更名为 `ColorPalette.DEFAULT` → 视频推理必失败。
- **修复**：`handlers/inference.py` 改用 `DEFAULT`（带旧版回退）。
- **验证**：30 帧测试视频推理成功，标注视频落盘。

### 2.11 功能缺口登记：程度图像模式（P1，未接线）
- `CheckPage.qml` 的 `severityMode` 只是本地开关，**无任何下游消费**：`AnnotationService` 无程度概念，标注页不接收程度意图，「标注程度模型」按钮仅跳转页面。
- **建议方案**（需产品决策）：程度分级复用分类标签体系（如 轻微/中等/严重 三个程度类别），标注页在程度模式下显示程度选择面板（复用分类模式 UI），写 classification labels；检查页 severityMode 打开时按样本分类标签显示程度徽章。
- 状态：**登记为 v0.3 P1 待办**，本轮未实现（避免半接线引入新风险）。

---

## 二、测试中发现并修复的真实缺陷

本报告的每一项修复都由上表场景实测触发后修复、再实测确认。

### 2.1 离线环境训练必挂（P0 级）
- **现象**：from_scratch 训练也强制加载 `yolov8n.pt`，无外网时 curl 连 github.com 21s×4 次重试后失败。
- **修复**：`ultralytics_adapter.py` 按配置选择 `.yaml`（结构定义，本地）或 `.pt`（预训练权重）；`from_scratch` / `pretrained=false` 不再联网。
- **验证**：S1/S2 在断网环境训练成功。

### 2.2 anomalib 适配器 API 漂移（P0 级，4 处）
anomalib 2.5 相对适配器编写时的 2.4.2 有 4 处破坏性变更，逐处修复：
1. `Folder(image_size=...)` 参数已移除 → 删除（缩放走 augmentations 体系）
2. 小测试集（2~3 张）`from_test` 切分模式除零（`subset_lengths` 取整为 0）→ 改 `val_split_mode="from_train"`
3. `Engine.fit(callbacks=...)` 不再接受 callbacks → 回调移至 `Engine(callbacks=[...])` 构造
4. ckpt 导出时按 `options.model_family` 默认 `efficient_ad` 重建 → **错配到错误模型族**。修复：优先 `AnomalibModule.load_from_checkpoint`，失败时从 ckpt 落盘路径（`models/<Family>/...`）推导 family + `hyper_parameters` 重建 + `load_state_dict`

### 2.3 PyTorch 2.6 weights_only 默认变更（P0 级）
- **现象**：`torch.load` 默认 `weights_only=True` 拒绝 `anomalib.PrecisionType` 全局对象，ckpt 加载失败。
- **修复**：ckpt 重建路径显式 `weights_only=False`（路径已经 IPC 白名单校验，属项目内受信训练产物）。

### 2.4 IPC 业务失败响应丢失关联 ID（P1 级）
- **现象**：handler 返回 `status=failed` 时，`server.py` 将其转为错误信封并**丢弃 result** → C++ 侧 `ExportService` 读不到 `artifact_id`，无法定位失败对象，只报 "Export response missing artifact_id"。
- **修复**：失败响应保留 `result`（artifact_id/run_id 等照常携带）。
- **验证**：异常导出失败时 C++ 能正确报出具体 artifact 与错误信息。

### 2.5 测试工程播种的教训（流程问题）
- SQL 直插测试工程（绕过 `createProject`）会缺少 `project.json` 落盘 → 路径白名单无法沿路径反推项目根 → 测试评估报「路径越界」。
- **结论**：任何测试/外部工具创建工程都必须走 service 公共 API（createProject 会写 project.json）；本报告 S2 复测已补齐。

### 2.6 中文乱码根因（P1 级，全局）
- MSVC 无 `/utf-8` 标志时，无 BOM UTF-8 源码中的中文字面量被按 GBK 解释（全局影响所有 C++ 中文文案）；`QLatin1String` 包装中文字面量会把 UTF-8 字节逐字节映射成乱码字符。
- **修复**：根 CMakeLists 全局 `add_compile_options(/utf-8)`；DemoBootstrap 的 `QLatin1String(中文)` 全部改 `QString::fromUtf8`。

---

## 三、演示模式与无项目浏览（本版新增能力）

1. **内置示例数据**：12 张合成工业缺陷图（`scripts/gen_demo_data.py` 确定性生成：拉丝金属面板 + 划痕/凹陷/污染三类缺陷，834KB）经 CMake 资源打进 exe，首次运行释放到 `%AppData%/LabelTorch/demo/`。
2. **首次运行自动建项**：projects 表为空时自动创建「示例项目」（3 类别 + 预置标签 + 数据集）并打开——开箱即可浏览全部功能。
3. **一键载入**：项目页侧栏「载入示例项目」按钮 + 未开项目时全局横幅入口（幂等，可反复调用）。
4. **导航解锁**：未打开项目也允许浏览全部页面；各页呈现空态引导（EmptyState + 直达按钮），全局横幅提示数据为空。

---

## 四、遗留与建议（不阻塞使用）

| 项 | 说明 | 建议 |
|----|------|------|
| matplotlib CJK 告警 | anomalib 内部绘图缺 CJK 字体（仅告警，训练不受影响） | 后端绘图配置 SimHei/雅黑字体 |
| 分类（classify）任务 E2E | ultralytics cls 需 ImageFolder 布局，快照目前产出 YOLO 式 data.yaml | P1：快照为 classify 产出类文件夹布局 |
| HF 依赖 | anomalib backbone 首次需下载（本机已验证 hf-mirror.com 可达） | 交接文档注明 `HF_ENDPOINT=https://hf-mirror.com` |
| GPU 实测 | 本机无 GPU，全部训练为 CPU 路径 | 按 GPU 开发机交接指南在 30/40/50 系显卡复测 |
| 视频推理 E2E | 页面与服务存在，未做真实视频链路 | P2 |

---

## 五、复现方式

```powershell
# 生成场景数据
python scripts/gen_demo_data.py --out %APPDATA%/LabelTorch/LabelTorch/e2e_b --count 48
python scripts/gen_demo_data.py --out %APPDATA%/LabelTorch/LabelTorch/e2e_anom --count 12 --defect-free-first 8

# S1/S2 检测全链路（示例项目 / 示例项目B）
set LT_DEBUG_OPEN_PROJECT=示例项目B
set LT_DEBUG_PAGE=training & set LT_DEBUG_AUTO_SNAPSHOT=1 & set LT_DEBUG_AUTO_TRAIN=1
set LT_DEBUG_TRAIN_EPOCHS=2
# 评估/导出：LT_DEBUG_PAGE=test / export + LT_DEBUG_AUTO_TEST=1 / LT_DEBUG_AUTO_EXPORT=1

# S3 异常全链路（示例项目异常）
set LT_DEBUG_TRAIN_ADAPTER=anomalib
```

钩子均为环境变量级，默认无操作，不影响正常使用；证据（日志/DB/截图）留存于 `%AppData%/LabelTorch/` 与本仓库 git 历史。
