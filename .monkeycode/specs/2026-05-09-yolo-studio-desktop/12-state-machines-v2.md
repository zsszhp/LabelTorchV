# 状态机规范 v2

## 1. 数据导入状态机

状态：

1. `idle`
2. `scanning`
3. `validating`
4. `reporting`
5. `completed`
6. `failed`

规则：

1. `scanning` 结束后必须进入 `validating`。
2. `failed` 必须保留失败上下文。

## 2. 标注保存状态机

状态：

1. `clean`
2. `dirty`
3. `saving`
4. `saved`
5. `save_failed`

规则：

1. 修改任何框后进入 `dirty`。
2. 保存成功后进入 `saved` 并回到 `clean`。

## 3. 训练任务状态机

状态：

1. `draft`
2. `validated`
3. `queued`
4. `running`
5. `stopping`
6. `succeeded`
7. `failed`
8. `cancelled`

## 4. 导出任务状态机

状态：

1. `pending`
2. `running`
3. `verifying`
4. `succeeded`
5. `failed`

## 5. 辅助标注批次状态机

状态：

1. `created`
2. `inferencing`
3. `reviewing`
4. `partially_confirmed`
5. `completed`
6. `failed`
