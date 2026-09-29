/**
 * @file test_annotation_autosave.cpp
 * @brief 标注自动保存与多边形落盘回读测试
 *
 * 期望行为：
 * 1. 每次 addAnnotation / addPolygonAnnotation 后经 AnnotationService 原子落盘，
 *    重新 loadFromLabel 不丢标注
 * 2. 连续添加多个标注后一次保存，全部标注均可回读
 * 3. 多边形顶点坐标在保存/回读后保持一致
 * 4. 无类别（class_0）可正常保存，不得静默失败
 */
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QSignalSpy>

#include "AnnotationModel.h"
#include "AnnotationService.h"
#include "database/Database.h"

namespace {

bool writeTextFile(const QString &path, const QByteArray &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(content);
    f.close();
    return true;
}

} // namespace

class TestAnnotationAutosave : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // AnnotationService 落盘时写 annotation_revisions，需要可用的 SQLite
        const QString dbPath = m_dir.filePath(QStringLiteral("test.db"));
        QVERIFY(Database::instance().open(dbPath));
        QVERIFY(Database::instance().initializeSchema());
    }

    // 连续添加多个 HBB 后保存，回读数量一致（模拟连续绘制自动保存）
    void testMultipleAnnotationsPersist()
    {
        AnnotationModel model;
        model.addAnnotation(0, QStringLiteral("class_0"), 0.3f, 0.3f, 0.1f, 0.1f);
        model.addAnnotation(0, QStringLiteral("class_0"), 0.6f, 0.6f, 0.15f, 0.15f);
        model.addAnnotation(1, QStringLiteral("scratch"), 0.5f, 0.2f, 0.08f, 0.08f);
        QCOMPARE(model.rowCount(), 3);

        const QString labelPath = m_dir.filePath(QStringLiteral("multi.txt"));
        AnnotationService svc;
        svc.setShapeType(0);
        QVERIFY(svc.saveAnnotations(labelPath, QString(), QString(), model.toVariantList()));

        AnnotationModel loaded;
        loaded.loadFromLabel(labelPath);
        QCOMPARE(loaded.rowCount(), 3);

        // 校验几何未漂移
        auto idx = loaded.index(0, 0);
        QVERIFY(qAbs(loaded.data(idx, AnnotationModel::CxRole).toFloat() - 0.3f) < 1e-4f);
        idx = loaded.index(1, 0);
        QVERIFY(qAbs(loaded.data(idx, AnnotationModel::WRole).toFloat() - 0.15f) < 1e-4f);
        idx = loaded.index(2, 0);
        QCOMPARE(loaded.data(idx, AnnotationModel::ClassIndexRole).toInt(), 1);
    }

    // 多边形保存后顶点完整回读
    void testPolygonPointsPersist()
    {
        AnnotationModel model;
        QVector<QPointF> pts;
        pts << QPointF(0.10, 0.10) << QPointF(0.40, 0.12)
            << QPointF(0.35, 0.45) << QPointF(0.12, 0.38);
        model.addPolygonAnnotation(0, QStringLiteral("class_0"), pts);
        QCOMPARE(model.rowCount(), 1);

        const QString labelPath = m_dir.filePath(QStringLiteral("poly.txt"));
        AnnotationService svc;
        svc.setShapeType(2);
        QVERIFY(svc.saveAnnotations(labelPath, QString(), QString(), model.toVariantList()));

        AnnotationModel loaded;
        // 按多边形类型回读，顶点不得丢失
        loaded.loadFromLabel(labelPath, 2);
        QCOMPARE(loaded.rowCount(), 1);
        QCOMPARE(loaded.data(loaded.index(0, 0), AnnotationModel::ShapeTypeRole).toInt(), 2);
        QVariantList gotPts = loaded.data(loaded.index(0, 0), AnnotationModel::PointsRole).toList();
        QCOMPARE(gotPts.size(), 4);
        QVERIFY(qAbs(gotPts[0].toMap().value(QStringLiteral("x")).toFloat() - 0.10f) < 1e-4f);
        QVERIFY(qAbs(gotPts[2].toMap().value(QStringLiteral("y")).toFloat() - 0.45f) < 1e-4f);
        QCOMPARE(loaded.data(loaded.index(0, 0), AnnotationModel::ClassIndexRole).toInt(), 0);

        // 同时校验 reader 直读结果
        QVariantList polys = svc.loadPolygonAnnotations(labelPath);
        QCOMPARE(polys.size(), 1);
    }

    // 保存空列表应产生空标签文件（删除全部标注后自动保存的场景）
    void testSaveEmptyListClearsFile()
    {
        const QString labelPath = m_dir.filePath(QStringLiteral("empty.txt"));
        QVERIFY(writeTextFile(labelPath, "0 0.5 0.5 0.2 0.2\n"));

        AnnotationService svc;
        svc.setShapeType(0);
        QVERIFY(svc.saveAnnotations(labelPath, QString(), QString(), QVariantList()));

        QFile f(labelPath);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll().trimmed(), QByteArray());
    }

    // 无类别 class_0 可正常保存（禁止静默失败）
    void testDefaultClassZeroSaves()
    {
        AnnotationModel model;
        model.addAnnotation(0, QStringLiteral("class_0"), 0.5f, 0.5f, 0.2f, 0.2f);

        const QString labelPath = m_dir.filePath(QStringLiteral("cls0.txt"));
        AnnotationService svc;
        svc.setShapeType(0);
        QVERIFY(svc.saveAnnotations(labelPath, QString(), QString(), model.toVariantList()));

        QVariantList boxes = svc.loadAnnotations(labelPath);
        QCOMPARE(boxes.size(), 1);
        QCOMPARE(boxes[0].toMap().value(QStringLiteral("classIndex")).toInt(), 0);
    }

    // 追加第二个标注后再次保存，两个都在（验证「画第二个框后第一个丢失」的回归）
    void testSecondAnnotationDoesNotDropFirst()
    {
        AnnotationModel model;
        AnnotationService svc;
        svc.setShapeType(0);
        const QString labelPath = m_dir.filePath(QStringLiteral("append.txt"));

        model.addAnnotation(0, QStringLiteral("class_0"), 0.2f, 0.2f, 0.1f, 0.1f);
        QVERIFY(svc.saveAnnotations(labelPath, QString(), QString(), model.toVariantList()));

        // 模拟继续画第二个框
        model.addAnnotation(0, QStringLiteral("class_0"), 0.7f, 0.7f, 0.1f, 0.1f);
        QVERIFY(svc.saveAnnotations(labelPath, QString(), QString(), model.toVariantList()));

        AnnotationModel loaded;
        loaded.loadFromLabel(labelPath);
        QCOMPARE(loaded.rowCount(), 2);
    }

private:
    QTemporaryDir m_dir;
};

QTEST_MAIN(TestAnnotationAutosave)
#include "test_annotation_autosave.moc"
