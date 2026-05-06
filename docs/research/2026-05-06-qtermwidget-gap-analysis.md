# qtermwidget 差距分析与优化记录

> **日期**: 2026-05-06  
> **范围**: `src/libqtghostty/TerminalWidget.*` 与 `/home/hualet/projects/q/qtermwidget/lib/TerminalDisplay.*`、`Screen*`、`Vt102Emulation.*` 的渲染与兼容性对比  
> **目标**: 解释当前渲染性能和终端兼容性差距，列出可优化项，并记录本轮落地改进  
> **Benchmark**: `/home/hualet/projects/v/vtebench`，在构建好的 `deepin-terminal-ghostty` 中执行 `cargo run --release`

---

## 1. 结论摘要

`qtermwidget` 和当前 `deepin-terminal-ghostty` 的核心差异不是单个绘制函数，而是两套不同的终端架构：

- `qtermwidget` 是 Konsole 系终端栈，模拟器、屏幕缓冲、dirty mask、scroll count、字符属性和 Qt 绘制层高度配合。
- 我们当前把终端语义交给 `libghostty-vt`，Qt 侧通过 render state 逐行逐 cell 拉取最终状态，再用 `QPainter` 绘制。
- 因此我们的兼容性上限主要来自 Ghostty VT，性能瓶颈则集中在“跨 C API 逐 cell 查询 + Qt 文本绘制 + 缺少 scroll-copy 信息”。

已有优化已经解决了最重的性能问题：替换 ReleaseFast `libghostty-vt`、合并文本 run、减少 cursor-only repaint。剩余差距主要在：

1. qtermwidget 有 scroll-copy 路径，滚动时移动现有 widget 内容，只重画新暴露行；我们目前仍依赖 Ghostty dirty rows 和 backbuffer 重绘。
2. qtermwidget 本地持有完整 `Character` 图像，dirty 判断是内存比较；我们需要通过 render state iterator 和多次 `ghostty_*_get` 查询。
3. qtermwidget 已支持更多 SGR 视觉属性和 VT 行属性；我们之前只渲染 bold、italic、inverse、颜色和宽字符。
4. qtermwidget 有成熟的键盘 translator、bracketed paste、filter/hotspot、line drawing、double-width/double-height line 等多年积累；我们还有较多终端应用兼容性缺口。

本轮已落地一项低风险兼容性优化：补齐 SGR underline、strikethrough、overline、faint、conceal 的基础渲染，并保持普通文本 run 合并路径。

---

## 2. qtermwidget 的关键实现

### 2.1 屏幕图像与脏区

qtermwidget 的 `Screen`/`ScreenWindow` 维护终端图像和滚动信息：

- `Screen::scrolledLines()` / `ScreenWindow::scrollCount()` 记录一轮输出造成的净滚动行数。
- `ScreenWindow::scrollRegion()` 知道滚动发生在哪个屏幕区域。
- `TerminalDisplay::updateImage()` 先调用 `scrollImage()`，然后比较 `_image` 与 `newimg`，生成 dirty region。

这意味着 qtermwidget 可以在 Qt widget 层做非常便宜的滚动：

```text
scrollImage(scrollCount, scrollRegion)
memmove(_image)
QWidget::scroll(...)
只 update 新暴露/变化的行
```

我们的 `TerminalWidget` 当前没有拿到 Ghostty VT 的“滚动了几行、哪个区域滚动”的高层信号，只能：

- `ghostty_render_state_update()`
- 遍历 row iterator
- 对 dirty row 重新 `renderRow()`
- 最后把 backbuffer 画回 widget

这解释了区域滚动类 benchmark 仍然和 qtermwidget/deepin-terminal 有差距。

### 2.2 文本片段合并

qtermwidget 的 `drawContents()` 会按以下条件合并 text fragment：

- 相同 foreground/background/rendition
- 相同 line drawing 状态
- 相同双宽字符状态
- 字符没有超过单元格宽度

我们此前已经做了对应优化：`TerminalWidget::renderRow()` 合并连续普通窄字符 run，减少 per-cell `drawText()`、QString 构造和 Harfbuzz shaping。这个方向有效，`vtebench` 旧数据中普通文本和滚动类项目曾达到约 2x 到 3x 提升。

### 2.3 SGR 与行属性

qtermwidget 的 `Character` 支持：

- `RE_BOLD`
- `RE_BLINK`
- `RE_UNDERLINE`
- `RE_ITALIC`
- `RE_FAINT`
- `RE_STRIKEOUT`
- `RE_CONCEAL`
- `RE_OVERLINE`
- `LINE_DOUBLEWIDTH`
- `LINE_DOUBLEHEIGHT`

我们之前虽然从 Ghostty 取得了完整 `GhosttyStyle`，但只使用了 `bold`、`italic`、`inverse` 和颜色。本轮已补齐：

- underline，包括 single/double/dotted/dashed 的基础线型
- strikethrough
- overline
- faint 前景色降强
- invisible/conceal 跳过 glyph 绘制

仍未补齐：

- blink text 定时隐藏
- curly underline 的波浪线绘制
- palette underline color 的完整解析
- VT double-width/double-height line 渲染
- DEC special graphics 的专用线段绘制
- kitty graphics placement 绘制
- hyperlink/filter hover 视觉

---

## 3. 为什么渲染性能仍有差距

### 3.1 C API 逐 cell 查询成本

qtermwidget 的显示层直接读取本地 `Character* _image`，属性在一个结构体里。我们每个 cell 至少需要：

- raw cell
- style
- grapheme length
- foreground color
- background color
- 宽字符状态
- 必要时 grapheme buffer

之前尝试过在 Qt 侧追加 raw-cell flags 快路径，但实测负收益，说明继续增加 per-cell 查询不是正确方向。更好的方向是让 Ghostty render API 暴露 row/run 级别数据。

### 3.2 缺少 scroll-copy 输入

qtermwidget 滚动时可以 `QWidget::scroll()`，这比重画整片文本便宜。我们虽然有 backbuffer，但不知道“哪些像素可以直接搬移”，只能重画 Ghostty 标记 dirty 的行。

可优化点：

1. 在 Ghostty 侧增加 scroll delta / scroll region render event。
2. Qt 侧收到滚动事件后先移动 backbuffer 的对应区域。
3. 只把新暴露行标记为 dirty 并渲染。

这是区域滚动类 benchmark 的最高优先级方向。

### 3.3 Qt 字体 fallback 与复杂文本

`unicode` 场景仍慢，主要来自 Qt 字体 fallback、glyph loading、Harfbuzz shaping。qtermwidget 在普通字符路径里使用 `std::wstring` fragment，且对 line drawing 字符有专门绘制路径；我们目前复杂 grapheme、宽字符和 fallback 字体仍会触发较重的 Qt 文本路径。

可优化点：

- 对 Unicode benchmark 单独采样，区分 CJK、emoji、combining mark、fallback font 的成本。
- 为 DEC line drawing 使用 QPainter 线段直接绘制。
- 研究 glyph/run cache，但需要谨慎处理字体、颜色、DPR、样式和复杂文本。

---

## 4. 本轮改动

### 4.1 兼容性补齐

修改文件：

- `src/libqtghostty/TerminalWidget.cpp`
- `tests/test_terminal_widget.cpp`

行为变化：

- `renderRow()` 的 text run key 增加 decoration 属性，避免把不同 SGR 装饰的文本错误合并。
- 绘制 underline、double underline、dotted underline、dashed underline、strikethrough、overline。
- `style.faint` 使用前景色与背景色混合，形成降强效果。
- `style.invisible` / SGR 8 不绘制 glyph，但仍保留终端状态推进。
- 装饰线绘制会恢复 `QPainter` 原 pen，避免污染后续文本颜色。

新增测试：

- `testRendersTextDecorations()`
- `testConcealedTextDoesNotRenderGlyphs()`

红绿验证：

```bash
./build/tests/test_terminal_widget -platform offscreen \
  testRendersTextDecorations testConcealedTextDoesNotRenderGlyphs
```

改动前两个测试失败；实现后两个测试通过。

---

## 5. vtebench 记录

本轮在构建好的终端中执行：

```bash
./build/deepin-terminal-ghostty \
  --propagate-exit-code \
  --working-directory /home/hualet/projects/v/vtebench \
  --execute '/home/hualet/.cargo/bin/cargo run --release -- --dat results-codex-2026-05-06.dat'
```

生成：

```text
/home/hualet/projects/v/vtebench/results-codex-2026-05-06.dat
```

与上一份已记录的 `results6.dat` 对比，单位为 ms，越低越好：

| Benchmark | `results6.dat` median | 本轮 median | 说明 |
|-----------|----------------------:|------------:|------|
| `cursor_motion` | 75.0 | 139.0 | 本轮环境/绘制负载下更慢 |
| `dense_cells` | 337.5 | 690.5 | 本轮环境/绘制负载下更慢 |
| `light_cells` | 58.0 | 99.0 | 本轮环境/绘制负载下更慢 |
| `medium_cells` | 117.0 | 144.0 | 更接近，仍慢 |
| `scrolling` | 634.0 | 626.0 | 基本持平 |
| `scrolling_bottom_region` | 670.0 | 602.0 | 略好 |
| `scrolling_bottom_small_region` | 592.0 | 584.0 | 基本持平 |
| `scrolling_fullscreen` | 81.0 | 157.0 | 本轮环境/绘制负载下更慢 |
| `scrolling_top_region` | 458.0 | 505.0 | 略慢 |
| `scrolling_top_small_region` | 467.5 | 551.0 | 略慢 |
| `sync_medium_cells` | 125.0 | 170.0 | 更慢，样本含 underline/color 序列 |
| `unicode` | 129.0 | 199.0 | 更慢，仍是重点瓶颈 |

解释：

- 本轮改动是兼容性补齐，不是针对 vtebench 吞吐的优化。
- `sync_medium_cells` 样本中包含 `4:3` underline 和 underline color 序列；以前这些样式被忽略，本轮会真实绘制装饰线，因此该类样本会增加绘制工作。
- 这份数据不能证明端到端吞吐提升；它证明本轮改动后 benchmark 可运行，并暴露出复杂样式和 Unicode 仍是性能重点。

---

## 6. 优化优先级

### P0: scroll-copy / 滚动事件

目标：拉近区域滚动 benchmark 与 qtermwidget 的最大差距。

建议：

- 优先调研 Ghostty 是否可暴露滚动 delta 和 scroll region。
- 若可行，`TerminalWidget` 在 flush 后先移动 backbuffer，再只渲染新暴露行。
- 不建议继续做通用 partial dirty update；此前实测负收益。

### P1: row/run 级 render API

目标：减少 C API per-cell 查询。

建议：

- 在 Ghostty render API 层提供按 run 输出的 text/style/color 数据。
- Qt 侧直接消费 run，避免每 cell 组装 run。
- 这比在 Qt 侧继续追加 `ghostty_cell_get_multi()` 快路径更有价值。

### P2: Unicode 与字体 fallback 专项

目标：改善 `unicode` 和复杂 TUI 内容。

建议：

- 对 `unicode` 单项录制 perf。
- 分离 CJK、emoji、combining mark、fallback 字体加载成本。
- 对 DEC line drawing 做专用 QPainter 线段路径。

### P3: 剩余 SGR/VT 兼容性

目标：减少终端应用渲染差异。

建议：

- blink text：新增文本 blink timer 和 dirty 区域管理。
- curly underline：用 QPainterPath 绘制波浪线。
- underline palette color：完整解析 `GhosttyStyleColor` 的 palette 分支。
- double-width/double-height line：确认 Ghostty render state 是否暴露行属性。
- kitty graphics：消费 `GHOSTTY_TERMINAL_DATA_KITTY_GRAPHICS` placement iterator 并绘制图片。

---

## 7. 当前结论

qtermwidget 的优势来自成熟终端栈和显示层之间的紧耦合优化，尤其是 scroll-copy、dirty character image 和完整 SGR/VT 渲染属性。我们的优势是复用 Ghostty VT 的现代协议能力，但 Qt wrapper 层仍缺少高层 render events 和完整样式绘制。

下一步最值得投入的是 scroll-copy 和 Ghostty row/run API，而不是继续在当前 per-cell Qt 代码上堆更多查询。兼容性方面，本轮已补齐基础文本装饰和 conceal；后续应继续补 blink text、curly underline、DEC line drawing 和 kitty graphics。
