#include <QTest>

class TestGeometry : public QObject
{
    Q_OBJECT
private slots:
    void testPlaceholder() { QVERIFY(true); }
};

QTEST_MAIN(TestGeometry)
#include "test_geometry.moc"
