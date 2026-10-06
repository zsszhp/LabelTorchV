#include "Breadcrumb.h"

#include <QByteArray>

#include <atomic>
#include <cstring>

namespace {

constexpr int kLogSlots = 256;
constexpr int kLogSlotBytes = 1024;   // 单行上限，超长截断
constexpr int kActionSlots = 32;
constexpr int kActionSlotBytes = 256;

// 环形缓冲本体：静态预分配，崩溃路径零分配。原子索引保证多线程 push 各得唯一槽位；
// 槽数循环碰撞时可能撕裂单行，诊断用途可接受。
struct {
    std::atomic<int> index{-1};
    char lines[kLogSlots][kLogSlotBytes];
    int lengths[kLogSlots];
} s_log;

struct {
    std::atomic<int> index{-1};
    char lines[kActionSlots][kActionSlotBytes];
    int lengths[kActionSlots];
} s_actions;

// 拷贝一行入环（截断到槽容量-1）
template <int SlotBytes>
void storeLine(std::atomic<int> &index, char (*lines)[SlotBytes],
               int *lengths, int slotCount, const char *utf8)
{
    if (!utf8 || !*utf8) return;
    const int slot = index.fetch_add(1) % slotCount;
    const int len = static_cast<int>(qstrnlen(utf8, SlotBytes - 1));
    memcpy(lines[slot], utf8, len);
    lines[slot][len] = '\0';
    lengths[slot] = len;
}

// 按时间序导出环内全部行到 out（NUL 结尾），返回写入字节数
template <int SlotBytes>
int dumpLines(std::atomic<int> &index, const char (*lines)[SlotBytes],
              const int *lengths, int slotCount,
              char *out, int outBytes)
{
    const int last = index.load();
    if (last < 0 || !out || outBytes <= 0) return 0;
    const int available = (last + 1 < slotCount) ? (last + 1) : slotCount;
    const int first = last + 1 - available;   // 取模后即最旧槽位

    int written = 0;
    for (int i = 0; i < available && written < outBytes - 1; ++i) {
        const int slot = (first + i) % slotCount;
        const int len = lengths[slot];
        if (len <= 0) continue;
        const int copy = (len < outBytes - 1 - written) ? len : (outBytes - 1 - written);
        memcpy(out + written, lines[slot], copy);
        written += copy;
        if (written < outBytes - 1) out[written++] = '\n';
    }
    out[written] = '\0';
    return written;
}

} // namespace

namespace Breadcrumb {

void push(const char *utf8Line)
{
    storeLine<kLogSlotBytes>(s_log.index, s_log.lines, s_log.lengths, kLogSlots, utf8Line);
}

void pushAction(const char *utf8Line)
{
    storeLine<kActionSlotBytes>(s_actions.index, s_actions.lines,
                                s_actions.lengths, kActionSlots, utf8Line);
}

int snapshot(char *out, int outBytes)
{
    return dumpLines<kLogSlotBytes>(s_log.index, s_log.lines, s_log.lengths,
                                    kLogSlots, out, outBytes);
}

int snapshotActions(char *out, int outBytes)
{
    return dumpLines<kActionSlotBytes>(s_actions.index, s_actions.lines, s_actions.lengths,
                                       kActionSlots, out, outBytes);
}

} // namespace Breadcrumb
