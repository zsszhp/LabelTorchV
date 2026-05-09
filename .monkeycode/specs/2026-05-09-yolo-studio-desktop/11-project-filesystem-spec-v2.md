# 项目文件系统规范 v2

## 1. 目标

为项目目录建立稳定、可升级、可追溯的本地文件结构，避免数据、模型、缓存、导出混乱堆叠。

## 2. 项目根目录结构

```text
<project_root>/
  project.json
  data/
    datasets/
    snapshots/
    taxonomy/
    revisions/
  models/
    versions/
    runs/
  exports/
  cache/
    thumbnails/
    temp/
  logs/
  diagnostics/
```

## 3. 目录含义

### 3.1 data/datasets

存放导入后的数据集元信息与引用，不建议复制全部原图，除非用户选择托管模式。

### 3.2 data/snapshots

存放训练用数据快照清单。

### 3.3 data/taxonomy

存放项目类别体系版本与映射版本记录。

### 3.4 data/revisions

存放标签修订前后快照或差异记录。

### 3.5 models/runs

存放训练运行日志、临时结果、原始输出。

### 3.6 models/versions

存放已注册模型版本的正式产物。

### 3.7 exports

存放正式导出产物。

### 3.8 cache

存放缩略图、临时图像、导入扫描缓存。

## 4. 文件命名要求

1. 使用稳定 id 而不是用户可变名称作为内部目录名。
2. 用户可见名称保存在元数据中。
3. 导出文件名应包含版本标识与格式后缀。

## 5. 安全规则

1. 所有系统生成文件必须在项目根目录下。
2. 不允许将临时文件留在正式目录长期不清理。
3. 删除缓存不得影响正式数据和模型版本。
