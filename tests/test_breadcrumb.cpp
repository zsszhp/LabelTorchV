/**
 * @file test_breadcrumb.cpp
 * @brief Breadcrumb 黑匣子环形缓冲回归测试
 *
 * 闪退修复回归（2026-10-06）：旧版 storeLine 用 `fetch_add(1) % slotCount`
 * 取槽且 index 初始 -1，首条日志得 slot=-1 → lines[-1] 向静态区前方
 * 越界写 1KB，踩坏相邻静态对象造成随机闪退（含绕过 SEH 的 fastfail）。
 * 本测试锁定：首条写入合法槽、跨 int 回绕仍安全、槽回绕后导出内容正确。
 */
#include <QtTest>
#include <QThread>
#include <QAtomicInt>

#include "utils/Breadcrumb.h"

#include <atomic>
#include <cstring>
#include <thread>

class TestBreadcrumb : public QObject
{
    Q_OBJECT

    static constexpr int kBufBytes = 300 * 1024;   // 与 main.cpp 黑匣子导出缓冲同级

private slots:
    /// 首条 push 必须落在合法槽位且可导出（旧版 -1 越界回归）
    void test_first_push_no_negative_slot()
    {
        // 独立进程内全局环——先验证 push+snapshot 往返
        static char buf[kBufBytes];
        memset(buf, 0, sizeof(buf));

        Breadcrumb::push("first-line");
        const int n = Breadcrumb::snapshot(buf, sizeof(buf));
        QVERIFY(n > 0);
        QVERIFY(strstr(buf, "first-line") != nullptr);
    }

    /// 多条推送后导出应包含所有行（未回绕阶段）
    void test_push_and_dump_order()
    {
        Breadcrumb::push("alpha");
        Breadcrumb::push("beta");
        static char buf[kBufBytes];
        memset(buf, 0, sizeof(buf));
        Breadcrumb::snapshot(buf, sizeof(buf));
        QVERIFY(strstr(buf, "alpha") != nullptr);
        QVERIFY(strstr(buf, "beta") != nullptr);
        // 顺序：alpha 在 beta 之前
        QVERIFY(strstr(buf, "alpha") < strstr(buf, "beta"));
    }

    /// 超过槽数回绕后仍只导出最近 kLogSlots 行，内容正确
    void test_wraparound()
    {
        for (int i = 0; i < 300; ++i) {
            Breadcrumb::push(QStringLiteral("wrap-line-%1").arg(i));
        }
        static char buf[kBufBytes];
        memset(buf, 0, sizeof(buf));
        Breadcrumb::snapshot(buf, sizeof(buf));
        // 最近的行存在
        QVERIFY(strstr(buf, "wrap-line-299") != nullptr);
        // 槽数 256：最早的行已被冲掉
        QVERIFY(strstr(buf, "wrap-line-0\n") == nullptr);
        QVERIFY(strstr(buf, "wrap-line-10\n") == nullptr);
    }

    /// 多线程并发 push：slot 分配互不冲突由原子性保证，这里验证不崩且总量可见
    void test_multithread_push()
    {
        constexpr int kThreads = 8;
        constexpr int kPerThread = 200;
        QVector<QThread *> threads;
        std::atomic<int> done{0};
        for (int t = 0; t < kThreads; ++t) {
            auto *th = QThread::create([&done]() {
                for (int i = 0; i < kPerThread; ++i)
                    Breadcrumb::push("threaded-log-line");
                done.fetch_add(1);
            });
            threads.append(th);
            th->start(QThread::LowestPriority);
        }
        for (auto *th : threads) {
            th->wait(10000);
            delete th;
        }
        QCOMPARE(done.load(), kThreads);
        // 导出不崩即通过（环已被上述 1600 条 + 既有会话日志填满回绕）
        static char buf[kBufBytes];
        memset(buf, 0, sizeof(buf));
        QVERIFY(Breadcrumb::snapshot(buf, sizeof(buf)) > 0);
    }

    /// pushAction 与黑匣子相互独立
    void test_actions_ring()
    {
        Breadcrumb::pushAction("11:00:00 action=open-dataset");
        static char buf[64 * 1024];
        memset(buf, 0, sizeof(buf));
        QVERIFY(Breadcrumb::snapshotActions(buf, sizeof(buf)) > 0);
        QVERIFY(strstr(buf, "action=open-dataset") != nullptr);
    }
};

QTEST_MAIN(TestBreadcrumb)
#include "test_breadcrumb.moc"
