# GPU 开发机交接指南（实施级）

> 版本：2.0
> 日期：2026-06-08
> 适用对象：在带 NVIDIA GPU 的 Windows 电脑上继续开发 LabelTorch 的人员或 AI
> 基线版本：v0.2.0（commit `7c4b1d8` 已推送 GitHub + Gitee）
> 配套文档：`docs/执行计划-v0.3.x-产线硬化.md`（主任务）、`docs/下一阶段蓝图-v0.3至v2.0.md`（路线）

---

## 一、为什么需要交接

当前开发机 **无 GPU**。v0.2.0 已完成：稳定性收口、体验闭环、CPU 绿色包、测试门禁、文档基线。

**必须在 GPU 机完成**：

| 工作 | 原因 |
|------|------|
| CUDA 版 PyTorch 打包验证 | CPU 机无法打出/验证 cu121/cu124 包 |
| 30/40/50 系显卡训练 | 需实机显卡与驱动 |
| 显存预检校准 | batch 建议依赖真实显存占用 |
| 训练性能基线 | 单 epoch 耗时、吞吐 |
| v1.1 增量训练精度验证 | 需要 GPU 训练 |

**CPU 机也可做**：标注 UI（v1.0 大部分）、文档、数据治理（v2.0 E/F）、导出逻辑。

---

## 二、克隆与基线确认

### 2.1 克隆（二选一）

```powershell
# SSH（推荐，需已配密钥）
git clone git@github.com:zsszhp/LabelTorchV.git D:\z\project\my\LabelTorchV

# 或 HTTPS
git clone https://gitee.com/zzsszhp/LabelTorchV.git D:\z\project\my\LabelTorchV

cd D:\z\project\my\LabelTorchV
```

### 2.2 确认基线

```powershell
git log -5 --oneline
# 期望看到（新到旧）：
# 7c4b1d8 新增v0.3至v2.0阶段蓝图、分阶段执行计划与GPU开发机交接指南
# 9a84158 清理过时脚本与临时文档
# 2cd9b21 v0.2.0 稳定性收口与体验闭环

git status
# 期望：working tree clean
```

### 2.3 必读文档（按顺序）

| 顺序 | 文档 | 用途 |
|------|------|------|
| 1 | `CLAUDE.md` | 架构、目录、规范（AI 助手必读） |
| 2 | `docs/下一阶段蓝图-v0.3至v2.0.md` | 版本路线 |
| 3 | `docs/执行计划-v0.3.x-产线硬化.md` | **本机主任务** |
| 4 | `docs/产品优化实施计划.md` | v0.2 已做什么，避免重复 |
| 5 | `docs/用户手册/03-快速上手.md` | 产品主流程 |
| 6 | `docs/GPU开发机交接指南.md`（本文） | 环境与交接细节 |

---

## 三、环境安装（逐步）

### 3.1 硬件与驱动自检

```powershell
nvidia-smi
```

记录输出中的：驱动版本、CUDA Version、显卡型号、显存。

**通过标准**：驱动 ≥ 535，CUDA Version ≥ 12.0，显存 ≥ 6GB（8GB+ 更佳）。

### 3.2 安装 Visual Studio 2022

1. 下载 VS 2022 Community
2. 工作负荷勾选「使用 C++ 的桌面开发」
3. 单个组件确认：MSVC v145、Windows 10/11 SDK（10.0.26100 或相近）
4. 安装后打开「x64 Native Tools Command Prompt for VS 2022」验证：

```powershell
where cl
# 期望：C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\...\cl.exe
# 注意：VS 版本路径可能是 17/Community 或 18/Community，以实际为准
```

### 3.3 安装 Qt 6.11.1

1. Qt 在线安装器，勾选 **Qt 6.11.1 → MSVC 2022 64-bit**
2. 建议路径：`C:\Qt\6.11.1\msvc2022_64`
3. 验证：

```powershell
Test-Path C:\Qt\6.11.1\msvc2022_64\bin\qmake.exe
Test-Path C:\Qt\6.11.1\msvc2022_64\bin\windeployqt.exe
```

### 3.4 安装 Miniconda/Anaconda 与 Python 环境

```powershell
# 1. 安装 Miniconda 到 D:\A\anaconda（或 F:\A\anaconda，按你习惯）

# 2. 创建环境
conda create -n labeltorch python=3.11 -y

# 3. 激活
conda activate labeltorch

# 4. 先装 CPU 基线依赖（跑测试用）
cd D:\z\project\my\LabelTorchV
pip install -r backend/requirements.txt

# 5. 再装 GPU 版 torch（覆盖 CPU 版）
# 选一个，与你的 CUDA Toolkit 一致：
pip install torch torchvision --index-url https://download.pytorch.org/whl/cu121
# 或
pip install torch torchvision --index-url https://download.pytorch.org/whl/cu124

# 6. 其余依赖
pip install ultralytics onnxruntime-gpu opencv-python Pillow numpy
# 异常检测（可选，v0.3 后做 anomalib 打包才必需）
pip install anomalib
```

### 3.5 验证 GPU 训练环境

```powershell
conda activate labeltorch
python -c "import torch; print('torch', torch.__version__); print('cuda', torch.cuda.is_available()); print('device', torch.cuda.get_device_name(0) if torch.cuda.is_available() else 'N/A'); print('capability', torch.cuda.get_device_capability(0) if torch.cuda.is_available() else 'N/A')"
```

**期望输出示例**：

```
torch 2.4.1+cu121
cuda True
device NVIDIA GeForce RTX 4090
capability (8, 9)
```

若 `cuda False`：

```powershell
python -c "import torch; print(torch.version.cuda)"
nvidia-smi
# 1) torch.version.cuda 为 None → 装的是 CPU 版，重装 cuXXX
# 2) 有 CUDA 但仍 False → 驱动过旧或 DLL 冲突，检查 PATH 是否混入其他 CUDA
```

### 3.6 配置环境变量（用户级，永久）

```powershell
# PowerShell 中执行（改成本机实际路径）
[Environment]::SetEnvironmentVariable("QT_ROOT", "C:\Qt\6.11.1\msvc2022_64", "User")
[Environment]::SetEnvironmentVariable("LABELTORCH_PYTHON_ENV", "D:\A\anaconda\envs\labeltorch", "User")
[Environment]::SetEnvironmentVariable("PYTHON_ENV", "D:\A\anaconda\envs\labeltorch", "User")

# 重开终端后验证
$env:QT_ROOT
$env:LABELTORCH_PYTHON_ENV
```

**为什么要设**：`CMakePresets.json`、`cmake/FindPythonEnv.cmake`、`scripts/build_green_package.ps1` 都读这些变量；不设会回退到写死的默认路径（可能不存在）。

---

## 四、构建与测试（逐步）

### 4.1 配置

打开「x64 Native Tools Command Prompt for VS 2022」或先执行：

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
# 路径按实际 VS 版本调整
```

然后：

```powershell
cd D:\z\project\my\LabelTorchV

cmake --preset x64-release
```

若 preset 失败，手动配置：

```powershell
cmake -B out/build/x64-release `
  -DCMAKE_PREFIX_PATH="C:\Qt\6.11.1\msvc2022_64" `
  -DLABELTORCH_PYTHON_ENV="D:\A\anaconda\envs\labeltorch" `
  -DCMAKE_BUILD_TYPE=Release
```

### 4.2 编译

```powershell
cmake --build out/build/x64-release
```

**通过标准**：`LabelTorchV` 目标 Built，0 错误。

### 4.3 C++ 测试

```powershell
cd out\build\x64-release
ctest --output-on-failure
```

**通过标准**：`100% tests passed, 0 tests failed out of 22`

若 IpcRestartTest 耗时约 29 秒，属正常（测指数退避）。

### 4.4 Python 测试

```powershell
cd D:\z\project\my\LabelTorchV\backend
& "$env:LABELTORCH_PYTHON_ENV\python.exe" -m pytest tests -q
& "$env:LABELTORCH_PYTHON_ENV\python.exe" -m ruff check .
```

**通过标准**：`48 passed`（或更多），ruff `All checks passed!`

### 4.5 启动主程序

```powershell
cd D:\z\project\my\LabelTorchV
.\out\build\x64-release\LabelTorchV.exe
```

**手工检查清单**：

- [ ] 窗口正常打开（深色工业风，8 项导航分组）
- [ ] 状态栏显示 GPU 名 + CUDA 版本（绿色），而不是「GPU: 不可用」
- [ ] 底部日志面板可展开，能显示后端日志
- [ ] 新建项目 → 导入数据集 → 训练页能选到数据冻结版
- [ ] 无闪退

### 4.6 冒烟：一次 GPU 训练

1. 准备小数据集（十几张图 + YOLO txt 即可）
2. 建项目（任务类型 detect）→ 导入 → 创建数据冻结版
3. 训练：epochs=5、batch=8、imgsz=640、device=auto
4. 另开终端观察：

```powershell
nvidia-smi
# 期望：出现 python 进程，显存占用上升
```

5. 训练结束后：测试页看指标 → 导出 onnx → 验证通过

---

## 五、本机主任务（v0.3.x）

按 `docs/执行计划-v0.3.x-产线硬化.md` 顺序执行：

| 顺序 | 任务 | 产出 |
|------|------|------|
| 1 | G1 GPU 版 torch 打包 | cu121/cu124 绿色包 |
| 2 | G2 显卡测试矩阵 | `docs/test-matrix/gpu/*.md` 记录 |
| 3 | G3 显存预检与 batch 建议 | 新 API + UI 回填 |
| 4 | G4 IFW 安装器实包 | 可安装 exe |
| 5 | G5 万张 4K 压测 | 性能基线表 |
| 6 | G6-G9 空态/FormField/日志级别/崩溃恢复 | UI 与稳定性收尾 |

### 测试记录怎么做

1. 复制 `docs/test-matrix/gpu/TEMPLATE.md`
2. 另存为 `docs/test-matrix/gpu/2026-MM-DD-RTX4090.md`（日期-显卡）
3. 每个场景填：结果、耗时、显存峰值、`nvidia-smi` 截图文件名
4. 截图存 `docs/test-matrix/gpu/images/`

---

## 六、提交与推送

```powershell
# 确认无敏感信息、无 deploy/ out/ 二进制
git status

git add <改动文件>
git commit -m "中文描述，一件事"

# 双平台
git push github main
git push gitee main
```

**规范**：

- commit 全文中文，不带类型前缀，不带 Co-Authored-By
- 不提交 `deploy/`、`out/`、日志、zip（已在 .gitignore）
- 一个 commit 可编译
- 修 Bug 先写失败测试

发版时：

```powershell
scripts\sync_version.ps1 -CheckOnly   # 必须通过
git tag v0.3.0
git push github v0.3.0
git push gitee v0.3.0
```

---

## 七、常见问题速查

| 现象 | 原因 | 处理 |
|------|------|------|
| `type_traits: No such file` | 未进 VS 环境 | 跑 vcvars64.bat |
| `Could not find Qt6` | QT_ROOT 未生效 | 设环境变量或 `-DCMAKE_PREFIX_PATH` |
| 后端「未连接」 | Python 路径错 | 查 `LABELTORCH_PYTHON_ENV`，看日志面板 |
| `torch.cuda.is_available()==False` | 装错 torch | 重装 cuXXX 版 |
| LNK1104 exe 被锁 | 测试进程残留 | 结束进程或 `Move-Item` 重命名旧 exe |
| PowerShell 中文乱码 | .ps1 编码 | 存 UTF-8 with BOM |
| ctest 挂起无输出 | GUI 子系统 | 用 `-o file,txt` 或看退出码 |
| 打包后缺 DLL | trim 白名单裁掉 | 对照 G1 保留 CUDA/SSL DLL |

---

## 八、环境核对清单（打勾后再开工）

- [ ] nvidia-smi 正常
- [ ] cl.exe 可用
- [ ] Qt 6.11.1 msvc2022_64 存在
- [ ] conda 环境 labeltorch 可激活
- [ ] torch.cuda.is_available() == True
- [ ] 环境变量 QT_ROOT / LABELTORCH_PYTHON_ENV 已设
- [ ] cmake --preset x64-release 成功
- [ ] cmake --build 成功
- [ ] ctest 全绿
- [ ] pytest 全绿
- [ ] 主程序启动且状态栏显示 GPU

全部打勾后，进入 `docs/执行计划-v0.3.x-产线硬化.md` 开工。
