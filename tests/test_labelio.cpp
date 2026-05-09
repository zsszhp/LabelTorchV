#include <QTest>
#include <QTemporaryDir>
#include "labelio/YoloTxtReader.h"
#include "labelio/YoloTxtWriter.h"

class TestLabelIO : public QObject
{
    Q_OBJECT
private slots:
    void testParseLine();
    void testParseLineInvalid();
    void testWriteAndRead();
    void testAtomicWrite();
    void testEmptyFile();
};

void TestLabelIO::testParseLine()
{
    AxisAlignedBox ann = YoloTxtReader::parseLine("3 0.500000 0.500000 0.400000 0.300000");
    QCOMPARE(ann.classIndex, 3);
    QCOMPARE(ann.cx, 0.5f);
    QCOMPARE(ann.cy, 0.5f);
    QCOMPARE(ann.w, 0.4f);
    QCOMPARE(ann.h, 0.3f);
    QVERIFY(!ann.id.isEmpty());
}

void TestLabelIO::testParseLineInvalid()
{
    // Wrong number of parts
    AxisAlignedBox a1 = YoloTxtReader::parseLine("1 0.5 0.5");
    QCOMPARE(a1.classIndex, -1);

    // Negative class_id
    AxisAlignedBox a2 = YoloTxtReader::parseLine("-1 0.5 0.5 0.3 0.3");
    QCOMPARE(a2.classIndex, -1);

    // Coordinate out of range
    AxisAlignedBox a3 = YoloTxtReader::parseLine("0 1.5 0.5 0.3 0.3");
    QCOMPARE(a3.classIndex, -1);
}

void TestLabelIO::testWriteAndRead()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QString filePath = tmpDir.path() + "/test_label.txt";

    QVector<AxisAlignedBox> annotations;
    AxisAlignedBox a;
    a.id = "test-1"; a.classIndex = 0; a.cx = 0.5f; a.cy = 0.5f; a.w = 0.4f; a.h = 0.3f;
    annotations.append(a);

    AxisAlignedBox b;
    b.id = "test-2"; b.classIndex = 1; b.cx = 0.3f; b.cy = 0.7f; b.w = 0.2f; b.h = 0.1f;
    annotations.append(b);

    QVERIFY(YoloTxtWriter::write(filePath, annotations));

    QVector<AxisAlignedBox> read = YoloTxtReader::read(filePath);
    QCOMPARE(read.size(), 2);
    QCOMPARE(read[0].classIndex, 0);
    QCOMPARE(read[1].classIndex, 1);

    // Verify coordinates are close (float precision)
    QVERIFY(qAbs(read[0].cx - 0.5f) < 0.001f);
    QVERIFY(qAbs(read[0].cy - 0.5f) < 0.001f);
}

void TestLabelIO::testAtomicWrite()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QString filePath = tmpDir.path() + "/atomic_test.txt";

    QVector<AxisAlignedBox> annotations;
    AxisAlignedBox a;
    a.classIndex = 5; a.cx = 0.1f; a.cy = 0.2f; a.w = 0.3f; a.h = 0.4f;
    annotations.append(a);

    QVERIFY(YoloTxtWriter::write(filePath, annotations));

    // No temp file should remain
    QVERIFY(!QFile::exists(filePath + ".tmp"));

    // File should be readable
    QVector<AxisAlignedBox> read = YoloTxtReader::read(filePath);
    QCOMPARE(read.size(), 1);
    QCOMPARE(read[0].classIndex, 5);
}

void TestLabelIO::testEmptyFile()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QString filePath = tmpDir.path() + "/empty.txt";

    QVector<AxisAlignedBox> empty;
    QVERIFY(YoloTxtWriter::write(filePath, empty));

    QVector<AxisAlignedBox> read = YoloTxtReader::read(filePath);
    QCOMPARE(read.size(), 0);
}

QTEST_MAIN(TestLabelIO)
#include "test_labelio.moc"
