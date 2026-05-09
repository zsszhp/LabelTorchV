#include <QTest>

class TestIpc : public QObject
{
    Q_OBJECT
private slots:
    void testPlaceholder() { QVERIFY(true); }
};

QTEST_MAIN(TestIpc)
#include "test_ipc.moc"
