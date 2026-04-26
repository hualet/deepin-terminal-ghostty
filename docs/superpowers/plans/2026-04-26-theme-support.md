# Theme Support Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add terminal color scheme support with system dark/light integration, 7 built-in themes, and UI entry points in both settings dialog and context menu.

**Architecture:** Terminal colors are driven by a `TerminalTheme` data struct in the library layer, loaded from JSON resource files in the app layer. `TerminalWidget::applyTheme()` pushes colors into the Ghostty VT engine. `MainWindow` resolves the active theme (system vs explicit), propagates it to all terminals, syncs DTK window chrome, and adapts the sidebar.

**Tech Stack:** C++20, Qt6, DTK6, libghostty-vt C API

---

## File Structure

| File | Action | Responsibility |
|---|---|---|
| `src/libqtghostty/TerminalTheme.h` | Create | `TerminalTheme` struct (fg, bg, cursor, ansi[16], isDark) |
| `src/app/themes/dark.json` | Create | Dark theme color data |
| `src/app/themes/light.json` | Create | Light theme color data |
| `src/app/themes/bim.json` | Create | Bim theme color data |
| `src/app/themes/tomorrow-night-blue.json` | Create | Tomorrow Night Blue theme color data |
| `src/app/themes/ocean-dark.json` | Create | Ocean Dark theme color data |
| `src/app/themes/hybrid.json` | Create | Hybrid theme color data |
| `src/app/themes/one-light.json` | Create | One Light theme color data |
| `src/app/ThemeLoader.h` | Create | `loadThemes()`, `findTheme()` declarations |
| `src/app/ThemeLoader.cpp` | Create | `loadThemes()`, `findTheme()` implementations |
| `src/app/settings/resources.qrc` | Modify | Register theme JSON files as Qt resources |
| `src/app/settings/default-config.json` | Modify | Add `theme` combobox option |
| `src/app/AppSettings.h` | Modify | Add `theme()`, `setTheme()`, `themeChanged` signal |
| `src/app/AppSettings.cpp` | Modify | Implement theme accessors, populate combo items |
| `src/libqtghostty/TerminalWidget.h` | Modify | Add `applyTheme()`, `m_isDark`, debug color getters |
| `src/libqtghostty/TerminalWidget.cpp` | Modify | Implement `applyTheme()`, fix `effectColorScheme` |
| `src/app/MainWindow.h` | Modify | Add `resolveTheme()`, `applyThemeToAll()` |
| `src/app/MainWindow.cpp` | Modify | Theme resolution, DTK integration, propagation |
| `src/app/TermPane.cpp` | Modify | Theme submenu in context menu |
| `src/app/VerticalTabSidebar.h` | Modify | Add `setDarkMode(bool)` |
| `src/app/VerticalTabSidebar.cpp` | Modify | Implement `setDarkMode` with light stylesheet |
| `tests/test_terminal_widget.cpp` | Modify | Add `testApplyThemeSetsColors` |
| `tests/test_main_window.cpp` | Modify | Add theme propagation tests |
| `tests/CMakeLists.txt` | Modify | Add ThemeLoader.cpp to test_main_window |

---

### Task 1: TerminalTheme Data Model

**Files:**
- Create: `src/libqtghostty/TerminalTheme.h`

The struct lives in the library layer so `TerminalWidget` can use it without depending on app code.

- [ ] **Step 1: Create TerminalTheme.h**

```cpp
// src/libqtghostty/TerminalTheme.h
#pragma once

#include <QColor>
#include <QString>

struct TerminalTheme {
    QString name;
    QString displayName;
    bool isDark = true;
    QColor foreground;
    QColor background;
    QColor cursor;
    QColor ansi[16];
};
```

- [ ] **Step 2: Verify header compiles**

Run: `cmake -B build 2>&1 | tail -5`
Expected: CMake configures successfully (no compile step needed yet).

- [ ] **Step 3: Commit**

```bash
git add src/libqtghostty/TerminalTheme.h
git commit -m "feat: add TerminalTheme data model"
```

---

### Task 2: Theme JSON Files + Resource Registration

**Files:**
- Create: `src/app/themes/dark.json`
- Create: `src/app/themes/light.json`
- Create: `src/app/themes/bim.json`
- Create: `src/app/themes/tomorrow-night-blue.json`
- Create: `src/app/themes/ocean-dark.json`
- Create: `src/app/themes/hybrid.json`
- Create: `src/app/themes/one-light.json`
- Modify: `src/app/settings/resources.qrc`

All color values migrated from original deepin-terminal `.colorscheme` files.

- [ ] **Step 1: Create all 7 theme JSON files**

`src/app/themes/dark.json`:
```json
{
    "name": "dark",
    "displayName": "Dark",
    "isDark": true,
    "foreground": [0, 205, 0],
    "background": [37, 37, 37],
    "cursor": [0, 205, 0],
    "ansi": [
        [0, 0, 0], [178, 24, 24], [24, 178, 24], [178, 104, 24],
        [52, 101, 164], [225, 30, 225], [24, 178, 178], [238, 232, 213],
        [104, 104, 104], [255, 84, 84], [133, 153, 0], [255, 255, 84],
        [52, 101, 164], [30, 144, 255], [253, 246, 227], [255, 255, 255]
    ]
}
```

`src/app/themes/light.json`:
```json
{
    "name": "light",
    "displayName": "Light",
    "isDark": false,
    "foreground": [0, 0, 0],
    "background": [248, 248, 248],
    "cursor": [0, 0, 0],
    "ansi": [
        [0, 0, 0], [178, 24, 24], [24, 178, 24], [178, 104, 24],
        [24, 24, 178], [225, 30, 225], [24, 178, 178], [238, 232, 213],
        [104, 104, 104], [255, 84, 84], [133, 153, 0], [233, 233, 79],
        [52, 101, 164], [30, 144, 255], [24, 178, 178], [238, 232, 213]
    ]
}
```

`src/app/themes/bim.json`:
```json
{
    "name": "bim",
    "displayName": "Bim",
    "isDark": true,
    "foreground": [255, 213, 0],
    "background": [1, 40, 73],
    "cursor": [255, 213, 0],
    "ansi": [
        [44, 36, 35], [178, 24, 24], [24, 178, 24], [245, 162, 85],
        [24, 24, 178], [169, 87, 236], [94, 238, 160], [145, 137, 136],
        [44, 36, 35], [255, 84, 84], [169, 238, 85], [245, 162, 85],
        [94, 162, 236], [169, 87, 236], [94, 238, 160], [145, 137, 136]
    ]
}
```

`src/app/themes/tomorrow-night-blue.json`:
```json
{
    "name": "tomorrow-night-blue",
    "displayName": "Tomorrow Night Blue",
    "isDark": true,
    "foreground": [141, 178, 172],
    "background": [29, 31, 33],
    "cursor": [141, 178, 172],
    "ansi": [
        [0, 0, 0], [178, 24, 24], [24, 178, 24], [240, 198, 116],
        [24, 24, 178], [178, 147, 187], [138, 190, 183], [255, 254, 254],
        [0, 0, 0], [255, 84, 84], [181, 189, 104], [240, 198, 116],
        [129, 162, 190], [178, 147, 187], [138, 190, 183], [255, 254, 254]
    ]
}
```

`src/app/themes/ocean-dark.json`:
```json
{
    "name": "ocean-dark",
    "displayName": "Ocean Dark",
    "isDark": true,
    "foreground": [99, 124, 206],
    "background": [28, 31, 39],
    "cursor": [99, 124, 206],
    "ansi": [
        [79, 79, 79], [178, 24, 24], [24, 178, 24], [229, 192, 121],
        [24, 24, 178], [164, 121, 157], [133, 166, 165], [238, 237, 238],
        [79, 79, 79], [255, 84, 84], [175, 211, 131], [229, 192, 121],
        [125, 144, 164], [164, 121, 157], [133, 166, 165], [238, 237, 238]
    ]
}
```

`src/app/themes/hybrid.json`:
```json
{
    "name": "hybrid",
    "displayName": "Hybrid",
    "isDark": true,
    "foreground": [254, 144, 0],
    "background": [20, 20, 20],
    "cursor": [254, 144, 0],
    "ansi": [
        [40, 42, 46], [178, 24, 24], [24, 178, 24], [222, 147, 95],
        [24, 24, 178], [133, 103, 143], [94, 141, 135], [150, 152, 150],
        [40, 42, 46], [255, 84, 84], [140, 148, 64], [222, 147, 95],
        [95, 129, 157], [133, 103, 143], [94, 141, 135], [150, 152, 150]
    ]
}
```

`src/app/themes/one-light.json`:
```json
{
    "name": "one-light",
    "displayName": "One Light",
    "isDark": false,
    "foreground": [0, 0, 0],
    "background": [248, 248, 248],
    "cursor": [0, 0, 0],
    "ansi": [
        [0, 0, 0], [178, 24, 24], [24, 178, 24], [133, 85, 4],
        [24, 24, 178], [147, 0, 146], [14, 111, 173], [142, 143, 150],
        [0, 0, 0], [255, 84, 84], [65, 147, 62], [133, 85, 4],
        [49, 94, 238], [147, 0, 146], [14, 111, 173], [142, 143, 150]
    ]
}
```

- [ ] **Step 2: Register theme files in resources.qrc**

Add a new `<qresource>` block to `src/app/settings/resources.qrc` before the closing `</RCC>`:

```xml
    <qresource prefix="/themes">
        <file alias="dark.json">../themes/dark.json</file>
        <file alias="light.json">../themes/light.json</file>
        <file alias="bim.json">../themes/bim.json</file>
        <file alias="tomorrow-night-blue.json">../themes/tomorrow-night-blue.json</file>
        <file alias="ocean-dark.json">../themes/ocean-dark.json</file>
        <file alias="hybrid.json">../themes/hybrid.json</file>
        <file alias="one-light.json">../themes/one-light.json</file>
    </qresource>
```

- [ ] **Step 3: Commit**

```bash
git add src/app/themes/ src/app/settings/resources.qrc
git commit -m "feat: add built-in theme JSON files and register as resources"
```

---

### Task 3: ThemeLoader + AppSettings Theme Key

**Files:**
- Create: `src/app/ThemeLoader.h`
- Create: `src/app/ThemeLoader.cpp`
- Modify: `src/app/settings/default-config.json`
- Modify: `src/app/AppSettings.h`
- Modify: `src/app/AppSettings.cpp`

- [ ] **Step 1: Create ThemeLoader.h**

```cpp
// src/app/ThemeLoader.h
#pragma once

#include "libqtghostty/TerminalTheme.h"

#include <QList>
#include <QString>

namespace ThemeLoader {
QList<TerminalTheme> loadThemes();
TerminalTheme findTheme(const QList<TerminalTheme> &themes, const QString &name);
}
```

- [ ] **Step 2: Create ThemeLoader.cpp**

```cpp
// src/app/ThemeLoader.cpp
#include "ThemeLoader.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace ThemeLoader {

QList<TerminalTheme> loadThemes() {
    QList<TerminalTheme> themes;
    QDir themesDir(QStringLiteral(":/themes"));
    const QStringList files = themesDir.entryList(QStringList() << QStringLiteral("*.json"), QDir::Files);
    for (const QString &fileName : files) {
        QFile file(QStringLiteral(":/themes/") + fileName);
        if (!file.open(QFile::ReadOnly))
            continue;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (!doc.isObject())
            continue;
        QJsonObject obj = doc.object();

        auto parseColor = [](const QJsonArray &arr) -> QColor {
            if (arr.size() != 3)
                return QColor();
            return QColor(arr[0].toInt(), arr[1].toInt(), arr[2].toInt());
        };

        TerminalTheme theme;
        theme.name = obj[QStringLiteral("name")].toString();
        theme.displayName = obj[QStringLiteral("displayName")].toString();
        theme.isDark = obj[QStringLiteral("isDark")].toBool(true);
        theme.foreground = parseColor(obj[QStringLiteral("foreground")].toArray());
        theme.background = parseColor(obj[QStringLiteral("background")].toArray());
        theme.cursor = parseColor(obj[QStringLiteral("cursor")].toArray());

        QJsonArray ansiArr = obj[QStringLiteral("ansi")].toArray();
        for (int i = 0; i < 16 && i < ansiArr.size(); ++i)
            theme.ansi[i] = parseColor(ansiArr[i].toArray());

        themes.append(theme);
    }
    return themes;
}

TerminalTheme findTheme(const QList<TerminalTheme> &themes, const QString &name) {
    for (const auto &t : themes) {
        if (t.name == name)
            return t;
    }
    for (const auto &t : themes) {
        if (t.name == QStringLiteral("dark"))
            return t;
    }
    return themes.isEmpty() ? TerminalTheme() : themes.first();
}

} // namespace ThemeLoader
```

- [ ] **Step 3: Add theme combobox to default-config.json**

Insert after the `verticalTabs` option (before the closing `]` of the `interface` group's `options` array). In `src/app/settings/default-config.json`, add this option after the `verticalTabs` entry at line 52:

```json
                        {
                            "key": "theme",
                            "name": "Theme",
                            "type": "combobox",
                            "default": "system"
                        }
```

The full `interface` options array becomes:
```json
                    "options": [
                        {
                            "key": "fontFamily",
                            "name": "Font family",
                            "type": "combobox",
                            "default": "Monospace"
                        },
                        {
                            "key": "fontSize",
                            "name": "Font size",
                            "type": "spinbutton",
                            "default": 11,
                            "min": 5,
                            "max": 72,
                            "step": 1
                        },
                        {
                            "key": "cursorShape",
                            "name": "Cursor shape",
                            "type": "combobox",
                            "default": 0
                        },
                        {
                            "key": "cursorBlink",
                            "text": "Cursor blink",
                            "type": "checkbox",
                            "default": true
                        },
                        {
                            "key": "scrollbackLines",
                            "name": "Scrollback lines",
                            "type": "spinbutton",
                            "default": 1000,
                            "min": 100,
                            "max": 50000,
                            "step": 100
                        },
                        {
                            "key": "verticalTabs",
                            "text": "Vertical Tabs",
                            "type": "checkbox",
                            "default": false
                        },
                        {
                            "key": "theme",
                            "name": "Theme",
                            "type": "combobox",
                            "default": "system"
                        }
                    ]
```

- [ ] **Step 4: Add theme accessors to AppSettings.h**

Add to `src/app/AppSettings.h`, after the `setVerticalTabsEnabled` declaration at line 35:

```cpp
    QString theme() const;
    void setTheme(const QString &theme);
```

Add to the `signals:` section after `verticalTabsEnabledChanged` at line 45:

```cpp
    void themeChanged(const QString &theme);
```

- [ ] **Step 5: Implement theme accessors in AppSettings.cpp**

Add include at the top of `src/app/AppSettings.cpp`:

```cpp
#include "ThemeLoader.h"
```

In `AppSettings::init()`, after the cursor shape population block (after line 90), add theme combo population:

```cpp
    auto themeOption = m_dsettings->option("basic.interface.theme");
    if (themeOption) {
        QMap<QString, QVariant> items;
        QStringList keys;
        QStringList values;
        keys.append(QStringLiteral("system"));
        values.append(tr("System"));
        auto themes = ThemeLoader::loadThemes();
        for (const auto &t : themes) {
            keys.append(t.name);
            values.append(t.displayName);
        }
        items.insert(QStringLiteral("keys"), keys);
        items.insert(QStringLiteral("values"), values);
        themeOption->setData(QStringLiteral("items"), items);
    }
```

In the `valueChanged` lambda (line 92-103), add a new branch after the `verticalTabs` check:

```cpp
        else if (key == "basic.interface.theme")
            emit themeChanged(theme());
```

Add accessor implementations at the end of the file (before the closing of the file):

```cpp
QString AppSettings::theme() const {
    return m_dsettings->value("basic.interface.theme").toString();
}

void AppSettings::setTheme(const QString &theme) {
    m_dsettings->setOption("basic.interface.theme", theme);
}
```

- [ ] **Step 6: Add ThemeLoader.cpp to main executable CMakeLists.txt**

In the root `CMakeLists.txt`, add `src/app/ThemeLoader.cpp` to the `deepin-terminal-ghostty` executable source list (after `src/app/AppSettings.cpp` at line 226):

```cpp
  src/app/ThemeLoader.cpp
```

- [ ] **Step 7: Add ThemeLoader.cpp to test CMakeLists.txt**

In `tests/CMakeLists.txt`, add to the `test_main_window` target sources (after `${CMAKE_SOURCE_DIR}/src/app/AppSettings.cpp` at line 104):

```cmake
  ${CMAKE_SOURCE_DIR}/src/app/ThemeLoader.cpp
```

Also add to `test_appsettings` target sources (after `${CMAKE_SOURCE_DIR}/src/app/AppSettings.cpp` at line 15):

```cmake
  ${CMAKE_SOURCE_DIR}/src/app/ThemeLoader.cpp
```

- [ ] **Step 8: Verify build compiles**

Run: `cmake -B build && cmake --build build 2>&1 | tail -20`
Expected: Build succeeds with no errors.

- [ ] **Step 9: Commit**

```bash
git add src/app/ThemeLoader.h src/app/ThemeLoader.cpp src/app/settings/default-config.json src/app/AppSettings.h src/app/AppSettings.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add ThemeLoader and theme setting to AppSettings"
```

---

### Task 4: TerminalWidget applyTheme + effectColorScheme Fix

**Files:**
- Modify: `src/libqtghostty/TerminalWidget.h`
- Modify: `src/libqtghostty/TerminalWidget.cpp`

- [ ] **Step 1: Add applyTheme declaration and m_isDark member to TerminalWidget.h**

In `src/libqtghostty/TerminalWidget.h`, add include after the existing includes (after `#include <QWidget>` at line 10):

```cpp
#include "TerminalTheme.h"
```

Add public method after `setScrollbackLines` (at line 33):

```cpp
    void applyTheme(const TerminalTheme &theme);
```

Add member variable in the private section, after `m_scrollbackLines` (at line 171):

```cpp
    bool m_isDark = true;
```

Add debug methods under the `QTGHOSTTY_TESTING` section (after `debugSelectedText` at line 55):

```cpp
    bool debugAppliedIsDark() const;
    QColor debugAppliedForeground() const;
    QColor debugAppliedBackground() const;
```

- [ ] **Step 2: Implement applyTheme and debug methods in TerminalWidget.cpp**

Add `#include "TerminalTheme.h"` is already in the header, so no additional include needed in the .cpp.

Add the `applyTheme` method implementation. Insert before the `paintEvent` method. In `src/libqtghostty/TerminalWidget.cpp`, add after the constructor/destructor section:

```cpp
void TerminalWidget::applyTheme(const TerminalTheme &theme) {
    if (!m_terminal)
        return;

    auto toRgb = [](const QColor &c) -> GhosttyColorRgb {
        return {static_cast<uint8_t>(c.red()), static_cast<uint8_t>(c.green()), static_cast<uint8_t>(c.blue())};
    };

    auto fg = toRgb(theme.foreground);
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_COLOR_FOREGROUND, &fg);

    auto bg = toRgb(theme.background);
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_COLOR_BACKGROUND, &bg);

    auto cr = toRgb(theme.cursor);
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_COLOR_CURSOR, &cr);

    GhosttyColorRgb palette[256] = {};
    ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_COLOR_PALETTE, palette);
    for (int i = 0; i < 16; ++i)
        palette[i] = toRgb(theme.ansi[i]);
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_COLOR_PALETTE, palette);

    m_isDark = theme.isDark;
    m_renderStateDirty = true;
    update();
}
```

Add debug method implementations (at the end of the file, before the `#include` moc line):

```cpp
#ifdef QTGHOSTTY_TESTING
bool TerminalWidget::debugAppliedIsDark() const {
    return m_isDark;
}

QColor TerminalWidget::debugAppliedForeground() const {
    if (!m_terminal)
        return QColor();
    GhosttyColorRgb rgb;
    ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_COLOR_FOREGROUND, &rgb);
    return QColor(rgb.r, rgb.g, rgb.b);
}

QColor TerminalWidget::debugAppliedBackground() const {
    if (!m_terminal)
        return QColor();
    GhosttyColorRgb rgb;
    ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_COLOR_BACKGROUND, &rgb);
    return QColor(rgb.r, rgb.g, rgb.b);
}
#endif
```

- [ ] **Step 3: Fix effectColorScheme callback**

In `src/libqtghostty/TerminalWidget.cpp`, replace the `effectColorScheme` function (lines 209-214):

Replace:
```cpp
bool effectColorScheme(GhosttyTerminal terminal, void *userdata, GhosttyColorScheme *out_scheme) {
    (void)terminal;
    (void)userdata;
    (void)out_scheme;
    return false;
}
```

With:
```cpp
bool effectColorScheme(GhosttyTerminal terminal, void *userdata, GhosttyColorScheme *out_scheme) {
    (void)terminal;
    auto *widget = static_cast<TerminalWidget *>(userdata);
    *out_scheme = widget->m_isDark ? GHOSTTY_COLOR_SCHEME_DARK : GHOSTTY_COLOR_SCHEME_LIGHT;
    return true;
}
```

- [ ] **Step 4: Verify build compiles**

Run: `cmake --build build 2>&1 | tail -20`
Expected: Build succeeds.

- [ ] **Step 5: Commit**

```bash
git add src/libqtghostty/TerminalWidget.h src/libqtghostty/TerminalWidget.cpp
git commit -m "feat: add TerminalWidget::applyTheme and fix effectColorScheme"
```

---

### Task 5: MainWindow Theme Propagation

**Files:**
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`

- [ ] **Step 1: Add theme methods to MainWindow.h**

Add includes at the top of `src/app/MainWindow.h` (after existing includes):

```cpp
#include "TerminalTheme.h"
```

Add private methods in the `private:` section, after `onConnectRemoteServer` (line 86):

```cpp
    TerminalTheme resolveTheme() const;
    void applyThemeToAll();
```

Add a cached themes member after `m_startupSessionHandled` (line 102):

```cpp
    QList<TerminalTheme> m_themes;
```

- [ ] **Step 2: Add theme includes to MainWindow.cpp**

At the top of `src/app/MainWindow.cpp`, add:

```cpp
#include "ThemeLoader.h"
#include <DGuiApplicationHelper>
```

- [ ] **Step 3: Implement resolveTheme and applyThemeToAll in MainWindow.cpp**

Add these methods at the end of the file (before the `#include` moc line or the last method):

```cpp
TerminalTheme MainWindow::resolveTheme() const {
    if (m_themes.isEmpty()) {
        // Mutable workaround for const method
        const_cast<MainWindow *>(this)->m_themes = ThemeLoader::loadThemes();
    }
    QString setting = AppSettings::instance()->theme();
    if (setting == QStringLiteral("system")) {
        auto type = DGuiApplicationHelper::instance()->themeType();
        return ThemeLoader::findTheme(m_themes, type == DGuiApplicationHelper::DarkType
                                                   ? QStringLiteral("dark")
                                                   : QStringLiteral("light"));
    }
    return ThemeLoader::findTheme(m_themes, setting);
}

void MainWindow::applyThemeToAll() {
    auto theme = resolveTheme();
    for (int i = 0; i < m_stackWidget->count(); ++i) {
        auto *pane = qobject_cast<TermPane *>(m_stackWidget->widget(i));
        if (!pane)
            continue;
        for (auto *term : pane->findChildren<TerminalWidget *>())
            term->applyTheme(theme);
    }

    auto *helper = DGuiApplicationHelper::instance();
    QString setting = AppSettings::instance()->theme();
    if (setting == QStringLiteral("system"))
        helper->setPaletteType(DGuiApplicationHelper::UnknownType);
    else
        helper->setPaletteType(theme.isDark ? DGuiApplicationHelper::DarkType
                                            : DGuiApplicationHelper::LightType);

    if (m_verticalSidebar)
        m_verticalSidebar->setDarkMode(theme.isDark);
}
```

- [ ] **Step 4: Wire theme connections in MainWindow constructor**

In `src/app/MainWindow.cpp`, in the constructor, after the existing settings connections block (after the `verticalTabsEnabledChanged` connection at line 117), add:

```cpp
    connect(settings, &AppSettings::themeChanged, this, [this]() {
        applyThemeToAll();
    });
    connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged, this, [this]() {
        if (AppSettings::instance()->theme() == QStringLiteral("system"))
            applyThemeToAll();
    });
```

After `addTab(true, initialSessionOptions)` at line 130, add:

```cpp
    m_themes = ThemeLoader::loadThemes();
    applyThemeToAll();
```

This loads themes once and applies to the initial tab. The `addTab` call is at line 130 — insert the theme loading immediately after it.

- [ ] **Step 5: Apply theme when new tabs are created**

In the `addTab` method, the new pane's terminal needs the current theme. Find the end of the `addTab` method (where the tab record is set up and connections are wired). After the pane is added to the stack and connections are established, add theme application.

In the `addTab` method, after the pane connections are wired (find where `connect(pane, ...)` calls are), add before the method returns:

```cpp
    if (!m_themes.isEmpty()) {
        auto theme = resolveTheme();
        for (auto *term : pane->findChildren<TerminalWidget *>())
            term->applyTheme(theme);
    }
```

- [ ] **Step 6: Verify build compiles**

Run: `cmake --build build 2>&1 | tail -20`
Expected: Build succeeds.

- [ ] **Step 7: Commit**

```bash
git add src/app/MainWindow.h src/app/MainWindow.cpp
git commit -m "feat: wire theme propagation in MainWindow with DTK integration"
```

---

### Task 6: Context Menu Theme Submenu

**Files:**
- Modify: `src/app/TermPane.cpp`

- [ ] **Step 1: Add Theme submenu to showTerminalContextMenu**

In `src/app/TermPane.cpp`, add include at the top:

```cpp
#include "AppSettings.h"
#include "ThemeLoader.h"
```

In the `showTerminalContextMenu` method (line 497), add a Theme submenu before the Settings action. Insert after the `closeSplitAction` line and before the second `menu.addSeparator()` before `settingsAction` (around line 516):

Replace the context menu method. The current code at lines 497-529:

```cpp
void TermPane::showTerminalContextMenu(TerminalWidget *term, const QPoint &globalPos) {
    if (!term)
        return;

    QMenu menu(this);

    if (term->hasSelection())
        menu.addAction(tr("Copy"), term, &TerminalWidget::copyToClipboard);

    auto *pasteAction = menu.addAction(tr("Paste"), term, &TerminalWidget::pasteFromClipboard);
    pasteAction->setEnabled(!QGuiApplication::clipboard()->text().isEmpty());

    menu.addSeparator();
    auto *searchAction = menu.addAction(tr("Search"));
    menu.addSeparator();
    auto *hSplitAction = menu.addAction(tr("Horizontal Split"));
    auto *vSplitAction = menu.addAction(tr("Vertical Split"));
    menu.addSeparator();
    auto *closeSplitAction = menu.addAction(tr("Close Split"));
    menu.addSeparator();
    auto *settingsAction = menu.addAction(tr("Settings"));

    auto *action = menu.exec(globalPos);
    if (action == searchAction)
        showSearchBar();
    else if (action == hSplitAction)
        splitCurrent(Qt::Horizontal);
    else if (action == vSplitAction)
        splitCurrent(Qt::Vertical);
    else if (action == closeSplitAction)
        closeCurrentSplit();
    else if (action == settingsAction)
        Q_EMIT requestSettings();
}
```

Replace with:

```cpp
void TermPane::showTerminalContextMenu(TerminalWidget *term, const QPoint &globalPos) {
    if (!term)
        return;

    QMenu menu(this);

    if (term->hasSelection())
        menu.addAction(tr("Copy"), term, &TerminalWidget::copyToClipboard);

    auto *pasteAction = menu.addAction(tr("Paste"), term, &TerminalWidget::pasteFromClipboard);
    pasteAction->setEnabled(!QGuiApplication::clipboard()->text().isEmpty());

    menu.addSeparator();
    auto *searchAction = menu.addAction(tr("Search"));
    menu.addSeparator();
    auto *hSplitAction = menu.addAction(tr("Horizontal Split"));
    auto *vSplitAction = menu.addAction(tr("Vertical Split"));
    menu.addSeparator();
    auto *closeSplitAction = menu.addAction(tr("Close Split"));
    menu.addSeparator();

    auto *themeMenu = menu.addMenu(tr("Theme"));
    QString currentTheme = AppSettings::instance()->theme();
    QAction *systemAction = themeMenu->addAction(tr("System"));
    systemAction->setCheckable(true);
    systemAction->setChecked(currentTheme == QStringLiteral("system"));
    themeMenu->addSeparator();
    auto themes = ThemeLoader::loadThemes();
    QList<QAction *> themeActions;
    for (const auto &t : themes) {
        auto *act = themeMenu->addAction(t.displayName);
        act->setCheckable(true);
        act->setChecked(currentTheme == t.name);
        act->setData(t.name);
        themeActions.append(act);
    }

    menu.addSeparator();
    auto *settingsAction = menu.addAction(tr("Settings"));

    auto *action = menu.exec(globalPos);
    if (action == searchAction)
        showSearchBar();
    else if (action == hSplitAction)
        splitCurrent(Qt::Horizontal);
    else if (action == vSplitAction)
        splitCurrent(Qt::Vertical);
    else if (action == closeSplitAction)
        closeCurrentSplit();
    else if (action == systemAction)
        AppSettings::instance()->setTheme(QStringLiteral("system"));
    else if (themeActions.contains(action))
        AppSettings::instance()->setTheme(action->data().toString());
    else if (action == settingsAction)
        Q_EMIT requestSettings();
}
```

- [ ] **Step 2: Verify build compiles**

Run: `cmake --build build 2>&1 | tail -20`
Expected: Build succeeds.

- [ ] **Step 3: Commit**

```bash
git add src/app/TermPane.cpp
git commit -m "feat: add Theme submenu to terminal context menu"
```

---

### Task 7: VerticalTabSidebar Dark/Light Adaptation

**Files:**
- Modify: `src/app/VerticalTabSidebar.h`
- Modify: `src/app/VerticalTabSidebar.cpp`

- [ ] **Step 1: Add setDarkMode declaration to VerticalTabSidebar.h**

In `src/app/VerticalTabSidebar.h`, add after the `items()` method declaration (line 26):

```cpp
    void setDarkMode(bool dark);
```

Add a private member after `m_layout` (line 40):

```cpp
    bool m_darkMode = true;
```

- [ ] **Step 2: Implement setDarkMode in VerticalTabSidebar.cpp**

Add the following method implementation. In `src/app/VerticalTabSidebar.cpp`, add at the end of the file (before the last `rebuild` method or after the constructor):

```cpp
void VerticalTabSidebar::setDarkMode(bool dark) {
    if (m_darkMode == dark)
        return;
    m_darkMode = dark;
    if (dark) {
        setStyleSheet(QStringLiteral(R"(
            #verticalTabSidebar {
                background: rgba(12, 15, 22, 0.72);
            }
            #verticalTabSidebarScrollArea {
                border: none;
                background: transparent;
            }
            #verticalTabSidebarContent {
                background: transparent;
            }
            #verticalTabSection {
                border: 1px solid rgba(255, 255, 255, 0.05);
                border-radius: 12px;
                background-color: rgba(29, 32, 40, 0.96);
            }
            #verticalTabSection:hover {
                background-color: rgba(37, 41, 51, 0.98);
            }
            #verticalTabSection[isCurrent="true"] {
                border: 1px solid rgba(83, 143, 255, 0.45);
                background-color: rgba(30, 45, 74, 0.98);
            }
            #verticalTabHeader {
                background: transparent;
            }
            #verticalTabSection[isCurrent="true"] #verticalTabButton {
                color: rgb(242, 247, 255);
            }
            #verticalTabExpandButton {
                border: none;
                background: transparent;
                padding: 0px 1px;
                margin: 0px;
                color: rgba(210, 218, 232, 0.7);
            }
            #verticalTabExpandButton:hover {
                background-color: rgba(255, 255, 255, 0.08);
                border-radius: 6px;
            }
            #verticalTabButton {
                border: none;
                background: transparent;
                padding: 6px 0px;
                margin: 0px;
                color: rgb(220, 225, 236);
                text-align: left;
                font-size: 15px;
            }
            #verticalTabButton:checked {
                font-weight: 600;
            }
            #verticalTabBadge,
            #verticalPaneBadge {
                border-radius: 12px;
                background-color: rgba(255, 255, 255, 0.08);
                color: rgb(220, 225, 236);
            }
            #verticalTabSection[isCurrent="true"] #verticalTabBadge {
                background-color: rgba(83, 143, 255, 0.18);
            }
            #verticalPaneList {
                background: transparent;
            }
            #verticalPaneGuide {
                min-width: 1px;
                max-width: 1px;
                background-color: rgba(255, 255, 255, 0.12);
                margin-top: 2px;
                margin-bottom: 4px;
            }
            #verticalPaneButton {
                border: none;
                background: transparent;
                padding: 5px 0px;
                margin: 0px;
                color: rgb(183, 191, 204);
                text-align: left;
                font-size: 14px;
            }
            #verticalPaneButton[active="true"] {
                color: rgb(229, 235, 245);
            }
            #verticalPaneButton:checked {
                font-weight: 500;
            }
        )"));
    } else {
        setStyleSheet(QStringLiteral(R"(
            #verticalTabSidebar {
                background: rgba(235, 235, 235, 0.9);
            }
            #verticalTabSidebarScrollArea {
                border: none;
                background: transparent;
            }
            #verticalTabSidebarContent {
                background: transparent;
            }
            #verticalTabSection {
                border: 1px solid rgba(0, 0, 0, 0.06);
                border-radius: 12px;
                background-color: rgba(255, 255, 255, 0.96);
            }
            #verticalTabSection:hover {
                background-color: rgba(245, 245, 245, 0.98);
            }
            #verticalTabSection[isCurrent="true"] {
                border: 1px solid rgba(83, 143, 255, 0.4);
                background-color: rgba(230, 240, 255, 0.98);
            }
            #verticalTabHeader {
                background: transparent;
            }
            #verticalTabSection[isCurrent="true"] #verticalTabButton {
                color: rgb(20, 40, 80);
            }
            #verticalTabExpandButton {
                border: none;
                background: transparent;
                padding: 0px 1px;
                margin: 0px;
                color: rgba(60, 60, 60, 0.7);
            }
            #verticalTabExpandButton:hover {
                background-color: rgba(0, 0, 0, 0.05);
                border-radius: 6px;
            }
            #verticalTabButton {
                border: none;
                background: transparent;
                padding: 6px 0px;
                margin: 0px;
                color: rgb(40, 40, 40);
                text-align: left;
                font-size: 15px;
            }
            #verticalTabButton:checked {
                font-weight: 600;
            }
            #verticalTabBadge,
            #verticalPaneBadge {
                border-radius: 12px;
                background-color: rgba(0, 0, 0, 0.05);
                color: rgb(50, 50, 50);
            }
            #verticalTabSection[isCurrent="true"] #verticalTabBadge {
                background-color: rgba(83, 143, 255, 0.15);
            }
            #verticalPaneList {
                background: transparent;
            }
            #verticalPaneGuide {
                min-width: 1px;
                max-width: 1px;
                background-color: rgba(0, 0, 0, 0.08);
                margin-top: 2px;
                margin-bottom: 4px;
            }
            #verticalPaneButton {
                border: none;
                background: transparent;
                padding: 5px 0px;
                margin: 0px;
                color: rgb(80, 80, 80);
                text-align: left;
                font-size: 14px;
            }
            #verticalPaneButton[active="true"] {
                color: rgb(30, 30, 30);
            }
            #verticalPaneButton:checked {
                font-weight: 500;
            }
        )"));
    }
}
```

Also, in the constructor, set `m_darkMode = true` (the default dark stylesheet is already applied).

- [ ] **Step 3: Verify build compiles**

Run: `cmake --build build 2>&1 | tail -20`
Expected: Build succeeds.

- [ ] **Step 4: Commit**

```bash
git add src/app/VerticalTabSidebar.h src/app/VerticalTabSidebar.cpp
git commit -m "feat: add setDarkMode to VerticalTabSidebar for light/dark adaptation"
```

---

### Task 8: Tests

**Files:**
- Modify: `tests/test_terminal_widget.cpp`
- Modify: `tests/test_main_window.cpp`
- Modify: `tests/CMakeLists.txt` (already done in Task 3)

- [ ] **Step 1: Add testApplyThemeSetsColors to test_terminal_widget.cpp**

Add the include at the top (after the existing includes):

```cpp
#include "TerminalTheme.h"
```

Add the test method declaration in the `TestTerminalWidget` class (after `testSelectedTextBottomRightToTopLeft`):

```cpp
    void testApplyThemeSetsColors();
```

Add the test implementation before `main()`:

```cpp
void TestTerminalWidget::testApplyThemeSetsColors() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());
    widget.resize(960, 640);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    TerminalTheme theme;
    theme.name = QStringLiteral("test");
    theme.displayName = QStringLiteral("Test");
    theme.isDark = true;
    theme.foreground = QColor(255, 0, 0);
    theme.background = QColor(0, 0, 255);
    theme.cursor = QColor(0, 255, 0);
    for (int i = 0; i < 16; ++i)
        theme.ansi[i] = QColor(i * 15, i * 15, i * 15);

    widget.applyTheme(theme);
    QApplication::processEvents();

    QVERIFY(widget.debugAppliedIsDark());
    QCOMPARE(widget.debugAppliedForeground(), QColor(255, 0, 0));
    QCOMPARE(widget.debugAppliedBackground(), QColor(0, 0, 255));
}
```

- [ ] **Step 2: Add theme tests to test_main_window.cpp**

Add includes at the top:

```cpp
#include "ThemeLoader.h"
```

Add test method declarations in the `TestMainWindow` class:

```cpp
    void testThemeLoaderLoadsAllThemes();
    void testThemeLoaderFindsThemeByName();
    void testThemeSettingDefaultIsSystem();
    void testThemeChangeAppliesToAllTerminals();
```

Add test implementations before `main()`:

```cpp
void TestMainWindow::testThemeLoaderLoadsAllThemes() {
    auto themes = ThemeLoader::loadThemes();
    QCOMPARE(themes.size(), 7);

    QStringList names;
    for (const auto &t : themes)
        names.append(t.name);

    QVERIFY(names.contains(QStringLiteral("dark")));
    QVERIFY(names.contains(QStringLiteral("light")));
    QVERIFY(names.contains(QStringLiteral("bim")));
    QVERIFY(names.contains(QStringLiteral("tomorrow-night-blue")));
    QVERIFY(names.contains(QStringLiteral("ocean-dark")));
    QVERIFY(names.contains(QStringLiteral("hybrid")));
    QVERIFY(names.contains(QStringLiteral("one-light")));
}

void TestMainWindow::testThemeLoaderFindsThemeByName() {
    auto themes = ThemeLoader::loadThemes();

    auto bim = ThemeLoader::findTheme(themes, QStringLiteral("bim"));
    QCOMPARE(bim.name, QStringLiteral("bim"));
    QCOMPARE(bim.displayName, QStringLiteral("Bim"));
    QVERIFY(bim.isDark);
    QCOMPARE(bim.foreground, QColor(255, 213, 0));
    QCOMPARE(bim.background, QColor(1, 40, 73));

    auto light = ThemeLoader::findTheme(themes, QStringLiteral("one-light"));
    QVERIFY(!light.isDark);

    auto fallback = ThemeLoader::findTheme(themes, QStringLiteral("nonexistent"));
    QCOMPARE(fallback.name, QStringLiteral("dark"));
}

void TestMainWindow::testThemeSettingDefaultIsSystem() {
    auto *settings = AppSettings::instance();
    QCOMPARE(settings->theme(), QStringLiteral("system"));
}

void TestMainWindow::testThemeChangeAppliesToAllTerminals() {
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *settings = AppSettings::instance();
    QSignalSpy spy(settings, &AppSettings::themeChanged);
    QVERIFY(spy.isValid());

    settings->setTheme(QStringLiteral("bim"));
    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(settings->theme(), QStringLiteral("bim"));

    auto *terminal = currentTerminal(window);
    QVERIFY(terminal);
    QTRY_COMPARE(terminal->debugAppliedForeground(), QColor(255, 213, 0));
    QTRY_COMPARE(terminal->debugAppliedBackground(), QColor(1, 40, 73));

    settings->setTheme(QStringLiteral("system"));
    QTRY_COMPARE(settings->theme(), QStringLiteral("system"));
}
```

- [ ] **Step 3: Run all tests**

Run: `cmake -B build -DBUILD_TESTING=ON && cmake --build build && cd build && ctest --output-on-failure`
Expected: All tests pass (except the 2 pre-existing grid sizing failures in TerminalWidget tests).

- [ ] **Step 4: Run clang-format**

Run: `clang-format -i $(find src tests -name '*.cpp' -o -name '*.h')`

- [ ] **Step 5: Verify clang-format passes**

Run: `clang-format --dry-run --Werror $(find src tests -name '*.cpp' -o -name '*.h')`
Expected: No output (all files pass formatting).

- [ ] **Step 6: Commit**

```bash
git add tests/test_terminal_widget.cpp tests/test_main_window.cpp
git commit -m "test: add theme loading, application, and propagation tests"
```

---

## Self-Review

### Spec Coverage

| Spec Requirement | Task |
|---|---|
| `TerminalTheme` struct with name, displayName, isDark, fg, bg, cursor, ansi[16] | Task 1 |
| Themes loaded from JSON resources via `loadThemes()` | Task 3 |
| `findTheme()` helper | Task 3 |
| 7 built-in themes migrated from original .colorscheme files | Task 2 |
| Special `"system"` setting | Task 3 (default), Task 5 (resolve) |
| `AppSettings` theme key (combobox, default "system") | Task 3 |
| `themeChanged` signal | Task 3 |
| `TerminalWidget::applyTheme()` with Ghostty color API | Task 4 |
| `effectColorScheme` callback returns true and reports isDark | Task 4 |
| System dark/light resolution via DTK | Task 5 |
| `setPaletteType` for window chrome | Task 5 |
| Theme change propagation to all terminals | Task 5 |
| Settings dialog combobox | Task 3 (populated at runtime) |
| Context menu Theme submenu | Task 6 |
| Sidebar `setDarkMode(bool)` | Task 7 |
| Tests for theme loading and application | Task 8 |

### Placeholder Scan

No TBD, TODO, or "implement later" patterns found. All code blocks contain complete implementations.

### Type Consistency

- `TerminalTheme` struct fields match across all files (name: QString, displayName: QString, isDark: bool, foreground/background/cursor: QColor, ansi: QColor[16])
- `applyTheme(const TerminalTheme &theme)` signature matches between header and implementation
- `setDarkMode(bool dark)` matches between header and implementation
- `themeChanged(const QString &theme)` signal matches AppSettings declaration and MainWindow connection
- `loadThemes()` returns `QList<TerminalTheme>` consistently between ThemeLoader.h/.cpp and all callers
- `findTheme(const QList<TerminalTheme> &, const QString &)` signature matches between header and implementation
