#include <QTest>
#include "geometry/AxisAlignedBox.h"

class TestGeometry : public QObject
{
    Q_OBJECT
private slots:
    void testAxisAlignedBoxBasics();
    void testHitTest();
    void testIntersects();
    void testIou();
    void testIsValid();
};

void TestGeometry::testAxisAlignedBoxBasics()
{
    AxisAlignedBox box;
    box.cx = 0.5f; box.cy = 0.5f; box.w = 0.4f; box.h = 0.3f;

    QCOMPARE(box.left(), 0.3f);
    QCOMPARE(box.top(), 0.35f);
    QCOMPARE(box.right(), 0.7f);
    QCOMPARE(box.bottom(), 0.65f);
}

void TestGeometry::testHitTest()
{
    AxisAlignedBox box;
    box.cx = 0.5f; box.cy = 0.5f; box.w = 0.4f; box.h = 0.4f;

    QVERIFY(box.hitTest(0.5f, 0.5f));   // center
    QVERIFY(box.hitTest(0.3f, 0.3f));   // corner
    QVERIFY(!box.hitTest(0.1f, 0.1f));  // outside
    QVERIFY(!box.hitTest(0.9f, 0.9f));  // outside
}

void TestGeometry::testIntersects()
{
    AxisAlignedBox a;
    a.cx = 0.3f; a.cy = 0.3f; a.w = 0.4f; a.h = 0.4f;

    AxisAlignedBox b;
    b.cx = 0.5f; b.cy = 0.5f; b.w = 0.4f; b.h = 0.4f;

    QVERIFY(a.intersects(b));
    QVERIFY(b.intersects(a));

    AxisAlignedBox c;
    c.cx = 0.9f; c.cy = 0.9f; c.w = 0.1f; c.h = 0.1f;

    QVERIFY(!a.intersects(c));
}

void TestGeometry::testIou()
{
    AxisAlignedBox a;
    a.cx = 0.25f; a.cy = 0.25f; a.w = 0.5f; a.h = 0.5f;  // [0, 0.5] x [0, 0.5]

    AxisAlignedBox b;
    b.cx = 0.5f; b.cy = 0.5f; b.w = 0.5f; b.h = 0.5f;  // [0.25, 0.75] x [0.25, 0.75]

    float iou = a.iou(b);
    QVERIFY(iou > 0.0f);
    QVERIFY(iou < 1.0f);

    // Identical boxes should have IoU = 1.0
    float selfIou = a.iou(a);
    QCOMPARE(selfIou, 1.0f);

    // Non-overlapping boxes should have IoU = 0
    AxisAlignedBox d;
    d.cx = 0.9f; d.cy = 0.9f; d.w = 0.1f; d.h = 0.1f;
    QCOMPARE(a.iou(d), 0.0f);
}

void TestGeometry::testIsValid()
{
    AxisAlignedBox valid;
    valid.cx = 0.5f; valid.cy = 0.5f; valid.w = 0.4f; valid.h = 0.3f;
    QVERIFY(valid.isValid());

    AxisAlignedBox zeroW;
    zeroW.cx = 0.5f; zeroW.cy = 0.5f; zeroW.w = 0.0f; zeroW.h = 0.3f;
    QVERIFY(!zeroW.isValid());

    AxisAlignedBox outOfRange;
    outOfRange.cx = 1.5f; outOfRange.cy = 0.5f; outOfRange.w = 0.4f; outOfRange.h = 0.3f;
    QVERIFY(!outOfRange.isValid());
}

QTEST_MAIN(TestGeometry)
#include "test_geometry.moc"
