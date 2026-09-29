/**
 * @file test_labelio_atomic.cpp
 * @brief P0-4 标签原子写入测试
 *
 * 期望行为：YoloTxtWriter 写入失败时，原文件内容必须完整保留。
 *
 * 覆盖场景：
 * 1. 临时文件无法创建（.tmp 路径被目录占位）→ 写入失败，原文件保留
 * 2. 目标文件被独占锁定（Windows）→ 写入失败，原文件保留
 * 3. 成功写入后无 .tmp 残留，内容被完整替换
 * 4. OBB / Polygon 写入同样遵守原子性契约
 */
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QFileInfo>

#include "labelio/YoloTxtReader.h"
#include "labelio/YoloTxtWriter.h"

#ifdef Q_OS_WIN
// NOGDI：排除 wingdi.h，避免其 Polygon 函数与标注结构体 Polygon 命名冲突
#define NOGDI
#include <windows.h>
#endif

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

QByteArray readAllBytes(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return f.readAll();
}

QVector<AxisAlignedBox> makeBoxes(int classIndex)
{
    QVector<AxisAlignedBox> v;
    AxisAlignedBox a;
    a.id = "ann-1";
    a.classIndex = classIndex;
    a.cx = 0.5f; a.cy = 0.5f; a.w = 0.2f; a.h = 0.2f;
    v.append(a);
    return v;
}

} // namespace

class TestLabelIOAtomic : public QObject
{
    Q_OBJECT

private slots:
    // P0-4 核心用例
    void testWriteFailureKeepsOriginal_TempBlocked();
    void testWriteFailureKeepsOriginal_DestLocked();
    void testWriteOBBCurveFailureKeepsOriginal();
    void testWritePolygonFailureKeepsOriginal();
    void testSuccessReplacesContentWithoutTmp();
};

// === P0-4 用例 ===

void TestLabelIOAtomic::testWriteFailureKeepsOriginal_TempBlocked()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QString filePath = tmpDir.path() + "/label.txt";

    const QByteArray original("0 0.5 0.5 0.1 0.1\n");
    QVERIFY(writeTextFile(filePath, original));

    // 用目录占位 .tmp 路径，使临时文件无法创建 → 写入必须失败
    QString tmpPath = filePath + ".tmp";
    QVERIFY(QDir().mkpath(tmpPath));
    QVERIFY(QFileInfo::exists(tmpPath) && QFileInfo(tmpPath).isDir());

    bool ok = YoloTxtWriter::write(filePath, makeBoxes(3));
    QVERIFY2(!ok, "临时文件无法创建时写入必须返回 false");

    // 关键契约：原文件内容不变
    QCOMPARE(readAllBytes(filePath), original);
}

void TestLabelIOAtomic::testWriteFailureKeepsOriginal_DestLocked()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QString filePath = tmpDir.path() + "/locked.txt";

    const QByteArray original("1 0.4 0.4 0.2 0.2\n");
    QVERIFY(writeTextFile(filePath, original));

#ifdef Q_OS_WIN
    // 以独占方式打开目标文件（不允许共享删除），使替换失败
    std::wstring wpath = filePath.toStdWString();
    HANDLE hFile = CreateFileW(wpath.c_str(), GENERIC_READ,
                               0 /* 无共享 */ , nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        QSKIP("无法独占打开目标文件，跳过该平台特定用例");
    }

    bool ok = YoloTxtWriter::write(filePath, makeBoxes(7));
    CloseHandle(hFile);

    QVERIFY2(!ok, "目标被独占锁定时写入必须返回 false");
    // 关键契约：原文件内容不变
    QCOMPARE(readAllBytes(filePath), original);
#else
    QSKIP("目标独占锁定用例仅在 Windows 上执行");
#endif
}

void TestLabelIOAtomic::testWriteOBBCurveFailureKeepsOriginal()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QString filePath = tmpDir.path() + "/obb.txt";

    const QByteArray original("0 0.1 0.1 0.2 0.1 0.2 0.2 0.1 0.2\n");
    QVERIFY(writeTextFile(filePath, original));

    // 阻塞临时文件
    QVERIFY(QDir().mkpath(filePath + ".tmp"));

    QVector<RotatedBox> empty;
    bool ok = YoloTxtWriter::writeOBB(filePath, empty);
    QVERIFY2(!ok, "OBB 写入失败时必须返回 false");
    QCOMPARE(readAllBytes(filePath), original);
}

void TestLabelIOAtomic::testWritePolygonFailureKeepsOriginal()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QString filePath = tmpDir.path() + "/poly.txt";

    const QByteArray original("0 0.1 0.1 0.2 0.1 0.2 0.2\n");
    QVERIFY(writeTextFile(filePath, original));

    QVERIFY(QDir().mkpath(filePath + ".tmp"));

    QVector<Polygon> empty;
    bool ok = YoloTxtWriter::writePolygon(filePath, empty);
    QVERIFY2(!ok, "Polygon 写入失败时必须返回 false");
    QCOMPARE(readAllBytes(filePath), original);
}

void TestLabelIOAtomic::testSuccessReplacesContentWithoutTmp()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QString filePath = tmpDir.path() + "/ok.txt";

    // 先写入旧内容
    QVERIFY(writeTextFile(filePath, QByteArray("9 0.9 0.9 0.1 0.1\n")));

    // 写入新标注
    QVector<AxisAlignedBox> anns = makeBoxes(5);
    QVERIFY2(YoloTxtWriter::write(filePath, anns), "正常写入应成功");

    // 无 .tmp 残留
    QVERIFY2(!QFile::exists(filePath + ".tmp"), "成功写入后不得残留临时文件");

    // 内容被完整替换为新标注
    QVector<AxisAlignedBox> read = YoloTxtReader::read(filePath);
    QCOMPARE(read.size(), 1);
    QCOMPARE(read[0].classIndex, 5);
}

QTEST_MAIN(TestLabelIOAtomic)
#include "test_labelio_atomic.moc"
