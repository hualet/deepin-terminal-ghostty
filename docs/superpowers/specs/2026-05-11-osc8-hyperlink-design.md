# OSC 8 Hyperlink 支持设计文档

## 背景

Issue #23 要求终端能够识别输出内容中的链接。经过讨论，确定仅支持 **OSC 8 显式超链接协议**（终端应用通过 `ESC]8;;URI\aTEXT\ESC]8;;\a` 标记的链接），不识别纯文本中的 URL。该方案性能最好，且直接复用 Ghostty 已有的 hyperlink 基础设施。

## 目标

1. 鼠标悬停在 OSC 8 hyperlink 上时，在链接文字下方显示下划线
2. 右键菜单增加"复制链接"、"打开链接"两个选项
3. Ctrl + 左键直接打开链接
4. 性能要求：利用 Ghostty 原生 API，O(1) 查询单 cell 的 hyperlink，避免全屏文本扫描

## 方案对比

| 方案 | 描述 | 优点 | 缺点 |
|------|------|------|------|
| A（推荐） | 复用 Ghostty 原生 hyperlink API | 性能最好，无需额外解析 | 仅支持 OSC 8 |
| B | 纯文本正则匹配 | 可识别所有 URL | 性能开销大，实现复杂 |
| C | 混合方案 | 兼顾性能和覆盖度 | 实现复杂度高 |

采用方案 A。

## 架构设计

改动集中在两个层次：
- `libqtghostty`（`TerminalWidget`）：负责 hyperlink 检测、下划线渲染、信号发射
- `app`（`TermPane` / `MainWindow`）：负责右键菜单集成、快捷键处理、URL 打开

### 数据模型（TerminalWidget）

新增私有成员：

```cpp
QString m_hoverHyperlinkUri;   // 当前鼠标悬停的 hyperlink URI（空表示无）
QPoint m_lastMousePos;         // 最后已知鼠标位置（用于 viewport 滚动后重新检测）
```

新增信号：

```cpp
void hyperlinkHovered(const QString &uri);     // URI 变化时发射（空字符串表示移出）
void hyperlinkActivated(const QString &uri);   // Ctrl+左键点击时发射
```

### 鼠标交互

#### mouseMoveEvent

1. 通过 `ghostty_grid_ref_hyperlink_uri` 查询当前 cell 的 hyperlink URI
2. 与 `m_hoverHyperlinkUri` 对比，变化时更新并重绘
3. 非空时设置光标 `Qt::PointingHandCursor`，否则恢复默认
4. 发射 `hyperlinkHovered(uri)`
5. 记录 `m_lastMousePos = event->pos()`

#### leaveEvent（新增 override）

`TerminalWidget` 需重写 `QWidget::leaveEvent`：

1. 清空 `m_hoverHyperlinkUri`
2. 发射 `hyperlinkHovered("")`
3. 恢复默认光标
4. 触发重绘

#### mousePressEvent（Ctrl+左键）

**不依赖 `m_hoverHyperlinkUri` 缓存，而是直接按点击位置查询 URI。**

1. 若 `event->button() == Qt::LeftButton && event->modifiers() & Qt::ControlModifier`：
   - 按 `event->pos()` 查询该位置的 hyperlink URI
   - 若 URI 非空：
     - 发射 `hyperlinkActivated(uri)`
     - `event->accept()`，不传递给 PTY
   - 若 URI 为空：走原有选择逻辑
2. 普通左键走原有选择逻辑

**mouse tracking 开启时的优先级**：Ctrl+左键永远优先检查 hyperlink，不发给 PTY。普通左键在 mouse tracking 开启时仍按现有逻辑发给 PTY。

#### 滚动补偿

`scrollViewportBy` / `scrollViewportToOffset` 后：
- 若鼠标仍在 widget 内（`rect().contains(m_lastMousePos)`）
- 用 `m_lastMousePos` 重新检测 hyperlink 状态

### 渲染（下划线）

**在 `renderRow()` 阶段直接绘制，避免 overlay 阶段的全屏 grid lookup。**

在 `renderRow()` 的 Text pass 中，当绘制每个 cell 的文本后，检查该 cell 的 hyperlink 状态：

```cpp
// 在 renderRow 的 cell 循环中（drawText pass）
if (!m_hoverHyperlinkUri.isEmpty()) {
    bool cellHasHyperlink = false;
    ghostty_cell_get(rawCell, GHOSTTY_CELL_DATA_HAS_HYPERLINK, &cellHasHyperlink);
    if (cellHasHyperlink) {
        // 获取该 cell 的 hyperlink URI（复用已有的 graphemes 查询上下文）
        QString cellUri = hyperlinkUriForCurrentCell();
        if (cellUri == m_hoverHyperlinkUri) {
            // 在该 cell 底部绘制下划线
            const int underlineY = qMin(y + m_cellHeight - 1, y + m_fontAscent + 2);
            painter.drawLine(x, underlineY, x + cellRenderWidth, underlineY);
        }
    }
}
```

**性能优化**：
- 复用 `renderRow()` 已有的 cell 遍历循环，不增加额外的全屏扫描
- 利用 `GHOSTTY_CELL_DATA_HAS_HYPERLINK` 快速判断单个 cell（O(1)）
- 仅在 `m_hoverHyperlinkUri` 非空时才执行检查
- 不需要在 overlay 阶段做 screen grid lookup

**跨行链接处理**：所有匹配 `m_hoverHyperlinkUri` 的 cell 都绘制下划线，不论是否跨行。只绘制可见 viewport 内的 cell。

### 右键菜单与快捷键（app 层）

#### 上下文菜单（TermPane::showTerminalContextMenu）

**不依赖 hover 缓存，右键事件发生时直接按事件位置查询 URI。**

1. 在 `TermPane::showTerminalContextMenu` 中，通过 `term->hyperlinkUriAt(globalPos)`（或类似接口）获取右键位置的 hyperlink URI
2. 若 URI 非空，临时创建两个 `QAction`：
   - "复制链接" → `QGuiApplication::clipboard()->setText(uri)`
   - "打开链接" → `QDesktopServices::openUrl(QUrl::fromUserInput(uri))`
3. 将这两个 action 插入到菜单中（例如在 Paste 之后）
4. URI 为空时不添加这两个 action

#### Ctrl+左键

连接 `hyperlinkActivated` 信号：
- 直接调用 `QDesktopServices::openUrl(QUrl::fromUserInput(uri))`

### 辅助方法（TerminalWidget）

```cpp
// 查询指定 screen 坐标的 hyperlink URI
QString hyperlinkUriAt(int screenRow, int col) const;

// 根据鼠标位置（widget 坐标）查询 hyperlink URI
// 用于右键菜单和 Ctrl+点击，不依赖 hover 缓存
QString hyperlinkUriAtPosition(const QPoint &pos) const;

// 根据鼠标位置更新 hyperlink 悬停状态
void updateHyperlinkHoverState(const QPoint &pos);
```

### 边界处理

| 场景 | 处理 |
|------|------|
| 链接跨多 cell / 跨行 | 所有匹配 URI 的可见 cell 均绘制下划线 |
| 鼠标移出 widget | `leaveEvent` 清空状态 |
| viewport 滚动 | 滚动后自动重新检测 `m_lastMousePos` 位置的 hyperlink |
| 快速移动 | 依赖 Ghostty O(1) 的 hyperlink 查询，每帧仅检测一个 cell |
| URI 超长 | `ghostty_grid_ref_hyperlink_uri` 支持动态 buffer 扩容 |
| 无效 URI | 使用 `QUrl::fromUserInput` 处理，无效时静默忽略 |

### Ghostty API 使用

关键 API：
- `GHOSTTY_CELL_DATA_HAS_HYPERLINK` — 判断 cell 是否有 hyperlink
- `GHOSTTY_ROW_DATA_HYPERLINK` — 判断整行是否有 hyperlink（快速跳过）
- `ghostty_grid_ref_hyperlink_uri` — 获取 cell 的 hyperlink URI

URI 获取流程：
1. 调用 `ghostty_grid_ref_hyperlink_uri(&ref, nullptr, 0, &requiredLen)` 获取所需 buffer 大小
2. 分配足够 buffer
3. 再次调用获取 URI 内容
4. 转为 `QString`

## 测试策略

### 单元测试（test_terminal_widget）

1. **hyperlink 检测测试**：
   - 模拟 OSC 8 序列写入终端
   - 验证 `hyperlinkHovered` 信号在鼠标移入/移出 hyperlink cell 时正确发射
   - 验证 `hyperlinkActivated` 信号在 Ctrl+左键时正确发射

2. **光标状态测试**：
   - 验证悬停 hyperlink 时光标变为 `PointingHandCursor`
   - 验证移出时恢复默认光标

### 集成测试（test_main_window）

1. **右键菜单测试**：
   - 模拟右键事件发生在 hyperlink cell 上
   - 验证菜单中包含"复制链接"和"打开链接" action
   - 验证 action 绑定的 URI 正确

2. **Ctrl+左键测试**：
   - 模拟 Ctrl+左键点击 hyperlink cell
   - 验证 `hyperlinkActivated` 信号发射正确 URI

**注意**：不直接 mock `QDesktopServices::openUrl`（静态 API 难以 mock）。app 层的 URI 打开逻辑通过信号验证；如需更深层测试，可在 app 层封装 `openUrl()` 虚函数或使用回调注入。

### 性能测试

- 确保渲染帧率不因 hyperlink 下划线绘制显著下降
- 验证快速鼠标移动时 UI 保持响应

## 影响范围

- `src/libqtghostty/TerminalWidget.h` — 新增信号和成员
- `src/libqtghostty/TerminalWidget.cpp` — 鼠标事件处理、渲染逻辑
- `src/app/TermPane.h/.cpp` 或 `MainWindow.h/.cpp` — 右键菜单集成
- `tests/test_terminal_widget.cpp` — 新增测试
- `tests/test_main_window.cpp` — 新增测试

## 不引入的新依赖

仅使用 Qt 内置类（`QUrl`, `QDesktopServices`, `QClipboard`）和 Ghostty 原生 API，不引入额外库。

## 风险与缓解

| 风险 | 缓解措施 |
|------|----------|
| Ghostty hyperlink API 行为与文档不符 | 已在代码中确认相关 API 存在，编写时通过测试验证 |
| URI 格式不合法导致 `openUrl` 失败 | 使用 `QUrl::fromUserInput` 处理，无效时静默忽略 |
| 频繁 mouseMove 导致性能问题 | 仅查询单 cell，利用 row 级快速跳过，不扫描全屏 |
| mouse tracking 开启时 Ctrl+点击与终端应用冲突 | Ctrl+左键永远优先处理 hyperlink，普通左键仍按现有逻辑发给 PTY |
| 链接跨多行时 selection 逻辑冲突 | Ctrl+左键优先处理 hyperlink，不触发 selection |
