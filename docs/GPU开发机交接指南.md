# GPU 开发机交接指南

> 版本：1.0
> 日期：2026-06-08
> 适用对象：在带 NVIDIA GPU 的另一台 Windows 电脑上继续开发 LabelTorch 的人员（含 AI 助手）
> 基线版本：v0.2.0（已推送 GitHub + Gitee）

---

## 一、为什么需要交接

当前开发机 **无 GPU**，已完成 v0.2.0 的功能开发、稳定性收口与 CPU 版交付。以下工作必须在 GPU 机完成：

1. CUDA 版 PyTorch 打包与验证
2. 30/40/50 系显卡真实训练
3. 显存预检与 batch 建议校准
4. 万张图性能压测（与 GPU 推理/训练相关部分）
5. v1.1 增量训练的精度验证

CPU 可完成的工作（标注 UI、文档、数据治理、导出逻辑）可在任一机器进行。

---

## 二、拿到仓库后第一步

### 2.1 克隆与确认

```powershell
# 二选一
git clone git@github.com:zsszhp/LabelTorchV.git
git clone https://gitee.com/zzsszhp/LabelTorchV.git

cd LabelTorchV
git log -3 --oneline
# 应看到：清理过时脚本与临时文档 / v0.2.0 稳定性收口与体验闭环
```

### 2.2 阅读顺序

1. `CLAUDE.md` — 项目架构与规范（AI 必读）
2. `docs/下一阶段蓝图-v0.3至v2.0.md` — 路线图
3. `docs/执行计划-v0.3.x-产线硬化.md` — **GPU 机主任务**
4. `docs/产品优化实施计划.md` — v0.2.0 已做内容，避免重复
5. `docs/用户手册/03-快速上手.md` — 理解产品流

---

## 三、环境安装

### 3.1 基础工具

| 组件 | 版本 | 获取方式 |
|------|------|----------|
| Windows 10/11 x64 | — | — |
| Visual Studio 2022 | v145 工具集 | 官网，勾选「使用 C++ 的桌面开发」 |
| CMake | 3.22+ | 或 Qt 自带 |
| Ninja | — | Qt Tools 或独立安装 |
| Qt | **6.11.1** msvc2022_64 | 官方安装器 |
| Git | 任意 | — |
| NVIDIA 驱动 | 最新 Studio/Game Ready | nvidia.com |
| CUDA Toolkit | 12.1 或 12.4 | 与 torch 对应 |

### 3.2 Python 环境（conda）

```powershell
# 建议安装到 D:\A\anaconda 或 F:\A\anaconda（可配置，禁止硬编码依赖）
conda create -n labeltorch python=3.11 -y
conda activate labeltorch

# CPU 开发用
pip install -r backend/requirements.txt

# GPU 训练用（按 CUDA 版本选）
# CUDA 12.1
pip install torch torchvision --index-url https://download.pytorch.org/whl/cu121
# CUDA 12.4
pip install torch torchvision --index-url https://download.pytorch.org/whl/cu124

pip install ultralytics onnxruntime-gpu opencv-python Pillow numpy
pip install anomalib  # 可选，异常检测
```

### 3.3 环境变量（推荐写入用户环境变量）

| 变量 | 示例 | 说明 |
|------|------|------|
| `QT_ROOT` | `C:\Qt\6.11.1\msvc2022_64` | CMake 找 Qt |
| `LABELTORCH_PYTHON_ENV` | `D:\A\anaconda\envs\labeltorch` | 后端 Python 环境 |
| `PYTHON_ENV` | 同上 | 旧别名兼容 |

构建脚本会读取这些变量；不设则回退到本机路径（可能失败）。

### 3.4 验证 GPU 环境

```powershell
conda activate labeltorch
python -c "import torch; print(torch.__version__, torch.cuda.is_available(), torch.cuda.get_device_name(0) if torch.cuda.is_available() else 'no gpu')"
```

期望：`True` + 显卡名（如 `NVIDIA GeForce RTX 4090`）。

---

## 四、构建与测试

### 4.1 配置与编译

```powershell
# 在 VS 开发者命令行或已跑 vcvars64 的环境
cd D:\z\project\my\LabelTorchV   # 换成你的路径

# 配置（路径可被环境变量覆盖）
cmake --preset x64-release

# 编译
cmake --build out/build/x64-release
```

若 preset 不可用：

```powershell
cmake -B out/build/x64-release -G "NMake Makefiles JOM" `
  -DCMAKE_PREFIX_PATH="$env:QT_ROOT" `
  -DLABELTORCH_PYTHON_ENV="$env:LABELTORCH_PYTHON_ENV" `
  -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/x64-release
```

### 4.2 测试

```powershell
# C++
cd out/build/x64-release
ctest --output-on-failure

# Python
cd backend
& "$env:LABELTORCH_PYTHON_ENV\python.exe" -m pytest tests -q
& "$env:LABELTORCH_PYTHON_ENV\python.exe" -m ruff check .
```

**通过标准**：ctest 全绿、pytest 全绿、ruff 无错误。

### 4.3 运行主程序

```powershell
.\out\build\x64-release\LabelTorchV.exe
```

首次启动会连接 Python 后端；状态栏应显示 GPU 名与 CUDA 版本。

---

## 五、GPU 机主任务（v0.3.x）

按 `docs/执行计划-v0.3.x-产线硬化.md` 执行，优先级：

1. **确认 torch CUDA 可用**，跑通一次 YOLOv8 小数据集训练
2. **记录显卡信息与训练指标**（见下表模板）
3. **GPU 版绿色包**：`build_green_package.ps1` 增加 CUDA 分支
4. **显存预检**：按显存给 batch 建议并实测校准
5. **万张 4K 压测**
6. **IFW 安装器实包**

### 测试记录模板（保存到 docs/test-matrix/gpu/）

```markdown
# GPU 测试记录

- 日期：
- 显卡：
- 驱动版本：
- CUDA：
- torch：
- Windows：

| 场景 | 结果 | 耗时 | 显存峰值 | 备注 |
|------|------|------|----------|------|
| YOLOv8 detect 训练 100 epoch | | | | |
| YOLOv8 obb 训练 | | | | |
| 异常检测 efficient_ad | | | | |
| ONNX 导出+验证 | | | | |
| 万张图导入浏览 | | | | |
```

---

## 六、注意事项

1. **禁止硬编码路径**：Qt/Python 路径一律走环境变量或 CMake 缓存变量
2. **不要提交** `deploy/`、`out/`、日志、绿色包 zip（已在 .gitignore）
3. **commit 中文描述**，不带 AI 痕迹，不带 Co-Authored-By
4. **一个 commit 一件事**，每个 commit 可编译
5. 修 Bug 先写失败测试
6. 发现与本指南不符的路径/版本，以实际机器为准并回写更新本文档

---

## 七、常见问题

| 现象 | 处理 |
|------|------|
| `type_traits: No such file` | 未进 VS 开发者环境，先跑 `vcvars64.bat` |
| `find_package(Qt6)` 失败 | 设 `QT_ROOT` 或 `-DCMAKE_PREFIX_PATH` |
| Python 后端连不上 | 检查 `LABELTORCH_PYTHON_ENV`，看日志面板 / `logs/` |
| torch.cuda.is_available() 为 False | 驱动 / CUDA / torch 版本不匹配，重装对应 cuXXX 版 torch |
| 链接 LNK1104 exe 被锁 | 杀残留测试进程或重命名旧 exe |
| PowerShell 跑 .ps1 中文乱码 | 保存为 UTF-8 with BOM |

---

## 八、完成后

1. 补全 `docs/test-matrix/gpu/` 测试记录
2. 更新 `CHANGELOG.md`（v0.3.0）
3. `scripts/sync_version.ps1 -CheckOnly` 通过
4. 绿色包冒烟通过
5. 推送双平台：`git push github main` + `git push gitee main`
