# Theme Support Design

## Goal

Add terminal color scheme support with system dark/light integration. Minimum viable: a few built-in themes selectable from the settings dialog and the context menu.

## Theme Data Model

A theme is a flat struct holding the colors that matter to a terminal:

```cpp
struct TerminalTheme {
    QString name;        // internal key: "dark", "bim", "ocean-dark"
    QString displayName; // shown in UI: "Dark", "Bim", "Ocean Dark"
    bool isDark;         // drives DTK window chrome (DarkType vs LightType)
    QColor foreground;
    QColor background;
    QColor cursor;
    QColor ansi[16];     // ANSI 0-7 normal, 8-15 bright
};
```

Themes are loaded from JSON files embedded in Qt resources. A `loadThemes()` function reads all `:/themes/*.json` at startup and returns a `QList<TerminalTheme>`. A `findTheme(name)` helper looks up by name.

## Theme File Format

Each JSON file under `src/app/themes/` contains one theme:

```json
{
    "name": "dark",
    "displayName": "Dark",
    "isDark": true,
    "foreground": [0, 205, 0],
    "background": [37, 37, 37],
    "cursor": [0, 205, 0],
    "ansi": [
        [0,0,0], [178,24,24], [24,178,24], [178,103,24],
        [24,24,178], [178,24,178], [24,178,178], [178,178,178],
        [104,104,104], [255,55,55], [55,255,55], [255,255,55],
        [55,55,255], [255,55,255], [55,255,255], [255,255,255]
    ]
}
```

Arrays are `[R, G, B]` integers 0-255.

## Built-in Themes

Migrated from original deepin-terminal `.colorscheme` files:

| name | displayName | isDark | Source |
|---|---|---|---|
| dark | Dark | true | Dark.colorscheme |
| light | Light | false | Light.colorscheme |
| bim | Bim | true | Theme4.colorscheme |
| tomorrow-night-blue | Tomorrow Night Blue | true | Theme3.colorscheme |
| ocean-dark | Ocean Dark | true | Theme7.colorscheme |
| hybrid | Hybrid | true | Theme6.colorscheme |
| one-light | One Light | false | Theme10.colorscheme |

Plus the special `"system"` setting that follows DTK's detected theme type, mapping to `dark` or `light`.

## Settings

`AppSettings` gains a `theme` key (type: combobox, default: `"system"`). Options are populated dynamically at runtime from the loaded theme list, with `"system"` prepended.

A `themeChanged(const QString &themeName)` signal is emitted when the setting changes.

## Applying Colors to TerminalWidget

`TerminalWidget` gains `applyTheme(const TerminalTheme &theme)`:

1. Convert `QColor` to `GhosttyColorRgb` helper.
2. Call `ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_COLOR_FOREGROUND, &rgb)`.
3. Same for `GHOSTTY_TERMINAL_OPT_COLOR_BACKGROUND` and `GHOSTTY_TERMINAL_OPT_COLOR_CURSOR`.
4. Build a `GhosttyColorRgb[256]` array: fill slots 0-15 from `ansi[]`, leave 16-255 as built-in defaults (read current palette with `ghostty_terminal_get`, then overwrite 0-15).
5. Call `ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_COLOR_PALETTE, &palette)`.
6. Set `m_renderStateDirty = true; update();`.

`effectColorScheme` callback is updated to return `true` and set `out_scheme` based on the current theme's `isDark` field (stored as a member on `TerminalWidget`).

## System Dark/Light Integration

### Resolving the Active Theme

A resolve function determines which `TerminalTheme` to use:

- If setting is `"system"`: read `DGuiApplicationHelper::instance()->themeType()`, return `dark` theme for `DarkType`, `light` theme for `LightType`.
- Otherwise: return the named theme.

### Theme Change Propagation

`MainWindow`:

1. On startup, resolve theme and call `applyTheme` on every `TerminalWidget`.
2. Connect `DGuiApplicationHelper::themeTypeChanged` → re-resolve and re-apply.
3. Connect `AppSettings::themeChanged` → re-resolve and re-apply.
4. When re-applying: also call `DGuiApplicationHelper::instance()->setPaletteType(theme.isDark ? DarkType : LightType)` unless the setting is `"system"` (in which case set `UnknownType` to let DTK follow the OS).

### Window Chrome

When a specific (non-system) theme is selected, `setPaletteType` is called with the theme's `isDark` value so the DTK window frame (title bar, menus, sidebar) matches the terminal content. When "system" is selected, `UnknownType` lets DTK follow the OS preference.

## UI Entry Points

### Settings Dialog

Add a `theme` combobox to `default-config.json` in the `basic.interface` group. The combobox options are populated at runtime by `AppSettings` when the dialog is created.

### Context Menu

`TermPane::showTerminalContextMenu` adds a "Theme" submenu listing all themes plus "System". Selecting an item updates the setting, which triggers the propagation chain.

## Sidebar Adaptation

`VerticalTabSidebar` currently uses hardcoded dark colors. When the theme changes, the sidebar should update its stylesheet. The simplest approach: expose a `setDarkMode(bool)` method that switches between two hardcoded stylesheets (dark/light). `MainWindow` calls this alongside `applyTheme`.

## File Changes

| File | Change |
|---|---|
| `src/app/themes/*.json` | 7 theme JSON files (new) |
| `src/app/TerminalTheme.h` | `TerminalTheme` struct, load/find functions (new) |
| `src/app/settings/resources.qrc` | Register theme JSON files |
| `src/app/settings/default-config.json` | Add `theme` combobox key |
| `src/app/AppSettings.h/.cpp` | Add `theme` setting access, `themeChanged` signal, populate combo options |
| `src/libqtghostty/TerminalWidget.h/.cpp` | Add `applyTheme()`, store current theme, fix `effectColorScheme` |
| `src/app/MainWindow.h/.cpp` | Theme resolution, DTK integration, propagation to all widgets |
| `src/app/TermPane.cpp` | Theme submenu in context menu |
| `src/app/VerticalTabSidebar.h/.cpp` | `setDarkMode(bool)` for stylesheet switching |
| `tests/test_main_window.cpp` | Test theme application and DTK sync |
| `tests/test_terminal_widget.cpp` | Test `applyTheme` sets Ghostty colors |
