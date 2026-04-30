# vtebench 文本渲染优化报告

> **日期**: 2026-04-30  
> **范围**: `deepin-terminal-ghostty` 在 `vtebench` 高吞吐输出场景下的 QPainter 文本渲染优化  
> **目标**: 降低逐 cell 文本绘制导致的 Qt/Harfbuzz shaping 开销，提高 `vtebench` 吞吐  
> **相关数据**: `build/perf.data`, `/home/hualet/projects/v/vtebench/results.dat`, `/home/hualet/projects/v/vtebench/results1.dat`

---

## 1. 背景

前一轮性能优化后，`deepin-terminal-ghostty` 已经解决了高输出场景下最严重的卡死问题，并把主要瓶颈从错误构建的 `libghostty-vt` 慢路径中移开。

本轮用户使用以下方式重新录制性能数据：

```bash
perf record -g ./deepin-terminal-ghostty \
  --working-directory /home/hualet/projects/v/vtebench/ \
  --execute cargo run --release -- --dat results.dat
```

随后在同一场景下重新测试，输出更新为 `results1.dat`。本报告记录这轮针对 `vtebench` 和 `perf.data` 的分析、修改与结果。

---

## 2. 优化前现象

### 2.1 vtebench 数据

优化前 `results.dat` 中，多个 benchmark 样本数很少，说明 10 秒窗口内吞吐不足，尤其是滚动相关场景：

| Benchmark | 中位数(ms) | 有效样本 |
|-----------|------------|----------|
| `light_cells` | 96.5 | 90 |
| `cursor_motion` | 144.5 | 64 |
| `medium_cells` | 209.0 | 47 |
| `dense_cells` | 325.0 | 30 |
| `scrolling` | 1369.5 | 8 |
| `scrolling_bottom_region` | 1300.0 | 8 |
| `scrolling_top_region` | 1107.5 | 10 |
| `sync_medium_cells` | 224.0 | 44 |

其中滚动类 benchmark 每个约 1 MiB 样本需要 1 秒以上，表现明显偏慢。

### 2.2 perf 热点

优化前 `perf.data` 显示热点集中在终端进程本身，而不是 benchmark 子进程：

- `deepin-terminal`: 约 `80.19%`
- `Thread (pooled)`: 约 `18.96%`
- `benchmark`: 约 `0.56%`

按动态库看，最重的是 Qt 文本绘制链路：

| 模块/符号 | 开销 |
|-----------|------|
| `libQt6Gui.so` | 约 `45.40%` |
| `libharfbuzz.so` | 约 `9.18%` |
| `QTextEngine::shapeTextWithHarfbuzzNG()` | 约 `3.38%` |
| `QTextEngine::itemize()` | 约 `1.13%` |
| `QPainter::drawText(...)` | 约 `0.87%` |
| `QTextEngine::shapeText(...)` | 约 `0.87%` |
| `QFontEngine::getGlyphPositions(...)` | 约 `0.51%` |

### 2.3 结论

这轮数据说明当时的主要问题已经不是 `vtebench` 自身，也不是 PTY 读写，而是 `TerminalWidget::renderRow()` 的绘制模型：

- 每个普通 cell 都构造一个 `QString`
- 每个 cell 单独调用一次 `QPainter::drawText()`
- 大量单字符文本反复触发 Qt itemize、Harfbuzz shaping 和 glyph layout
- 空白 cell 也会走文本绘制路径

这正好解释了 `light_cells`、`medium_cells` 和滚动类 benchmark 的表现。

---

## 3. 修改方案

### 3.1 文本 run 合并

核心修改在 `TerminalWidget::renderRow()`：

- 新增行内 `TextRun`
- 对连续窄字符做合并
- 合并条件保持保守：
  - 窄字符
  - 同字体
  - 同前景色
  - 无显式背景色
  - 非 inverse
- 遇到以下情况立即 flush 当前 run：
  - 背景色
  - inverse
  - 宽字符
  - 字体或前景色变化

这样可以把一整段普通 ASCII 文本从“每个 cell 一次 `drawText()`”变成“一段文本一次 `drawText()`”。

### 3.2 单 codepoint 快路径

普通单 codepoint cell 不再通过 grapheme buffer 构造文本，而是直接从 `GhosttyCell` 取 `GHOSTTY_CELL_DATA_CODEPOINT`：

- 降低 `std::vector<uint32_t>` 临时分配
- 降低 `QString` 构造和析构压力
- 保留多 codepoint grapheme 的旧路径，避免影响组合字符

### 3.3 空白 cell 跳过文本绘制

`graphemeLen == 0` 的窄 cell 不再构造空格并绘制：

- 背景仍然在行开始时整体填充
- 有显式背景色的 cell 仍然保留背景填充
- 纯空白文本不再进入 `drawText()`

### 3.4 测试覆盖

新增 `testCoalescesPlainTextIntoRenderRuns()`：

- 输入一行连续普通文本
- 触发渲染
- 验证本帧文本绘制 run 数显著低于逐 cell 绘制上限

同时保留并复跑既有渲染测试：

- 宽字符跨双 cell 渲染
- ANSI 前景色渲染
- supplementary plane 字符
- 长 grapheme cell
- dirty row 增量渲染

---

## 4. 优化后结果

### 4.1 vtebench 对比

`results.dat` 为优化前，`results1.dat` 为优化后。单位为毫秒，越低越好。

| Benchmark | 优化前中位数 | 优化后中位数 | 提升倍数 | 优化前样本 | 优化后样本 |
|-----------|--------------|--------------|----------|------------|------------|
| `cursor_motion` | 144.5 | 65.0 | 2.22x | 64 | 133 |
| `dense_cells` | 325.0 | 282.0 | 1.15x | 30 | 34 |
| `light_cells` | 96.5 | 49.0 | 1.97x | 90 | 179 |
| `medium_cells` | 209.0 | 105.0 | 1.99x | 47 | 85 |
| `scrolling` | 1369.5 | 526.0 | 2.60x | 8 | 19 |
| `scrolling_bottom_region` | 1300.0 | 505.0 | 2.57x | 8 | 20 |
| `scrolling_bottom_small_region` | 1307.5 | 506.0 | 2.58x | 8 | 21 |
| `scrolling_fullscreen` | 152.0 | 94.0 | 1.62x | 59 | 99 |
| `scrolling_top_region` | 1107.5 | 422.0 | 2.62x | 10 | 25 |
| `scrolling_top_small_region` | 1193.0 | 386.5 | 3.09x | 9 | 26 |
| `sync_medium_cells` | 224.0 | 112.5 | 1.99x | 44 | 82 |
| `unicode` | 92.0 | 82.5 | 1.12x | 83 | 82 |

整体看：

- 大多数普通文本和中等复杂度场景接近 `2x`
- 滚动区域类场景达到 `2.57x` 到 `3.09x`
- `dense_cells` 和 `unicode` 提升较小，符合预期，因为它们更容易触发复杂文本、样式切换或 Unicode 路径，不能大规模合并

### 4.2 perf 对比

优化后 `perf.data` 的命令为：

```text
perf record -g ./deepin-terminal-ghostty \
  --working-directory /home/hualet/projects/v/vtebench/ \
  --execute cargo run --release -- --dat results1.dat
```

关键热点变化如下：

| 热点 | 优化前 | 优化后 |
|------|--------|--------|
| `libharfbuzz.so` | 9.18% | 1.59% |
| `QTextEngine::shapeTextWithHarfbuzzNG()` | 3.38% | 0.43% |
| `QTextEngine::itemize()` | 1.13% | 0.22% |
| `QTextEngine::shapeText()` | 0.87% | 0.17% |
| `QFontEngine::getGlyphPositions(...)` | 0.51% | 0.14% |

这说明优化直接命中了目标瓶颈：逐 cell `drawText()` 造成的 Qt/Harfbuzz shaping 被显著压低。

### 4.3 新的热点分布

优化后绘制侧成本下降，热点相对转移到：

- `ghostty_terminal_vt_write()`
- `terminal.Terminal.print`
- `terminal.stream.Stream(...).nextNonUtf8`
- `terminal.Screen.cursorScrollAbove`
- `ghostty_render_state_row_cells_get(_multi)`

这并不表示这些路径变慢，而是文本 shaping 被削掉后，VT 解析、滚动和 render state 访问成为更明显的剩余成本。

---

## 5. 验证

本轮执行过的验证命令：

```bash
cmake --build build --target test_terminal_widget
./build/tests/test_terminal_widget -platform offscreen \
  testCoalescesPlainTextIntoRenderRuns \
  testRendersWideCharactersAcrossTwoCells \
  testRendersAnsiForegroundColors \
  testRendersSupplementaryPlaneCharacters \
  testRendersLongGraphemeCells \
  testIncrementalUpdatesRenderDirtyRowsOnly
```

结果：

- 渲染相关定向测试通过
- 新增 run 合并测试通过
- 宽字符、ANSI 颜色、长 grapheme 和 dirty row 未回退

随后执行：

```bash
./build/tests/test_terminal_widget -platform offscreen
clang-format --dry-run --Werror src/libqtghostty/TerminalWidget.cpp src/libqtghostty/TerminalWidget.h tests/test_terminal_widget.cpp
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

结果：

- `test_terminal_widget`: `45 passed`
- `clang-format`: 通过
- `cmake --build build`: 通过
- `ctest`: `6/7` 通过
- 已知失败: `AppSettings::testVerticalTabsEnabled`
  - 单独复跑仍失败
  - 失败点为设置值读回为 `false`
  - 与本轮 `TerminalWidget` 渲染改动无关

---

## 6. 阶段结论

本轮优化是有效的。

最直接的证据是：

- `vtebench` 多数项目中位耗时约减半
- 滚动区域类 benchmark 提升约 `2.6x` 到 `3.1x`
- `libharfbuzz` 和 `QTextEngine` shaping 热点大幅下降

这说明 `TerminalWidget::renderRow()` 之前的 per-cell 绘制模型确实是 `vtebench` 场景下的重要瓶颈。

当前继续优化的优先方向已经改变：

1. 继续减少 render state 遍历和 cell 查询成本
2. 研究滚动场景下是否能利用 back buffer 做行级 scroll copy
3. 继续观察 `ghostty_terminal_vt_write()`、`Terminal.print` 和 `cursorScrollAbove`
4. 对 `dense_cells` 与 `unicode` 这类低收益场景单独 profiling，判断是否值得做复杂文本缓存

---

## 7. 对应改动

本轮改动文件：

- `src/libqtghostty/TerminalWidget.cpp`
- `src/libqtghostty/TerminalWidget.h`
- `tests/test_terminal_widget.cpp`

主要行为变化：

- 普通窄文本按 run 合并绘制
- 单 codepoint cell 走快路径
- 空白 cell 跳过文本绘制
- 增加测试计数器和 run 合并回归测试

