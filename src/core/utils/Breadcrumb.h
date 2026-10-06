#ifndef BREADCRUMB_H
#define BREADCRUMB_H

#include <QString>

// 崩溃安全的日志黑匣子：预分配环形缓冲，无锁（原子索引），崩溃路径零堆分配。
// 用途：程序崩溃时由异常过滤器导出"崩溃前最近日志"与"最后用户动作"，
// 回答"闪退前发生了什么"。push 可在任意线程调用；snapshot 系列只应在
// 崩溃处理器/terminate 处理器内使用（并发写入可能产生个别撕裂行，可接受）。
namespace Breadcrumb {

// 记录一行日志（任意线程；超长截断）
void push(const char *utf8Line);

// 记录一条用户动作（UserAction 专用轻量环，容量小、保留最后 N 条）
void pushAction(const char *utf8Line);

// 崩溃路径：按时间序导出日志环到 out（NUL 结尾），返回写入字节数
int snapshot(char *out, int outBytes);

// 崩溃路径：按时间序导出用户动作环到 out（NUL 结尾），返回写入字节数
int snapshotActions(char *out, int outBytes);

// 便捷重载：QString/QByteArray 转 UTF-8 后入环（常规路径，允许分配）
inline void push(const QString &line) { push(line.toUtf8().constData()); }
inline void pushAction(const QString &line) { pushAction(line.toUtf8().constData()); }
inline void push(const QByteArray &line) { push(line.constData()); }
inline void pushAction(const QByteArray &line) { pushAction(line.constData()); }

} // namespace Breadcrumb

#endif // BREADCRUMB_H
