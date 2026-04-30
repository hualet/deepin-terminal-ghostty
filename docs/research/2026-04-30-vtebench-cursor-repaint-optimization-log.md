# vtebench 光标与 repaint 路径优化报告

> **日期**: 2026-04-30  
> **范围**: `deepin-terminal-ghostty` 在 vtebench 后续优化中的光标更新和 repaint 路径  
> **目标**: 降低纯光标更新的无效整窗 repaint，验证 partial dirty rows 收窄 repaint 的可行性  
> **相关数据**: `build/perf-results7.data`, `build/perf-results8.data`, `/home/hualet/projects/v/vtebench/results6.dat`, `/home/hualet/projects/v/vtebench/results7.dat`

---

## 1. 背景

上一轮渲染路径微优化后，`TerminalWidget::renderRow()` 在 perf 中已经不是主要热点。
新的热点主要集中在：

- QtGui 栅格化和文本绘制线程
- `ghostty_terminal_vt_write()`
- Ghostty VT 解析和滚动路径
- 光标、局部更新和 repaint 调度

因此本轮没有继续在每个 cell 上追加查询，而是优先尝试降低 repaint 范围。
重点验证两类场景：

1. 纯光标移动或回车/退格这类不会改变文本内容的 PTY 数据。
2. Ghostty render state 只标记部分 dirty rows 时，是否可以只请求 dirty 行 repaint。

---

## 2. 已保留的修改

### 2.1 cursor-only PTY 数据识别

新增 `isCursorOnlyPtyData()`，只接受保守的光标相关输入：

- `\r`
- `\b`
- CSI `A/B/C/D/G/H/f`

只要数据中包含普通可打印字符、OSC、清屏、属性变化或其他 CSI 序列，就不进入该快路径。
这样避免把会改变屏幕内容的输入误判为纯光标更新。

### 2.2 纯光标更新只 repaint 光标矩形

`flushPendingPtyData()` 现在可以接收一个 `QRect *repaintRegion`：

- flush 前记录旧光标矩形
- 写入 Ghostty VT
- 若输入是 cursor-only，则立即更新 render state
- 若 Ghostty 只返回 clean 或 partial dirty，则清理 dirty 标记
- 只请求旧/新光标矩形的 union repaint

这条路径会跳过整窗 `update()`，避免纯光标移动触发完整 backbuffer 绘制。

### 2.3 光标闪烁只 repaint 光标矩形

光标 blink timer 原来每 500ms 调用一次 `update()`，会请求整个 widget repaint。

本轮改为：

- blink 前记录当前光标矩形
- 切换 `m_cursorBlinkVisible`
- blink 后记录新光标矩形
- 只 repaint 两个矩形的 union

这对 vtebench 主测试影响有限，但对真实交互中的空闲光标闪烁更合理。

### 2.4 测试覆盖

新增 `testCursorOnlyUpdatesUseNarrowRepaint()`：

- 使用 `sleep 5` 作为测试子进程，避免 shell prompt 自身输出干扰
- 先写入普通文本并完成一次 render
- 再注入 `\r`
- 验证 cursor-only repaint 计数增加

该测试覆盖了 cursor-only PTY 更新进入窄 repaint 路径的行为。

---

## 3. 失败尝试：partial dirty rows 收窄 update 区域

### 3.1 尝试内容

曾尝试把所有 partial dirty render state 都转成更小的 Qt update 区域：

- flush 后立即 `ghostty_render_state_update()`
- 如果 dirty state 是 `GHOSTTY_RENDER_STATE_DIRTY_PARTIAL`
- 遍历 dirty rows，构造这些行对应的 widget QRect
- 将 dirty 行和旧/新光标矩形合并后传给 `update(region)`

预期收益是：

- `cursor_motion` 每次只改少量行时减少 Qt repaint 面积
- `medium_cells`、`light_cells` 和局部滚动减少整窗 paint 成本

### 3.2 实测结果

这版代码对应：

- `build/perf-results8.data`
- `/home/hualet/projects/v/vtebench/results7.dat`

与保留 cursor-only 优化后的 `results6.dat` 对比，多数项目变慢：

| Benchmark | `results6.dat` | partial dirty 尝试 | 比例 |
|-----------|----------------|--------------------|------|
| `cursor_motion` | 80.3ms | 96.3ms | 1.20x |
| `dense_cells` | 348.8ms | 422.6ms | 1.21x |
| `light_cells` | 66.2ms | 84.4ms | 1.27x |
| `medium_cells` | 117.4ms | 128.8ms | 1.10x |
| `scrolling` | 634.4ms | 660.0ms | 1.04x |
| `scrolling_fullscreen` | 82.4ms | 108.5ms | 1.32x |
| `sync_medium_cells` | 127.8ms | 140.3ms | 1.10x |
| `unicode` | 151.2ms | 155.9ms | 1.03x |

只有 `scrolling_bottom_region`、`scrolling_top_region` 和
`scrolling_top_small_region` 基本持平，没有形成可靠收益。

### 3.3 结论

这条路是负收益，已经回退。

可能原因：

- flush 阶段提前更新 render state 增加了同步工作。
- 每次 partial dirty 都遍历 row iterator，抵消了 Qt update 区域变小的收益。
- Qt 对多个细长 repaint 区域的调度和合并成本不低。
- vtebench 的高吞吐场景更需要减少 repaint 次数，而不是把每次 repaint 切得更碎。

后续如果继续做局部 repaint，应该先做更聚焦的指标：

- 统计每次 flush 的 dirty row 数量分布。
- 统计 Qt 实际 paint event 的 region 数量和面积。
- 只对 dirty row 很少的场景启用，而不是所有 partial dirty 都启用。

---

## 4. 当前版本数据

本轮最终保留 cursor-only repaint 优化，数据对应：

- `build/perf-results7.data`
- `/home/hualet/projects/v/vtebench/results6.dat`

`results5.dat` 是上一轮基线，`results6.dat` 是本轮保留改动后的结果。
原版 deepin-terminal 数据来自用户提供的对比样本。单位为毫秒，越低越好。

| Benchmark | `results5.dat` | `results6.dat` | 比例 | 原版 deepin-terminal | `results6/原版` |
|-----------|----------------|----------------|------|----------------------|-----------------|
| `cursor_motion` | 159.6 | 80.3 | 0.50x | 20.0 | 4.02x |
| `dense_cells` | 643.2 | 348.8 | 0.54x | 165.8 | 2.10x |
| `light_cells` | 81.3 | 66.2 | 0.81x | 29.2 | 2.27x |
| `medium_cells` | 180.7 | 117.4 | 0.65x | 46.0 | 2.55x |
| `scrolling` | 752.8 | 634.4 | 0.84x | 562.7 | 1.13x |
| `scrolling_bottom_region` | 726.6 | 667.9 | 0.92x | 272.2 | 2.45x |
| `scrolling_bottom_small_region` | 715.9 | 588.9 | 0.82x | 225.2 | 2.62x |
| `scrolling_fullscreen` | 160.3 | 82.4 | 0.51x | 50.3 | 1.64x |
| `scrolling_top_region` | 592.4 | 450.7 | 0.76x | 223.6 | 2.02x |
| `scrolling_top_small_region` | 635.9 | 457.3 | 0.72x | 218.3 | 2.09x |
| `sync_medium_cells` | 187.6 | 127.8 | 0.68x | 53.3 | 2.40x |
| `unicode` | 183.0 | 151.2 | 0.83x | 45.6 | 3.32x |

本轮相对 `results5.dat` 全项改善，其中：

- `cursor_motion` 约减半
- `dense_cells` 约减半
- `scrolling_fullscreen` 约减半
- `medium_cells`、`sync_medium_cells` 有明显改善

但距离原版 deepin-terminal 仍有差距，尤其是：

- `cursor_motion`
- `unicode`
- 区域滚动
- `medium_cells` / `sync_medium_cells`

---

## 5. perf 观察

`perf-results7.data` 中，主要热点仍然不是 `TerminalWidget::renderRow()`：

| 模块 | 占比 |
|------|------|
| `Thread (pooled) / libc.so.6` | 43.91% |
| `deepin-terminal / libQt6Gui.so` | 36.74% |
| `deepin-terminal / libghostty-vt.so` | 8.90% |
| `deepin-terminal / libc.so.6` | 3.22% |
| `deepin-terminal / libqtghostty.so` | 0.82% |

`TerminalWidget::renderRow()` 约 `0.38%`，说明文本 run 合并后的 C++ row render
本身已经不是最大瓶颈。

同时，`cursor_motion` benchmark 脚本并不是纯光标移动。它的核心输出是：

```sh
printf "\e[${line};${column}H$char"
```

也就是每次移动光标后立即写入一个字符。因此 cursor-only fast path 不能直接覆盖
`cursor_motion` 主循环，只能覆盖实际 shell 或应用中的纯光标移动、回车、退格类更新。
`cursor_motion` 本轮仍改善，更多来自 repaint/flush 时序和测量波动，不能把全部收益归因于
cursor-only fast path。

---

## 6. 验证

本轮执行过：

```bash
clang-format --dry-run --Werror src/libqtghostty/TerminalWidget.cpp src/libqtghostty/TerminalWidget.h tests/test_terminal_widget.cpp
cmake --build build
./build/tests/test_terminal_widget -platform offscreen
```

结果：

- `clang-format`: 通过
- `cmake --build build`: 通过
- `test_terminal_widget`: `46 passed`

---

## 7. 阶段结论

本轮最终保留：

- cursor-only PTY 数据窄 repaint
- 光标 blink 窄 repaint
- 对应测试覆盖

本轮明确回退：

- partial dirty rows 通用窄 update

下一步更有价值的方向：

1. 单独统计 repaint 次数、paint event region 面积和 dirty row 数量，避免只靠 vtebench 总耗时推断。
2. 对 `cursor_motion` 做针对性分析，因为它是“移动光标 + 写字符”，不是纯光标场景。
3. 研究是否能在 Ghostty render API 层拿到 run/segment 级数据，进一步减少 cell 查询。
4. 针对 `unicode` 的 Qt 字体 fallback 和 Harfbuzz shaping 建立单独样本，避免被普通文本路径掩盖。
5. 对区域滚动评估 scroll-copy/backbuffer copy，而不是每次都按 dirty row 重绘。

---

## 8. 对应改动

本轮最终保留改动文件：

- `src/libqtghostty/TerminalWidget.cpp`
- `src/libqtghostty/TerminalWidget.h`
- `tests/test_terminal_widget.cpp`
