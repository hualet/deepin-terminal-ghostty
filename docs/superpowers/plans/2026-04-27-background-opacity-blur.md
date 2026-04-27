# Background Opacity and Blur Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add background transparency (opacity slider) and background blur (checkbox) to deepin-terminal-ghostty, matching the deepin-terminal experience.

**Architecture:** Settings are stored via DSettings with opacity (int 20–100) and blur (bool). Opacity is applied by modifying the alpha channel on default-background `fillRect` calls in `TerminalWidget`'s paint path. Blur is toggled via `DMainWindow::setEnableBlurWindow()`. Compositor capability is checked via `DWindowManagerHelper`. Keyboard shortcuts Ctrl+Shift+Up/Down adjust opacity in 5% steps.

**Tech Stack:** C++20, Qt6 Widgets, DTK6 (DMainWindow, DWindowManagerHelper, DSettings)

---

### Task 1: Add opacity and blur settings to default-config.json

**Files:**
- Modify: `src/app/settings/default-config.json:53-58`

- [ ] **Step 1: Add opacity slider and blur checkbox after the colorScheme option**

Insert two new option objects after the `colorScheme` option (after line 58) and before the closing `]` of the options array (line 59). The new options go between `colorScheme` and the end of the array:

```json
                        {
                            "key": "colorScheme",
                            "name": "Color Scheme",
                            "type": "combobox",
                            "default": "system"
                        },
                        {
                            "key": "opacity",
                            "name": "Opacity",
                            "type": "slider",
                            "default": 100,
                            "min": 20,
                            "max": 100
                        },
                        {
                            "key": "blurred_background",
                            "text": "Blur background",
                            "type": "checkbox",
                            "default": false
                        }
```

- [ ] **Step 2: Verify JSON is valid**

Run: `python3 -c "import json; json.load(open('src/app/settings/default-config.json')); print('valid')"`
Expected: `valid`

- [ ] **Step 3: Commit**

```bash
git add src/app/settings/default-config.json
git commit -m "feat: add opacity and blur background settings to default-config.json"
```

---

### Task 2: Add opacity and blur API to AppSettings

**Files:**
- Modify: `src/app/AppSettings.h:25-57`
- Modify: `src/app/AppSettings.cpp:113-127`

- [ ] **Step 1: Add declarations to AppSettings.h**

Add new methods after `setColorScheme` (line 43) and new signals after `colorSchemeChanged` (line 57):

After line 43 (`void setColorScheme(const QString &scheme);`), add:

```cpp
    qreal opacity() const;
    bool backgroundBlur() const;
    void setOpacity(qreal opacity);
```

After line 57 (`void colorSchemeChanged(const QString &scheme);`), add:

```cpp
    void opacityChanged(qreal opacity);
    void backgroundBlurChanged(bool enabled);
```

- [ ] **Step 2: Add signal wiring in AppSettings.cpp init()**

In `AppSettings::init()`, inside the `connect(m_dsettings, &Dtk::Core::DSettings::valueChanged, ...)` lambda (currently at line 113), add two new `else if` branches before the closing `});` of the lambda (currently line 126). Add these after the `colorScheme` branch (line 125):

```cpp
        else if (key == "basic.interface.opacity")
            emit opacityChanged(opacity());
        else if (key == "basic.interface.blurred_background")
            emit backgroundBlurChanged(backgroundBlur());
```

- [ ] **Step 3: Add accessor implementations in AppSettings.cpp**

At the end of the file (after `setColorScheme` at line 239), add:

```cpp
qreal AppSettings::opacity() const {
    return m_dsettings->value("basic.interface.opacity").toInt() / 100.0;
}

bool AppSettings::backgroundBlur() const {
    return m_dsettings->value("basic.interface.blurred_background").toBool();
}

void AppSettings::setOpacity(qreal opacity) {
    int val = qBound(20, qRound(opacity * 100.0), 100);
    m_dsettings->setOption("basic.interface.opacity", val);
}
```

Note: `m_dsettings->setOption()` triggers `DSettings::valueChanged`, which fires the lambda from init(), which emits `AppSettings::opacityChanged(qreal)`. No manual signal emit needed.

- [ ] **Step 4: Build to verify compilation**

Run: `cmake --build build 2>&1 | tail -20`
Expected: Build succeeds with no errors.

- [ ] **Step 5: Commit**

```bash
git add src/app/AppSettings.h src/app/AppSettings.cpp
git commit -m "feat: add opacity and backgroundBlur settings API"
```

---

### Task 3: Add setOpacity() to TerminalWidget and modify paint path

**Files:**
- Modify: `src/libqtghostty/TerminalWidget.h:32-37` (add setOpacity declaration)
- Modify: `src/libqtghostty/TerminalWidget.h:179` (add m_opacity member)
- Modify: `src/libqtghostty/TerminalWidget.cpp:544` (widget background fill)
- Modify: `src/libqtghostty/TerminalWidget.cpp:568-569` (back-buffer fill)
- Modify: `src/libqtghostty/TerminalWidget.cpp:608` (default row background)

- [ ] **Step 1: Add setOpacity declaration to TerminalWidget.h**

After `void setScrollbackLines(int lines);` (line 36), add:

```cpp
    void setOpacity(qreal opacity);
```

- [ ] **Step 2: Add m_opacity member to TerminalWidget.h**

After `bool m_isDark = true;` (line 179), add:

```cpp
    qreal m_opacity = 1.0;
```

- [ ] **Step 3: Add setOpacity implementation to TerminalWidget.cpp**

After the `setScrollbackLines` method (line 1226), add:

```cpp
void TerminalWidget::setOpacity(qreal opacity) {
    m_opacity = opacity;
    setAttribute(Qt::WA_TranslucentBackground, m_opacity < 1.0);
    update();
}
```

- [ ] **Step 4: Modify widget background fill (line 544)**

Change line 544 from:
```cpp
    painter.fillRect(rect(), QColor(colors.background.r, colors.background.g, colors.background.b));
```
to:
```cpp
    painter.fillRect(rect(), QColor(colors.background.r, colors.background.g, colors.background.b,
                                    qRound(m_opacity * 255)));
```

- [ ] **Step 5: Modify back-buffer full redraw fill (lines 568-569)**

Change lines 568-569 from:
```cpp
        backPainter.fillRect(QRect(QPoint(0, 0), contentRect.size()),
                             QColor(colors.background.r, colors.background.g, colors.background.b));
```
to:
```cpp
        backPainter.fillRect(QRect(QPoint(0, 0), contentRect.size()),
                             QColor(colors.background.r, colors.background.g, colors.background.b,
                                    qRound(m_opacity * 255)));
```

- [ ] **Step 6: Modify default row background in renderRow (line 608)**

Change line 608 from:
```cpp
    const QColor defaultBackground(colors.background.r, colors.background.g, colors.background.b);
```
to:
```cpp
    const QColor defaultBackground(colors.background.r, colors.background.g, colors.background.b,
                                   qRound(m_opacity * 255));
```

This also affects the `fillRect` on line 610 since `defaultBackground` is used there. The explicit cell background colors on lines 668 and 670 stay fully opaque — only the default background gets alpha.

- [ ] **Step 7: Build to verify compilation**

Run: `cmake --build build 2>&1 | tail -20`
Expected: Build succeeds.

- [ ] **Step 8: Commit**

```bash
git add src/libqtghostty/TerminalWidget.h src/libqtghostty/TerminalWidget.cpp
git commit -m "feat: add setOpacity() to TerminalWidget with alpha-modified paint path"
```

---

### Task 4: Add setOpacity() forwarding to TermPane

**Files:**
- Modify: `src/app/TermPane.h:41-43`
- Modify: `src/app/TermPane.cpp`

- [ ] **Step 1: Add setOpacity declaration to TermPane.h**

After `void setCustomTitle(const QString &title);` (line 42), add:

```cpp
    void setOpacity(qreal opacity);
```

- [ ] **Step 2: Add setOpacity implementation to TermPane.cpp**

After `setCustomTitle` (after line 746), add:

```cpp
void TermPane::setOpacity(qreal opacity) {
    for (auto *term : findChildren<TerminalWidget *>())
        term->setOpacity(opacity);
}
```

- [ ] **Step 3: Build to verify compilation**

Run: `cmake --build build 2>&1 | tail -20`
Expected: Build succeeds.

- [ ] **Step 4: Commit**

```bash
git add src/app/TermPane.h src/app/TermPane.cpp
git commit -m "feat: add setOpacity() forwarding to TermPane"
```

---

### Task 5: Wire up MainWindow — blur, compositor detection, opacity propagation

**Files:**
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`

This is the largest task. It connects everything together.

- [ ] **Step 1: Add includes to MainWindow.cpp**

Add after the existing includes (after line 31):

```cpp
#include <DWindowManagerHelper>
```

- [ ] **Step 2: Add member declarations to MainWindow.h**

After `bool m_hasConfirmedClose = false;` (line 110), add:

```cpp
    bool m_compositorHasBlur = false;
    void initWindowEffects();
    void applyOpacityToAll();
    void setWindowBlurEnabled(bool enabled);
    void onCompositorCapabilitiesChanged();
```

- [ ] **Step 3: Add initWindowEffects() call in MainWindow constructor**

In the constructor body (after `setupShortcuts();` on line 156), add:

```cpp
    initWindowEffects();
```

- [ ] **Step 4: Implement initWindowEffects() in MainWindow.cpp**

Add before `resolveTheme()` (before line 947):

```cpp
void MainWindow::initWindowEffects() {
    auto *wmHelper = DWindowManagerHelper::instance();
    m_compositorHasBlur = wmHelper->hasBlurWindow();

    connect(wmHelper, &DWindowManagerHelper::hasBlurWindowChanged, this,
            &MainWindow::onCompositorCapabilitiesChanged);

    auto *settings = AppSettings::instance();
    connect(settings, &AppSettings::opacityChanged, this, &MainWindow::applyOpacityToAll);
    connect(settings, &AppSettings::backgroundBlurChanged, this, &MainWindow::setWindowBlurEnabled);

    qreal initialOpacity = settings->opacity();
    if (!m_compositorHasBlur)
        initialOpacity = 1.0;
    applyOpacityToAll();

    setWindowBlurEnabled(settings->backgroundBlur());
}
```

- [ ] **Step 5: Implement applyOpacityToAll() in MainWindow.cpp**

Add after `initWindowEffects()`:

```cpp
void MainWindow::applyOpacityToAll() {
    qreal opacity = AppSettings::instance()->opacity();
    if (!m_compositorHasBlur)
        opacity = 1.0;

    for (int i = 0; i < m_stackWidget->count(); ++i) {
        if (auto *pane = qobject_cast<TermPane *>(m_stackWidget->widget(i)))
            pane->setOpacity(opacity);
    }
}
```

- [ ] **Step 6: Implement setWindowBlurEnabled() in MainWindow.cpp**

Add after `applyOpacityToAll()`:

```cpp
void MainWindow::setWindowBlurEnabled(bool enabled) {
    if (!m_compositorHasBlur)
        enabled = false;

    setEnableBlurWindow(enabled);

    bool needsTranslucent = enabled || AppSettings::instance()->opacity() < 1.0;
    setAttribute(Qt::WA_TranslucentBackground, needsTranslucent && m_compositorHasBlur);
}
```

- [ ] **Step 7: Implement onCompositorCapabilitiesChanged() in MainWindow.cpp**

Add after `setWindowBlurEnabled()`:

```cpp
void MainWindow::onCompositorCapabilitiesChanged() {
    m_compositorHasBlur = DWindowManagerHelper::instance()->hasBlurWindow();

    if (!m_compositorHasBlur) {
        setWindowBlurEnabled(false);
    }

    applyOpacityToAll();

    auto *settings = AppSettings::instance();
    setWindowBlurEnabled(settings->backgroundBlur());
}
```

- [ ] **Step 8: Apply opacity to new tabs in addTab()**

In `addTab()`, after the theme application block (after line 375, the closing brace of `if (!m_themes.isEmpty())`), add:

```cpp
    if (m_compositorHasBlur) {
        pane->setOpacity(AppSettings::instance()->opacity());
    }
```

- [ ] **Step 9: Build to verify compilation**

Run: `cmake --build build 2>&1 | tail -20`
Expected: Build succeeds.

- [ ] **Step 10: Commit**

```bash
git add src/app/MainWindow.h src/app/MainWindow.cpp
git commit -m "feat: wire up blur, compositor detection, and opacity propagation in MainWindow"
```

---

### Task 6: Add keyboard shortcuts for opacity adjustment

**Files:**
- Modify: `src/app/TermPane.cpp:444-522` (eventFilter)

- [ ] **Step 1: Add Ctrl+Shift+Up/Down handling in TermPane::eventFilter**

In `TermPane::eventFilter()`, before the final `return false;` (line 521), add:

```cpp
    if (keyEvent->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) {
        auto *settings = AppSettings::instance();
        if (keyEvent->key() == Qt::Key_Up) {
            qreal current = settings->opacity();
            settings->setOpacity(qMin(1.0, current + 0.05));
            return true;
        }
        if (keyEvent->key() == Qt::Key_Down) {
            qreal current = settings->opacity();
            settings->setOpacity(qMax(0.2, current - 0.05));
            return true;
        }
    }
```

This intercepts Ctrl+Shift+Up/Down before they reach the terminal's PTY. The `setOpacity()` call on `AppSettings` triggers the signal chain: `opacityChanged` → `MainWindow::applyOpacityToAll()` → all TermPanes and their TerminalWidgets update.

- [ ] **Step 2: Build to verify compilation**

Run: `cmake --build build 2>&1 | tail -20`
Expected: Build succeeds.

- [ ] **Step 3: Commit**

```bash
git add src/app/TermPane.cpp
git commit -m "feat: add Ctrl+Shift+Up/Down shortcuts for opacity adjustment"
```

---

### Task 7: Run tests and verify

**Files:** None (verification only)

- [ ] **Step 1: Run existing automated tests**

Run: `cmake -B build -DBUILD_TESTING=ON && cmake --build build && cd build && ctest --output-on-failure`
Expected: All existing tests pass.

- [ ] **Step 2: Run clang-format on all changed files**

Run: `clang-format -i $(find src -name '*.cpp' -o -name '*.h')`
Then verify: `clang-format --dry-run --Werror $(find src -name '*.cpp' -o -name '*.h')`
Expected: No errors.

- [ ] **Step 3: Manual smoke test**

Run: `./build/deepin-terminal-ghostty`

Verify:
1. Open Settings → Interface → adjust Opacity slider → background becomes transparent, text stays opaque
2. Toggle "Blur background" checkbox → compositor blur appears/disappears
3. Press Ctrl+Shift+Down → opacity decreases by 5%
4. Press Ctrl+Shift+Up → opacity increases by 5%
5. With blur disabled and opacity at 100%, terminal looks identical to before
