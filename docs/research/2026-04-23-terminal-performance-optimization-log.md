# 终端性能优化过程报告

> **日期**: 2026-04-23  
> **范围**: `deepin-terminal-ghostty` 终端重输出、TUI 动画、缩放与字体缩放场景的性能优化  
> **目标**: 在不引入明显输出迟滞的前提下，降低高频输出时的 CPU 占用，消除卡死和严重掉帧  
> **相关提交**: `e56ce4e`, `6ec8770`, `d2b8ccb`, `bb33fc1`

---

## 1. 初始现象

用户侧反馈集中在两个问题：

- 终端在大量输出或 TUI 动画下 CPU 很高，甚至会卡死
- 参考分支 `opcode` 已经做过一轮优化，CPU 有改善，但终端输出有明显迟滞感

最初判断是：

- `opcode` 的思路主要是固定 `16ms` 合帧，属于“用吞吐换延迟”
- 当前主线还保留了宽字符和输入法能力，不能为了性能把这些能力回退掉

因此本轮优化的约束被明确为：

1. 不能接受固定帧率节流导致的明显“发黏”
2. 不能回退宽字符、输入法、彩色渲染等已存在能力
3. 优先沿 `TerminalWidget` 的渲染与调度路径做低风险优化

---

## 2. 第 1 轮: 先压重绘压力

### 2.1 优化前判断

在没有第一份 `perf` 之前，最可能的热点看起来在两个方向：

- `paintEvent()` 每次都整屏重建
- PTY 数据高频到达时，`TerminalWidget` 刷新节奏过于频繁

### 2.2 修改

提交: `e56ce4e` `perf: reduce terminal repaint pressure`

这轮做了两类优化：

- 将 PTY 输入改成自适应调度
  - 首包尽快刷新
  - burst 在短窗口内合并
  - 避免 `opcode` 的固定 `16ms` 节流
- 给 `ghostty_render_state_update()` 加脏标记
  - 只有终端状态真的变化时才更新 render state
  - 光标闪烁、focus、输入法查询不再无条件触发整帧重算

### 2.3 验证

执行：

```bash
./build/tests/test_terminal_widget -platform offscreen
clang-format --dry-run --Werror src/libqtghostty/TerminalWidget.cpp src/libqtghostty/TerminalWidget.h tests/test_terminal_widget.cpp
```

结果：

- 自动化测试通过
- 体感上“严重卡死”问题已有缓解
- 但 `find .` 这类持续高输出下仍有明显延迟感
- 窗口放大、缩小和字体缩放仍可能掉帧

结论：

- 仅靠 repaint 调度优化还不够，需要真实 profiling 确认热点

---

## 3. 第 2 轮: 首次 profiling，确认主瓶颈不在 Qt 绘制

### 3.1 Profiling 方法

用户手动录制 `perf.data` 后进行分析。

### 3.2 关键数据

第一份有效 `perf` 的结论非常集中：

- `TerminalWidget::paintEvent()` 约 `0.7%`
- 约 `92%` CPU 在 `TerminalWidget::flushPendingPtyData() -> ghostty_terminal_vt_write()`
- 最热符号来自 `libghostty-vt`
  - `terminal.page.Page.verifyIntegrity`
  - `PageList.grow`
  - 滚屏和打印路径

### 3.3 结论

这说明：

- Qt 绘制不是当时的主瓶颈
- 真正的主耗时在 `libghostty-vt`
- 仓库里带的 `libghostty-vt` 很可能不是正常 release 构建

另一个侧面证据是库体积异常大：

- 旧库约 `32 MB`
- 很像带额外检查的慢路径构建

### 3.4 修改

提交: `6ec8770` `perf: reduce terminal output stalls`

这轮包含两部分：

1. Widget 侧继续优化
- PTY 驱动 repaint 合并
- rapid resize 合并
- dirty-row back buffer，避免每帧整屏逐 cell 重画

2. 替换 `libghostty-vt`
- 从本地 Ghostty 源码构建 `ReleaseFast` 版 `libghostty-vt`
- 替换仓库中打包的运行时库

### 3.5 验证

执行：

```bash
./build/tests/test_terminal_widget -platform offscreen
clang-format --dry-run --Werror src/libqtghostty/TerminalWidget.cpp src/libqtghostty/TerminalWidget.h tests/test_terminal_widget.cpp
```

结果：

- 自动化测试通过
- 用户体感反馈为“提升巨大”
- `find .`、缩放、字体缩放下的严重卡死基本消失

### 3.6 阶段结论

这轮是整次优化里收益最大的一轮。

原因不是简单“调度写得更聪明”，而是：

- 通过 `perf` 发现实际主瓶颈在 `libghostty-vt`
- 并确认仓库中的库本身就运行在慢路径
- 换成 `ReleaseFast` 运行时后，主耗时被整体拿掉

---

## 4. 第 3 轮: 第二次 profiling，热点转移到文本绘制

### 4.1 关键数据

替换 release 版 `libghostty-vt` 之后，新的 `perf` 热点发生了明显变化：

- `TerminalWidget::paintEvent()` 约 `4.13%`
- `renderTerminal()` 约 `4.07%`
- `renderRow()` 约 `3.37%`
- `QTextEngine::shapeTextWithHarfbuzzNG()` 约 `3.11%`
- `QTextEngine::itemize()` 约 `2.51%`
- `ghostty_terminal_vt_write()` 降到约 `1.42%`

`renderRow()` 的细粒度热点表现为：

- `QString::~QString()`
- `operator+(QString&&, QString const&)`
- `QFontEngineFT::loadGlyphFor()`
- 每 cell 文本 shaping 和字体切换

### 4.2 结论

在 release 版 `libghostty-vt` 下，真正值得抠的热点已经变成：

- 每个 cell 构造和销毁 `QString`
- 每个 cell 单独 `drawText`
- 每个 cell 触发 Qt itemize 和 Harfbuzz shaping
- 每个 cell 复制并改写 `QFont`

### 4.3 修改

提交: `d2b8ccb` `perf: batch terminal text shaping`

这轮核心是改写 `renderRow()`：

- 缓存 `QFont` 变体
  - 普通
  - 粗体
  - 斜体
  - 粗斜体
- 合并连续窄字符 run
  - 按“同字体 + 同前景色”的连续单元合并成一次 `drawText`
  - 宽字符继续走单独路径，避免影响双宽字符渲染
- 去掉热路径中的 `QString(QChar(...)) + text`
- 颜色字段改为单独查询，修复一次性能优化中引入的彩色回退问题

### 4.4 回归问题与修复

这轮中间出现过一次明显功能回退：

- 终端界面变成黑白

根因是：

- 把可选的 `fg_color` / `bg_color` 一起放进 `row_cells_get_multi()`
- 该 API 遇到第一个缺失字段就提前返回
- 导致很多 cell 没有成功取到前景色

修复方式：

- 固定字段继续 `get_multi`
- 可选颜色字段单独查询
- 增加 ANSI 前景色回归测试

### 4.5 验证

执行：

```bash
cmake --build build --target test_terminal_widget
./build/tests/test_terminal_widget -platform offscreen
clang-format --dry-run --Werror src/libqtghostty/TerminalWidget.cpp src/libqtghostty/TerminalWidget.h tests/test_terminal_widget.cpp
```

结果：

- 测试通过
- 用户反馈“速度更快了”
- 彩色渲染恢复正常

### 4.6 阶段结论

这轮说明 release 版 `libghostty-vt` 之上，QPainter 路径仍然有可观优化空间，但方式应该是：

- 减少绘制调用次数
- 减少 shaping 次数
- 减少热路径临时对象

而不是继续单纯抠 repaint 调度。

---

## 5. 第 4 轮: 第三次 profiling，热点回到 PTY 输入链路

### 5.1 关键数据

文本 run 合并之后，新一轮 `perf` 热点变成：

- `TerminalWidget::flushPendingPtyData()` 约 `11.29%`
- `ghostty_terminal_vt_write()` 约 `11.12%`
- `PtySession::handleMasterReadyRead()` 约 `8.17%`
- `QSocketNotifier::activated(...)` 约 `9.18%`

绘制侧已经明显下降：

- `paintEvent()` 约 `2.63%`
- `renderRow()` 约 `2.26%`
- `QTextEngine::itemize()` 约 `1.17%`
- `Harfbuzz shaping` 约 `0.39%`

### 5.2 结论

这次 profiling 的含义很明确：

- 文本绘制优化是有效的
- 当前主瓶颈已经从绘制回到“PTY 数据到 VT 状态机”的链路

真正的问题变成：

- PTY 有数据就很快走一次 Qt signal/slot
- 小块数据会导致频繁 `ghostty_terminal_vt_write()`
- `QSocketNotifier -> handleMasterReadyRead -> dataReceived -> onPtyDataReceived -> flushPendingPtyData`
  这条链路的调用次数太多

### 5.3 修改

提交: `bb33fc1` `perf: batch pty input delivery`

这轮做了两件事：

1. `PtySession` 侧前移聚合
- 在同一次 readiness callback 内尽量多读
- 增大读取块大小，从 `4096` 提到 `64 KiB`
- 聚合后只发一次 `dataReceived(QByteArray)`

2. `TerminalWidget` 侧调整 flush 策略
- 小 burst 先进入极短合并窗口
- 大包立即 flush
- 避免回到固定帧率节流导致的可见迟滞

### 5.4 验证

执行：

```bash
./build/tests/test_pty_session
./build/tests/test_terminal_widget -platform offscreen
clang-format --dry-run --Werror src/libqtghostty/PtySession.cpp src/libqtghostty/TerminalWidget.cpp src/libqtghostty/TerminalWidget.h tests/test_terminal_widget.cpp
```

额外新增回归覆盖：

- 小 burst 会合并成一次 VT flush
- 新调度下需要等待 flush 完成的时序测试

### 5.5 阶段结论

从设计上，这轮把“合并”前移到了更靠近 PTY 的位置，方向是对的。  
但是否真正显著降低了主瓶颈，还需要再用新一轮 `perf` 验证。

---

## 6. 第 5 轮: 最新 profiling，确认仍然是 PTY -> VT 写入主导

### 6.1 关键数据

在 `bb33fc1` 之后重新录制 `perf.data`，结果仍然显示主瓶颈集中在同一条路径：

- `TerminalWidget::flushPendingPtyData()` 约 `12.98%`
- `ghostty_terminal_vt_write()` 约 `12.79%`
- `QSocketNotifier::activated(...)` 约 `11.23%`
- `PtySession::handleMasterReadyRead()` 约 `9.98%`
- `read()` 约 `1.18%`

绘制侧维持较低水平：

- `paintEvent()` 约 `2.61%`
- `renderRow()` 约 `2.25%`
- `QTextEngine::itemize()` 约 `1.13%`
- `Harfbuzz shaping` 约 `0.43%`

### 6.2 结论

最新结论可以概括为两点：

1. 绘制已经不再是主要矛盾
- `renderRow()` 和 Qt shaping 都被压到了相对次要的位置

2. 终端大输出下，真正的主耗时已经集中在 VT 写入和其上游调度
- `ghostty_terminal_vt_write()` 本身仍然是最大的单点纯计算热点
- Qt 的 notifier/signal 链也还有明显开销

这说明当前状态下：

- 再继续抠 `renderRow()` 已经不是优先级最高的方向
- 后续如果还要继续优化，应该继续降低 `vt_write` 调用频率，或者进一步把批处理前移到 `PtySession`
- 更激进的优化收益会越来越依赖 `libghostty-vt` 内部处理效率，而不只是外层调度

---

## 7. 整体优化收益总结

### 7.1 关键收益

从用户体感和 profiling 的共同结论来看，今天这轮优化已经取得了三个确定成果：

1. 去掉了最严重的卡死
- 高输出场景下不再容易把窗口拖到近乎失去响应

2. 把主瓶颈从错误位置挪开了
- 最开始怀疑是 Qt 绘制
- 通过 `perf` 证明最大问题其实是 `libghostty-vt` 的构建与 VT 写入路径

3. 绘制路径已经被压到次要位置
- 文本 shaping 和 per-cell 绘制开销已明显下降

### 7.2 收益最大的动作

今天所有修改里，收益最大的是：

- 用 `perf` 确认仓库内 `libghostty-vt` 处于慢路径
- 换成 `ReleaseFast` 运行时库

如果没有这一步，后面的 Qt 层优化即使都做完，也只能在错误的热点旁边做微调。

### 7.3 当前剩余主瓶颈

截至最后一次 profiling，主瓶颈仍然是：

- `PtySession` 可读事件频繁触发
- `TerminalWidget::flushPendingPtyData()`
- `ghostty_terminal_vt_write()`

换句话说，剩余问题主要是：

- 如何进一步合并 PTY 输入批次
- 如何在不增加可感知迟滞的前提下降低 `vt_write` 次数
- 是否还需要进一步研究 Ghostty 内部的打印/滚屏路径

---

## 8. 过程中形成的经验

今天这轮优化里，比较明确的工程经验有四条：

1. 不要凭直觉假设热点在绘制
- 这次第一份 `perf` 直接证明主要问题在 `libghostty-vt`

2. 固定帧率节流不是这类终端的好解法
- CPU 会下降，但交互和输出会明显变黏

3. 性能优化必须同时守住功能回归
- 宽字符
- 输入法
- ANSI 颜色
- 彩色文本

4. 每做完一轮都要重新 profiling
- 热点会转移
- 上一轮最热的函数，下一轮可能已经不值得继续抠

---

## 9. 对应提交时间线

| 时间 | 提交 | 说明 |
|------|------|------|
| 2026-04-22 23:57 | `e56ce4e` | 自适应 repaint 合并 + render state dirty 缓存 |
| 2026-04-23 00:48 | `6ec8770` | dirty-row back buffer + resize 合并 + ReleaseFast `libghostty-vt` |
| 2026-04-23 01:00 | `d2b8ccb` | 文本 run 合并，减少 Qt shaping，修复颜色回退 |
| 2026-04-23 01:10 | `bb33fc1` | PTY 输入聚合 + 小 burst 短窗口 flush |

---

## 10. 已验证命令

今天过程中反复使用并通过的验证命令包括：

```bash
./build/tests/test_pty_session
./build/tests/test_terminal_widget -platform offscreen
clang-format --dry-run --Werror src/libqtghostty/PtySession.cpp src/libqtghostty/TerminalWidget.cpp src/libqtghostty/TerminalWidget.h tests/test_terminal_widget.cpp
```

性能分析主要依赖：

```bash
perf record ...
perf report --stdio -i perf.data
```

---

## 11. 当前状态

到 `bb33fc1` 为止，终端性能已经从“高输出时容易卡死”改善到“明显更顺，但剩余瓶颈主要集中在 PTY 输入批处理和 `ghostty_terminal_vt_write()` 本身”。

如果后续继续推进，最自然的下一步是：

- 进一步前移 PTY 聚合
- 继续减少 `vt_write` 次数
- 在必要时回到 Ghostty 内部路径继续做针对性 profiling

