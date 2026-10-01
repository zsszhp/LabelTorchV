#ifndef DEMOBOOTSTRAP_H
#define DEMOBOOTSTRAP_H

#include <QObject>
#include <QString>

class ProjectService;
class TaxonomyService;
class DatasetService;
class TagService;
class AppController;

/**
 * @brief 示例项目引导（演示模式）。
 *
 * 首次运行（projects 表为空）或用户在项目页点击「载入示例项目」时，
 * 通过各 Service 公共 API 创建与 UI 同构的示例项目：
 *   - 合成缺陷检测数据集（12 图 / 3 类别：划痕、凹陷、污染）
 *   - 类别体系（随导入自动生成后按索引规范命名）
 *   - 预置标签（默认/良品/漏检/误检/待定/重要）
 *
 * 幂等：已存在同名项目时直接返回其 id，不重复创建。
 * 全程走 Service 公共 API，与用户在 UI 中手动创建完全同构。
 */
class DemoBootstrap : public QObject
{
    Q_OBJECT

public:
    explicit DemoBootstrap(QObject *parent = nullptr);

    void setServices(ProjectService *projects, TaxonomyService *taxonomies,
                     DatasetService *datasets, TagService *tags);

    /// 确保示例项目存在并返回其项目 id（失败返回空串）
    Q_INVOKABLE QString ensureDemoProject();

    /// 示例项目是否缺失（供 UI 决定是否显示「载入示例项目」按钮）
    Q_INVOKABLE bool demoProjectMissing();

private:
    /// 将内置示例资源释放到 AppData/demo（幂等），返回解压根目录
    QString extractDemoAssets();

    ProjectService *m_projects = nullptr;
    TaxonomyService *m_taxonomies = nullptr;
    DatasetService *m_datasets = nullptr;
    TagService *m_tags = nullptr;
};

#endif // DEMOBOOTSTRAP_H
