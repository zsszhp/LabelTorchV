# LabelTorchV 项目规范

## 项目概述
标炬（LabelTorch）- 面向工业缺陷检测的本地离线视觉数据治理与模型闭环平台。
核心工作流：导入数据集 → 数据快照 → 训练 → 推理/辅助标注 → 导出模型（pt/onnx）
支持任务类型：detect / obb / classify / anomaly
详细架构见 CLAUDE.md

## 命令
- 配置：`cmake --preset msvc2022-release`
- 构建：`cmake --build --preset msvc2022-release`
- 测试C++：`ctest --preset msvc2022-release`
- 测试Python：`cd backend && python -m pytest tests/`
- Lint：`cd backend && python -m ruff check .`

## 🔴 强制验证（AI每次修改后必须执行）
- **修改C++/QML/CMake → 必须构建确认编译通过**（曾出过提交编译错误代码的问题）
- **修改功能代码 → 必须运行相关测试**（防止静默引入Bug）
- **修改Python代码 → 必须运行ruff check**（Lint错误必须修复）
- **完成功能开发 → 必须执行代码审查**（检查逻辑/性能/安全/规范/资源泄漏）

## 架构硬约束（绝对红线）
1. 坚持Qt+C++ + Python混合架构，不使用纯Python方案（项目定位是桌面工业软件）
2. 训练永远依赖数据快照，不直接依赖数据集实时状态（数据一致性保障）
3. 模型版本必须可追溯到训练任务和数据快照（工业审计要求）
4. 不可逆操作必须可审计（annotation_revisions / task_events）
5. 离线优先，不依赖云端服务（工业现场可能无网络）
6. IPC进程隔离：Python崩溃不影响主进程（稳定性保障）
7. 数据快照不可变：创建后不可修改
8. 标注修订追踪：每次保存自动创建修订记录
9. 导出产物验证：必须经过artifact.verify验证

## Git规范
- 单人开发，仅用main分支，不创建feature分支
- 提交格式：`<type>: <中文描述>`
  feat / fix / refactor / docs / test / perf / style / chore / ci
- 每次提交必须推送到GitHub + Gitee双平台
- 版本tag格式：v{major}.{minor}.{patch}

### 发布前检查清单
- [ ] Release编译通过
- [ ] 全量C++测试通过
- [ ] 全量Python测试通过
- [ ] ruff check无错误
- [ ] 冒烟测试通过
- [ ] CMakeLists.txt版本号已更新

## 代码规范（核心）
- C++/Python/QML必须加注释（除非用户要求不加）
- 界面语言仅中文，禁止占位符，禁止硬编码路径
- C++：禁止裸new/delete，用QObject父子树/智能指针（防内存泄漏）
- C++：日志用ltInfo(LT_LOG_XXX())，不用qDebug()（统一日志系统）
- C++：所有ID用UUID（Id::generate()），Service间依赖用setXxx()注入
- QML：颜色/字体/间距/圆角必须用Theme.xxx，禁止硬编码（UI一致性）
- Python：async/await模式，Handler签名`async def handle_xxx(payload: dict) -> dict`
- 文件命名：C++ PascalCase / QML PascalCase / Python snake_case

## 错误处理（不允许静默失败）
- 所有外部调用（IPC/文件IO/数据库/QProcess）必须有错误处理和日志
- IPC调用必须检查success字段，文件IO必须检查QFile::open()返回值
- 数据库操作必须检查SQL执行结果，QProcess必须连接errorOccurred信号
- Python Handler必须try/except，异常返回{"success": false, "error": {...}}
- QML Service调用失败必须向用户展示错误信息，不允许静默忽略

## 线程安全
- 禁止主线程执行耗时操作（>16ms），用QtConcurrent/QThreadPool
- 跨线程操作必须用信号槽或QMetaObject::invokeMethod（Qt线程模型要求）
- IpcClient响应回调在读取线程，更新UI必须切到主线程

## 原子操作
- 文件写入必须原子性：先写临时文件再rename（防止崩溃导致半写损坏）
- 数据库写操作必须用事务，多表关联操作在同一事务内

## 性能
- 大数据集必须分页/虚拟化加载（>1000样本时必须分页，工业数据集可能上万张）
- 图片加载必须异步（ThumbnailCache + ThumbnailGenerator）
- 禁止用Repeater加载大量数据，用ListView+delegate按需渲染

## 安全
- 禁止硬编码密钥/密码/Token，禁止日志输出敏感信息
- 文件路径必须校验合法性防路径遍历（QFileInfo::canonicalFilePath()）

## 测试
- 新增Service/核心逻辑必须编写测试（覆盖正常/边界/错误三条路径）
- 修Bug必须先写失败测试用例再修复（防回归，曾出过同一Bug反复出现的问题）

## 数据库迁移
- Schema变更必须写迁移脚本（src/core/database/migrations/，格式M{N}_描述.sql）
- 迁移脚本必须幂等，包含UP和DOWN两部分

## IPC健壮性
- 所有IPC请求必须设置超时（默认30秒）
- Python崩溃后IpcClient必须自动重启（最多5次），重启后发environment.check确认就绪
- 超时请求必须从pendingCommands移除（防内存泄漏）

## MVP边界
MVP包含：导入数据集 → 训练 → 导出模型（pt/onnx）
MVP不包含：标注功能、辅助标注、数据治理、多模型支持

## 环境路径
- Qt：C:/Qt/6.11.1/msvc2022_64
- Python：C:/A/anaconda/envs/labeltorch/python.exe
- 数据库：QStandardPaths::AppDataLocation/labeltorch.db

## 踩坑记录
- Qt 6.11 Debug模式下NaN会触发qFatal退出，main.cpp已做降级处理，不要删除
- QML模块输出目录受QTP0004 NEW策略影响，不要手动修改输出路径
- 跑构建前确保MSVC环境已加载（cmake preset会自动处理）
