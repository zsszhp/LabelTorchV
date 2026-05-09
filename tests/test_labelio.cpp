#include <QTest>

class TestLabelIO : public QObject
{
    Q_OBJECT
private slots:
    void testPlaceholder() { QVERIFY(true); }
};

QTEST_MAIN(TestLabelIO)
#include "test_labelio.moc"
