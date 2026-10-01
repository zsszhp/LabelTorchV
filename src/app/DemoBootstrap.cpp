#include "DemoBootstrap.h"

#include "AppController.h"
#include "ProjectService.h"
#include "TaxonomyService.h"
#include "DatasetService.h"
#include "TagService.h"
#include "utils/AuditLog.h"
#include "utils/Log.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>

namespace {

constexpr int kDemoImageCount = 12;
constexpr const char *kDemoProjectName = "示例项目";
constexpr const char *kDemoDatasetName = "示例数据集";
// 与 scripts/gen_demo_data.py 的类别顺序保持一致（索引即 YOLO class_id）
const char *kDemoClassNames[3] = {"划痕", "凹陷", "污染"};
const char *kDemoTagNames[6] = {"默认", "良品", "漏检", "误检", "待定", "重要"};

} // namespace

DemoBootstrap::DemoBootstrap(QObject *parent) : QObject(parent) {}

void DemoBootstrap::setServices(ProjectService *projects, TaxonomyService *taxonomies,
                                DatasetService *datasets, TagService *tags)
{
    m_projects = projects;
    m_taxonomies = taxonomies;
    m_datasets = datasets;
    m_tags = tags;
}

QString DemoBootstrap::extractDemoAssets()
{
    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                         + QStringLiteral("/demo");
    const QString imageDir = root + QStringLiteral("/images");
    const QString labelDir = root + QStringLiteral("/labels");
    QDir().mkpath(imageDir);
    QDir().mkpath(labelDir);

    for (int i = 0; i < kDemoImageCount; ++i) {
        const QString stem = QStringLiteral("panel_%1").arg(i, 3, 10, QLatin1Char('0'));
        // 图片 12 张全量释放；标签仅前 10 张存在（后 2 张演示「未标注」流转）
        const QString imageRes = QStringLiteral(":/demo/images/") + stem + QStringLiteral(".jpg");
        const QString imageDst = imageDir + QLatin1Char('/') + stem + QStringLiteral(".jpg");
        if (QFile::exists(imageRes) && !QFile::exists(imageDst))
            QFile::copy(imageRes, imageDst);

        const QString labelRes = QStringLiteral(":/demo/labels/") + stem + QStringLiteral(".txt");
        const QString labelDst = labelDir + QLatin1Char('/') + stem + QStringLiteral(".txt");
        if (QFile::exists(labelRes) && !QFile::exists(labelDst))
            QFile::copy(labelRes, labelDst);
    }
    return root;
}

bool DemoBootstrap::demoProjectMissing()
{
    if (!m_projects) return true;
    const QVariantList all = m_projects->listProjects();
    for (const QVariant &entry : all) {
        if (entry.toMap().value("name").toString() == QString::fromUtf8(kDemoProjectName))
            return false;
    }
    return true;
}

QString DemoBootstrap::ensureDemoProject()
{
    if (!m_projects || !m_taxonomies || !m_datasets || !m_tags) {
        ltError(LT_LOG_APP()) << "DemoBootstrap services not wired";
        return {};
    }

    // 幂等：已有同名项目直接返回
    const QVariantList all = m_projects->listProjects();
    for (const QVariant &entry : all) {
        const QVariantMap m = entry.toMap();
        if (m.value("name").toString() == QString::fromUtf8(kDemoProjectName))
            return m.value("id").toString();
    }

    // 1. 释放内置示例资源
    const QString demoRoot = extractDemoAssets();

    // 2. 创建项目（createProject 内部会建立空类别体系）
    const QString projectId = m_projects->createProject(QString::fromUtf8(kDemoProjectName),
                                                        demoRoot, QStringLiteral("detect"));
    if (projectId.isEmpty()) {
        ltError(LT_LOG_APP()) << "Failed to create demo project";
        return {};
    }

    // 3. 导入示例数据集（importDatasetSeparate 会把标签中的类别并入第一个类别体系）
    const QString datasetId = m_datasets->importDatasetSeparate(
        projectId, QString::fromUtf8(kDemoDatasetName),
        demoRoot + QStringLiteral("/images"),
        demoRoot + QStringLiteral("/labels"));
    if (datasetId.isEmpty()) {
        ltError(LT_LOG_APP()) << "Failed to import demo dataset";
        return projectId; // 项目已建，数据集失败不回滚（可手动重试导入）
    }

    // 4. 类别规范化：导入器对无名字 YOLO 标签生成的类别名不可控，
    //    这里按索引统一改名为「划痕/凹陷/污染」（与生成脚本顺序一致）
    const QVariantList taxonomies = m_taxonomies->listTaxonomies(projectId);
    if (!taxonomies.isEmpty()) {
        const QString taxonomyId = taxonomies.first().toMap().value("id").toString();
        const QVariantList classes = m_taxonomies->getClasses(taxonomyId);
        if (classes.isEmpty()) {
            for (const char *name : kDemoClassNames)
                m_taxonomies->addClass(taxonomyId, QString::fromUtf8(name));
        } else {
            for (int i = 0; i < 3 && i < classes.size(); ++i)
                m_taxonomies->renameClass(taxonomyId, i, QString::fromUtf8(kDemoClassNames[i]));
        }
    }

    // 5. 预置标签
    for (const char *tag : kDemoTagNames)
        m_tags->addTag(datasetId, QString::fromUtf8(tag));

    AuditLog::record(QStringLiteral("project"), projectId,
                     QStringLiteral("demo_created"),
                     {{QStringLiteral("datasetId"), datasetId},
                      {QStringLiteral("images"), kDemoImageCount}});

    ltInfo(LT_LOG_APP()) << "Demo project ensured:" << projectId
                         << "dataset:" << datasetId;
    return projectId;
}
