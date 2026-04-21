# GPU 渲染可行性调研报告

> **调研日期**: 2026-04-21  
> **调研目标**: 评估将 `deepin-terminal-ghostty` 从 QPainter CPU 渲染迁移到 GPU 渲染的可行性与价值  
> **参考项目**: Ghostty (上游终端引擎)、Ghostling (参考实现)

---

## 1. 背景与动机

当前 `deepin-terminal-ghostty` 使用 Qt 的 `QPainter` 在 `paintEvent` 中逐行逐单元格绘制终端内容。这种 CPU 渲染方式实现简单、跨平台兼容性好，但在以下场景可能存在性能瓶颈：

- 大量文本快速输出（如 `cat` 大文件）
- 高分辨率（4K/HiDPI）下的频繁重绘
- 未来支持 Kitty Graphics Protocol 等富媒体协议

Ghostty 主程序本身采用 GPU 渲染（Metal/OpenGL），本报告旨在评估在 Qt 应用中复刻类似方案的可行性。

---

## 2. Ghostty 渲染架构分析

### 2.1 整体架构

Ghostty 的渲染层位于 `src/renderer/`，核心设计为 **后端无关的 generic 渲染器 + 平台特定的图形 API 后端**：

```
┌─────────────────────────────────────────┐
│  Terminal State (VT 状态机)              │
└──────────────┬──────────────────────────┘
               │ ghostty_render_state_update()
               ▼
┌─────────────────────────────────────────┐
│  RenderState (脏矩形追踪 + 单元格数据)    │
└──────────────┬──────────────────────────┘
               │ CPU 端数据准备
               ▼
┌─────────────────────────────────────────┐
│  Generic Renderer (backend-agnostic)    │
│  - 单元格数据整理                         │
│  - 字体 Atlas 管理                        │
│  - GPU Buffer 上传                        │
└──────────────┬──────────────────────────┘
               │ 平台抽象层
      ┌────────┴────────┐
      ▼                 ▼
┌──────────┐     ┌──────────┐
│  Metal   │     │ OpenGL   │
│(macOS)   │     │(Linux)   │
└──────────┘     └──────────┘
```

### 2.2 GPU 渲染管线细节

从 `src/renderer/generic.zig` 的 `drawFrame()` 分析，Ghostty 的每帧渲染分为 6 个 pass：

| Pass | Shader | 说明 |
|------|--------|------|
| 1 | `bg_color` / `bg_image` | 清空画布或绘制背景图片 |
| 2 | `image` | Kitty 图片（在背景之下） |
| 3 | `cell_bg` | 单元格背景色（opaque） |
| 4 | `image` | Kitty 图片（在文字之下） |
| 5 | `cell_text` | **文字主体**，使用 **instanced rendering** |
| 6 | `image` | Kitty 图片（在文字之上） |

**关键技术点**：

- **字体 Atlas**：字形预先渲染到两张 GPU texture（grayscale + color），运行时只采样坐标
- **批量渲染**：一次 `glDrawArraysInstanced` 渲染所有文字单元格，每个实例对应一个单元格
- **脏矩形**：只上传变化的单元格数据到 GPU buffer，避免每帧全量传输

### 2.3 `libghostty-vt` 的定位

`libghostty-vt` **仅提供 VT 状态机 + RenderState API**，不包含任何 GPU 渲染代码：

- 提供 `GhosttyRenderStateRowIterator` + `GhosttyRenderStateRowCells` 遍历接口
- 提供脏状态追踪（`GHOSTTY_RENDER_STATE_DIRTY_PARTIAL` / `FULL`）
- **不提供**：字体管理、GPU buffer、shader、绘制命令

这意味着 GPU 渲染不是"开启一个开关"，而是需要**在消费端完全重写渲染层**。

---

## 3. Qt6 GPU 渲染方案对比

| 方案 | Widgets 兼容性 | 工作量 | 说明 |
|------|---------------|--------|------|
| **`QOpenGLWidget`** | ✅ 完全兼容 | **很大** | 在 QWidget 中嵌入 OpenGL 上下文，完全自定义渲染管线。最灵活，但需要手写所有 GPU 代码 |
| **`QRhiWidget`** (Qt 6.6+) | ✅ 兼容 | **极大** | Qt 的跨平台 GPU 抽象（OpenGL/Vulkan/Metal/D3D）。更现代，但文档极少，学习曲线陡峭 |
| **`QQuickWidget`** | ❌ 需改架构 | 不可行 | 要在 Widgets 应用中嵌入 Qt Quick，本质上需要重写为 QML 架构 |
| **保持 QPainter** | ✅ 无改动 | 无 | Qt6 的 QPainter 在 X11/Wayland 上内部可能已通过 `XRender`/compositor 走 GPU 合成路径 |

**最现实的方案**：`QOpenGLWidget`，因为与现有 Widgets 架构兼容，且 Ghostty 已有 OpenGL 渲染器可供参考。

---

## 4. 工作量估算（QOpenGLWidget 方案）

如果要实现 Ghostty 级别的 GPU 渲染，预估工作量如下：

### 4.1 字体系统重写（2–3 周）
- 使用 FreeType 加载字形
- 构建动态 GPU texture atlas（grayscale + color 双 atlas）
- 处理字形缓存、淘汰策略、彩色表情符号（Emoji）

### 4.2 单元格数据上传（1–2 周）
- 将 `GhosttyRenderStateRowCells` 转换为 GPU vertex/instance buffer 格式
- 实现脏矩形追踪，只上传变化的数据
- 处理光标位置、选中高亮、滚动偏移量

### 4.3 Shader 编写（1–2 周）
- **Vertex Shader**：从单元格数据计算 quad 顶点位置、采样 atlas UV
- **Fragment Shader**：从 atlas 采样字形、应用前景/背景色、处理透明度
- **Background Shader**：绘制背景色或背景图片

### 4.4 Qt 集成（1 周）
- `TerminalWidget` 从 `QWidget` 改为继承 `QOpenGLWidget`
- OpenGL 上下文管理、VSync 控制、HiDPI 适配
- 输入事件（键盘、鼠标滚轮、焦点）正确转发

### 4.5 调试与优化（2–3 周）
- 字体渲染质量调优（subpixel anti-aliasing、gamma 校正）
- 减少 CPU→GPU 数据传输（persistent mapped buffer、double buffering）
- 性能 profiling：确保 `cat 100MB` 不掉帧

**总计：约 2–3 个月全职开发**，前提是开发者熟悉 OpenGL/图形编程。

---

## 5. 价值评估

### 5.1 性能场景对比

| 场景 | GPU 渲染价值 | 当前 QPainter 是否够用 |
|------|-------------|----------------------|
| 日常 shell 交互（命令输入、少量输出） | **低** | ✅ 完全够用，CPU 占用 < 5% |
| `cat` 10MB 日志文件（大量文本高速输出） | **高** | ⚠️ 可能掉帧或 CPU 占用飙升 |
| 快速滚动（鼠标滚轮、触控板惯性滚动） | **高** | ⚠️ 每帧需重绘数十行 |
| Kitty Graphics Protocol 图片显示 | **极高** | ❌ CPU 渲染不可行，每张图片都是全屏位图操作 |
| 4K/HiDPI 高分辨率 | **中** | ⚠️ 绘制调用和像素数成正比倍增 |
| 笔记本低功耗模式 | **中** | ✅ 轻量场景下 CPU 渲染更省电 |

### 5.2 与项目定位的匹配度

当前 `deepin-terminal-ghostty` 的定位是 **"最小 viable 终端模拟器"**，核心目标是验证 `libghostty-vt` 在 Qt 应用中的集成可行性。

- **QPainter 的优势**：实现简单、代码量少（~200 行渲染逻辑）、跨平台稳定、易于维护
- **GPU 渲染的劣势**：引入 2–3 个月开发量、大幅增加代码复杂度、需要专职图形开发人力

**结论**：在项目当前阶段，GPU 渲染的**收益无法覆盖成本**。

---

## 6. 折中路径：验证 Qt 内部 GPU 加速

Qt6 的 `QPainter` 在 Linux 平台上**可能已经走了 GPU 加速**：

- **X11**：Qt 默认使用 `XRender` 扩展，XRender 通常由 GPU 驱动加速
- **Wayland**：compositor 负责合成，Qt 的 buffer 直接提交给 Wayland compositor（可能走 GPU）

可以通过以下命令验证当前是否已使用 GPU：

```bash
# 强制纯软件渲染，对比性能差异
QT_QUICK_BACKEND=software ./build/deepin-terminal-ghostty

# 查看 Qt 使用的平台插件和渲染后端
QT_LOGGING_RULES="qt.qpa.*=true" ./build/deepin-terminal-ghostty

# 使用 apitrace 抓取实际 OpenGL 调用（如果有）
apitrace trace ./build/deepin-terminal-ghostty
```

如果验证发现 QPainter 已经通过平台层间接使用了 GPU，则迁移到显式 GPU 渲染的紧迫性进一步降低。

---

## 7. 结论与建议

### 7.1 核心结论

**不建议在当前阶段迁移到 GPU 渲染。**

理由：
1. **复杂度与收益不匹配**：最小 viable 定位下，QPainter 在日常使用中没有可感知的性能瓶颈
2. **工作量巨大**：2–3 个月全职开发，且需要 OpenGL/图形编程 expertise
3. **维护负担**：字体 atlas、GPU buffer 管理、shader 调试都是长期维护成本
4. **libghostty-vt 不提供帮助**：GPU 渲染需要消费端完全自研，没有现成的 Qt GPU 渲染后端可用

### 7.2 建议的后续行动

| 优先级 | 行动 | 时机 |
|--------|------|------|
| P0 | **验证 QPainter 是否已走 GPU 加速** | 立即（用 `apitrace` 或 `QT_LOGGING_RULES`） |
| P1 | **保持 QPainter，优化增量渲染** | 持续（当前脏矩形追踪已部分实现） |
| P2 | **评估 Kitty Graphics Protocol 需求** | 如果社区明确要求图片渲染 |
| P3 | **设计 GPU 渲染的技术方案** | 当确定 Kitty Graphics Protocol 为必做功能时 |
| P4 | **实施 GPU 渲染** | 有专职图形开发人力且性能瓶颈已确认时 |

### 7.3 未来实施 GPU 渲染的参考路径

如果未来决定实施，建议按以下顺序参考 Ghostty 源码：

1. **数据准备**：参考 `src/renderer/generic.zig` 的 `updateFrame()`，学习如何从 `Terminal` 构建单元格数组
2. **OpenGL 后端**：参考 `src/renderer/opengl/` 目录下的 `Frame.zig`、`Pipeline.zig`、`Buffer.zig`
3. **Shader**：参考 `src/renderer/shaders/` 中的 GLSL shader
4. **字体 Atlas**：参考 `src/font/` 中的 atlas 构建逻辑
5. **Qt 集成**：用 `QOpenGLWidget` 替换 `QWidget`，在 `initializeGL()` / `paintGL()` 中实现上述管线

---

## 8. 参考资源

| 资源 | 路径 | 说明 |
|------|------|------|
| Ghostty 通用渲染器 | `~/projects/g/ghostty/src/renderer/generic.zig` | 后端无关的渲染逻辑，核心参考 |
| Ghostty OpenGL 后端 | `~/projects/g/ghostty/src/renderer/opengl/` | OpenGL buffer/texture/shader 管理 |
| Ghostty Shader | `~/projects/g/ghostty/src/renderer/shaders/` | GLSL vertex/fragment shader |
| Ghostty 字体系统 | `~/projects/g/ghostty/src/font/` | Atlas 构建、字形缓存 |
| libghostty-vt Render API | `ghostty/vt/render.h` | C API 定义 |
| Ghostling 参考实现 | `~/projects/g/ghostling/main.c` | 基于 raylib 的 CPU 渲染参考 |
