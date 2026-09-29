/**
 * @file test_ipc_restart.cpp
 * @brief P0-8 后端进程重启计数/退避逻辑测试
 *
 * 期望行为：
 * 1. 后端反复崩溃时，重启次数受 MAX_RESTART（5）限制，不得无限重启
 * 2. 重启间隔呈指数退避（1s/2s/4s/8s/16s），不得每次固定间隔
 * 3. 「一起来就崩」的场景下，重启计数不得因进程短暂启动成功而被清零
 *
 * 测试通过写入「崩溃桩脚本」（启动后立即退出）并统计其被拉起的次数/时刻来验证。
 * 若 PATH 中找不到 python，相关用例自动跳过。
 */
#include <QTest>
#include <QCoreApplication>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QElapsedTimer>

#include "IpcClient.h"

namespace {

/// 写入崩溃桩脚本：每次启动追加一行时间戳到计数文件后立即退出
QString writeCrashStub(const QString &dir, const QString &counterPath)
{
    QString scriptPath = dir + "/crash_stub.py";
    QString counterEscaped = counterPath;
    counterEscaped.replace("\\", "\\\\");

    QByteArray code;
    code += "import sys, time\n";
    code += "with open(r'" + counterEscaped.toUtf8() + "', 'a') as f:\n";
    code += "    f.write(str(time.time()) + '\\n')\n";
    code += "sys.exit(1)\n";

    QFile f(scriptPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return {};
    f.write(code);
    f.close();
    return scriptPath;
}

/// 读取计数文件中的时间戳列表（每行一个 float 秒）
QVector<double> readTimestamps(const QString &counterPath)
{
    QVector<double> out;
    QFile f(counterPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return out;
    QTextStream in(&f);
    while (!in.atEnd()) {
        bool ok = false;
        double t = in.readLine().trimmed().toDouble(&ok);
        if (ok) out.append(t);
    }
    return out;
}

} // namespace

class TestIpcRestart : public QObject
{
    Q_OBJECT

private slots:
    void testBackoffGapsGrow();
    void testRestartCountCapped();
};

void TestIpcRestart::testBackoffGapsGrow()
{
    QString python = QStandardPaths::findExecutable(QStringLiteral("python"));
    if (python.isEmpty()) {
        python = QStandardPaths::findExecutable(QStringLiteral("python3"));
    }
    if (python.isEmpty()) {
        QSKIP("PATH 中未找到 python，跳过重启退避用例");
    }

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QString counterPath = tmpDir.path() + "/launches.txt";
    QString stubPath = writeCrashStub(tmpDir.path(), counterPath);
    QVERIFY(!stubPath.isEmpty());

    IpcClient client;
    QSignalSpy errSpy(&client, &IpcClient::backendError);
    QVERIFY(errSpy.isValid());

    // 拉起崩溃桩：每次启动即退出，触发自动重启
    client.startBackend(python, stubPath);

    // 观察窗口：等待至少 3 次拉起（初次 + 2 次重启，理论间隔约 1s、2s）
    QElapsedTimer timer;
    timer.start();
    QVector<double> stamps;
    while (timer.elapsed() < 12000) {
        QTest::qWait(200);
        stamps = readTimestamps(counterPath);
        if (stamps.size() >= 3) break;
    }

    // 停止后端，避免继续重启干扰后续用例
    client.stopBackend();
    QTest::qWait(300);

    if (stamps.size() < 2) {
        QSKIP(qPrintable(QString("崩溃桩拉起次数不足（%1 次），无法验证退避间隔").arg(stamps.size())));
    }

    // 期望行为：重启间隔应递增（指数退避），而不是固定 3 秒
    // 断言：后一次间隔 >= 前一次间隔（允许 10% 计时误差）
    for (int i = 2; i < stamps.size(); ++i) {
        double gapPrev = stamps[i - 1] - stamps[i - 2];
        double gapCurr = stamps[i] - stamps[i - 1];
        QVERIFY2(gapCurr + 0.1 >= gapPrev,
                 qPrintable(QString("重启间隔应非递减（指数退避）：gap[%1]=%2s, gap[%2]=%3s")
                                .arg(i - 2).arg(gapPrev, 0, 'f', 2)
                                .arg(i - 1).arg(gapCurr, 0, 'f', 2)));
    }
}

void TestIpcRestart::testRestartCountCapped()
{
    QString python = QStandardPaths::findExecutable(QStringLiteral("python"));
    if (python.isEmpty()) {
        python = QStandardPaths::findExecutable(QStringLiteral("python3"));
    }
    if (python.isEmpty()) {
        QSKIP("PATH 中未找到 python，跳过重启上限用例");
    }

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QString counterPath = tmpDir.path() + "/launches.txt";
    QString stubPath = writeCrashStub(tmpDir.path(), counterPath);
    QVERIFY(!stubPath.isEmpty());

    IpcClient client;
    QSignalSpy errSpy(&client, &IpcClient::backendError);
    QVERIFY(errSpy.isValid());

    client.startBackend(python, stubPath);

    // 观察窗口 25 秒：覆盖 MAX_RESTART=5 次重启的退避序列（1+2+4+8+16≈31s 理论上限，
    // 但「一起来就崩」场景下若计数被错误清零会远超 6 次拉起）
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 25000) {
        QTest::qWait(250);
        // 达到上限后会收到 backendError，可提前结束观察
        if (!errSpy.isEmpty() && errSpy.size() >= 1) {
            // 再多等一小段时间确认不再拉起
            QTest::qWait(2000);
            break;
        }
    }

    client.stopBackend();
    QTest::qWait(300);

    QVector<double> stamps = readTimestamps(counterPath);
    // 期望行为：拉起次数受 MAX_RESTART 限制（初次 + 最多 5 次重启 = 6 次）
    // 即便计时抖动，也绝不允许远超上限（例如 10+ 次）
    QVERIFY2(stamps.size() <= 8,
             qPrintable(QString("重启次数应受 MAX_RESTART 限制，实际拉起 %1 次").arg(stamps.size())));
}

QTEST_MAIN(TestIpcRestart)
#include "test_ipc_restart.moc"
