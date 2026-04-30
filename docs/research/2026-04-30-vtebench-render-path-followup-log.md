# vtebench 渲染路径跟进优化报告

> **日期**: 2026-04-30  
> **范围**: `deepin-terminal-ghostty` 在上一轮文本 run 合并后的继续优化  
> **目标**: 继续降低 `TerminalWidget` paint 路径中的临时分配、冗余填充和 per-cell 查询成本  
> **相关数据**: `build/perf-results4.data`, `build/perf-results5.data`, `build/perf-results6.data`, `/home/hualet/projects/v/vtebench/results3.dat`, `/home/hualet/projects/v/vtebench/results5.dat`

---

## 1. 背景

上一轮已经通过文本 run 合并显著降低了 `QPainter::drawText()`、Qt
`QTextEngine` 和 Harfbuzz shaping 的成本。随后使用原版 deepin-terminal
数据做对比后，仍能看到几个明显差距：

- `cursor_motion` 仍明显慢于原版 deepin-terminal
- `unicode`、`medium_cells`、`sync_medium_cells` 仍有较大差距
- 区域滚动类 benchmark 仍受 `cursorScrollAbove` 和 paint 更新影响

本轮继续围绕 `TerminalWidget::renderRow()` 与 `renderTerminal()` 做小步优化，
同时记录一次失败尝试，避免后续重复走同一条路。

---

## 2. 已保留的修改

### 2.1 小 grapheme 使用栈缓冲

`textFromRenderCellGraphemes()` 原来会按 `graphemeLen` 创建
`std::vector<uint32_t>`。这对复杂 Unicode 文本是必要路径，但绝大多数
grapheme 很短。

本轮改为：

- `graphemeLen <= 8` 时使用 `std::array<uint32_t, 8>`
- 更长 grapheme 才退回 `std::vector<uint32_t>`
- 保留 `kMaxCellGraphemeCodepoints` 防御上限

这减少了 unicode 场景中小 grapheme 的 heap 分配。

### 2.2 单 codepoint 直接追加到 text run

上一轮 run 合并后，普通单 codepoint cell 仍会先构造一个临时 `QString`，
再 append 到当前 run。本轮增加 `appendTextRunCodepoint()`：

- 当前 run 可延续时，直接把 codepoint append 到 run 的 `QString`
- 当前 run 不可延续时，先 flush，再开始新 run
- 只用于非 inverse、无显式背景、非宽字符的单 codepoint cell

这样减少了普通 ASCII / Latin 文本路径中的临时 `QString` 构造。

### 2.3 避免终端内容区重复背景填充

`renderTerminal()` 原来每帧会先 `fillRect(rect(), bgWithAlpha)`，随后又把
backbuffer 画到 terminal content 区域。对于 content 区域，这次填充会被
backbuffer 覆盖。

本轮改为：

- 当 `terminalContentRect() == rect()` 时，不再额外填充 widget 背景
- 当存在 contents margins 时，只填充 content 外侧四个边缘区域
- backbuffer 内部仍按 dirty/full redraw 逻辑维护背景

这降低了 paint 路径中一块冗余的 QtGui 栅格化工作。

---

## 3. 失败尝试：raw-cell flags 快路径

### 3.1 尝试内容

为了减少普通 cell 的 style/color 查询，曾尝试在 `renderRow()` 开头先读取：

- `GHOSTTY_CELL_DATA_WIDE`
- `GHOSTTY_CELL_DATA_HAS_STYLING`
- `GHOSTTY_CELL_DATA_CONTENT_TAG`
- `GHOSTTY_CELL_DATA_CODEPOINT`

预期是：

- 无样式、窄字符、单 codepoint 的 cell 直接进入普通文本 run
- 有样式、宽字符、背景色或特殊 content tag 时再走完整路径

### 3.2 实测结果

这版代码对应：

- `build/perf-results4.data`
- `/home/hualet/projects/v/vtebench/results3.dat`

和上一轮较好结果 `results1.dat` 对比，均值反而明显变慢：

| Benchmark | `results1.dat` | raw-cell 快路径 | 比例 |
|-----------|----------------|-----------------|------|
| `cursor_motion` | 75.0ms | 121.7ms | 1.62x |
| `dense_cells` | 301.4ms | 596.3ms | 1.98x |
| `light_cells` | 55.6ms | 91.0ms | 1.64x |
| `medium_cells` | 118.3ms | 152.2ms | 1.29x |
| `scrolling_fullscreen` | 101.4ms | 125.7ms | 1.24x |
| `unicode` | 124.2ms | 165.5ms | 1.33x |

### 3.3 结论

这条路是负收益，已经回退。

主要原因判断为：

- `ghostty_cell_get_multi()` 额外读取多项 raw-cell flags 的成本较高
- 省掉的 style/color 查询不足以抵消新增查询
- `dense_cells` 退化最明显，说明额外查询会在高 cell 密度场景中被放大

后续如果要继续减少 cell 查询，应该优先考虑 Ghostty render API 层提供
row/run 级别的数据，而不是在 Qt 侧对每个 cell 追加更多查询。

---

## 4. 无效样本记录

曾尝试生成：

- `build/perf-results5.data`
- `/home/hualet/projects/v/vtebench/results4.dat`

该次运行没有使用外部 DISPLAY 权限，程序报错：

```text
could not connect to display :0
```

因此 `perf-results5.data` 只有约 49 KiB，不是有效样本。后续分析不使用这份
数据。

---

## 5. 当前版本数据

回退 raw-cell flags 快路径并保留低风险优化后，重新采样得到：

- `build/perf-results6.data`
- `/home/hualet/projects/v/vtebench/results5.dat`

当前数据与 `results1.dat`、`results2.dat` 对比如下。单位为毫秒，越低越好。

| Benchmark | `results1.dat` | `results2.dat` | `results5.dat` | `results5/results1` |
|-----------|----------------|----------------|----------------|---------------------|
| `cursor_motion` | 75.0 | 107.9 | 159.6 | 2.13x |
| `dense_cells` | 301.4 | 435.5 | 643.2 | 2.13x |
| `light_cells` | 55.6 | 88.8 | 81.3 | 1.46x |
| `medium_cells` | 118.3 | 155.4 | 180.7 | 1.53x |
| `scrolling` | 552.2 | 728.6 | 752.8 | 1.36x |
| `scrolling_bottom_region` | 515.0 | 694.1 | 726.6 | 1.41x |
| `scrolling_bottom_small_region` | 498.2 | 720.8 | 715.9 | 1.44x |
| `scrolling_fullscreen` | 101.4 | 153.7 | 160.3 | 1.58x |
| `scrolling_top_region` | 412.1 | 577.8 | 592.4 | 1.44x |
| `scrolling_top_small_region` | 387.2 | 599.8 | 635.9 | 1.64x |
| `sync_medium_cells` | 122.3 | 169.2 | 187.6 | 1.53x |
| `unicode` | 124.2 | 192.2 | 183.0 | 1.47x |

这组数据仍没有回到 `results1.dat` 的水平。结合上一轮 `results2.dat` 也整体变慢，
本轮无法证明 vtebench 均值有稳定收益。

不过 `perf-results6.data` 中热点形态没有出现新的异常：

| 模块 | 占比 |
|------|------|
| `deepin-terminal / libQt6Gui.so` | 40.53% |
| `Thread (pooled) / libc.so.6` | 36.88% |
| `deepin-terminal / libghostty-vt.so` | 10.66% |
| `deepin-terminal / libqtghostty.so` | 0.99% |

`TerminalWidget::renderRow()` 仍约 `0.53%`，说明本轮保留改动没有把热点转移到
我们自己的 C++ 代码里。QtGui 绘制、线程池中的 libc 消耗、Ghostty VT 写入仍是
主要瓶颈。

---

## 6. 验证

本轮执行过：

```bash
clang-format --dry-run --Werror src/libqtghostty/TerminalWidget.cpp
cmake --build build
./build/tests/test_terminal_widget -platform offscreen
```

结果：

- `clang-format`: 通过
- `cmake --build build`: 通过
- `test_terminal_widget`: `45 passed`

---

## 7. 阶段结论

本轮保留的是低风险微优化，但没有足够数据证明 vtebench 端到端稳定提升。

明确结论：

- raw-cell flags per-cell 快路径是负收益，已经撤回
- 小 grapheme 栈缓冲和单 codepoint 直追加可以保留，复杂度较低且测试覆盖通过
- content 区域冗余背景填充可以去掉，backbuffer 已负责 terminal content 绘制
- 当前最大差距仍不是这几个微优化能解决的，下一步应继续看 cursor/repaint 与
  VT 写入节奏，而不是继续在每个 cell 上追加查询

后续建议：

1. 对 `cursor_motion` 单独录制更短、更聚焦的 perf，隔离 repaint 频率和 cursor overlay。
2. 研究是否能对 cursor-only 更新跳过 backbuffer row render，只重绘旧/新 cursor rect。
3. 继续观察 QtGui `Thread (pooled)` 高占比，确认是否来自 glyph raster/cache 或平台插件线程。
4. 若需要进一步减少 cell 查询，应优先推动 Ghostty 侧提供 run/segment 级 render API。

---

## 8. 对应改动

本轮最终保留改动文件：

- `src/libqtghostty/TerminalWidget.cpp`

主要行为变化：

- 小 grapheme 使用栈缓冲
- 普通单 codepoint 直接追加到文本 run
- 避免 terminal content 区域被 backbuffer 覆盖前的冗余背景填充

