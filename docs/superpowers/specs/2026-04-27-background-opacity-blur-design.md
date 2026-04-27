# Background Opacity and Blur Design

## Goal

Add background transparency (opacity slider) and background blur (checkbox) to
deepin-terminal-ghostty, matching the deepin-terminal user experience. Includes
compositor capability detection and keyboard shortcuts for opacity adjustment.

## Reference

deepin-terminal at `~/projects/deepin/deepin-terminal` implements the same
features. This design follows its patterns, adapted to the Ghostty VT render
pipeline.

## Scope

- Background opacity: slider 20–100%, stored as int, applied as qreal 0.2–1.0
- Background blur: on/off toggle
- Compositor capability detection: hide/show settings, force opacity=1.0 when unsupported
- Keyboard shortcuts: Ctrl+Shift+Up/Down to adjust opacity by 5% steps

## Architecture

### Layer Boundaries (per qtghostty/app split)

`qtghostty` layer:
- `TerminalWidget::setOpacity(qreal)` — paints background with alpha, sets `WA_TranslucentBackground`
- No knowledge of settings, blur, or compositor

`app` layer:
- `AppSettings` — stores opacity/blur values, emits change signals
- `MainWindow` — toggles blur via `DMainWindow::setEnableBlurWindow()`, routes opacity to terminals
- `TermPane` — propagates opacity to its `TerminalWidget`
- Compositor detection via `DWindowManagerHelper`
- Keyboard shortcut handling
- Settings dialog visibility control

## Component Design

### 1. Settings

**File:** `src/app/settings/default-config.json`

Add to `basic.interface` group:

```json
{
  "key": "opacity",
  "type": "slider",
  "default": 100,
  "range": [20, 100]
}
```

```json
{
  "key": "blurred_background",
  "type": "checkbox",
  "default": false
}
```

**File:** `src/app/AppSettings.h/.cpp`

New public API:

```cpp
qreal opacity() const;          // returns 0.2–1.0
bool backgroundBlur() const;    // returns blurred_background value
void setOpacity(qreal opacity); // for keyboard shortcuts, updates DSettings
```

New signals:

```cpp
void opacityChanged(qreal opacity);
void backgroundBlurChanged(bool enabled);
```

Implementation notes:
- `opacity()` reads `basic.interface.opacity` (int 20–100) and divides by 100.0
- `setOpacity()` converts qreal back to int, clamps to [20, 100], calls `DSettingsOption::setValue()`
- Connect `DSettingsOption::valueChanged` for each option to emit typed signals
- Storage uses existing `DSettings` + `QSettingBackend` → `~/.config/deepin/deepin-terminal-ghostty.conf`

### 2. TerminalWidget Opacity Rendering

**File:** `src/libqtghostty/TerminalWidget.h`

Add:

```cpp
public:
  void setOpacity(qreal opacity);

private:
  qreal m_opacity = 1.0;
```

**File:** `src/libqtghostty/TerminalWidget.cpp`

`setOpacity(qreal)`:
- Stores `m_opacity`
- If `m_opacity < 1.0`, sets `WA_TranslucentBackground` on self
- If `m_opacity >= 1.0`, removes `WA_TranslucentBackground`
- Calls `update()` to trigger repaint

`renderTerminal()` modifications (3 fill sites):

1. **Line ~544** — widget background fill (outside content area):
   ```cpp
   QColor bg(colors.background.r, colors.background.g, colors.background.b,
             qRound(m_opacity * 255));
   painter.fillRect(rect(), bg);
   ```

2. **Line ~568** — back-buffer full redraw fill (content area):
   ```cpp
   QColor bgAlpha(colors.background.r, colors.background.g, colors.background.b,
                  qRound(m_opacity * 255));
   backPainter.fillRect(QRect(QPoint(0, 0), contentRect.size()), bgAlpha);
   ```

3. **Line ~608** — `renderRow()` default background:
   Same alpha-modified background color for row fill.

All foreground text, cursor, and selection rendering remains fully opaque.

### 3. MainWindow Blur and Compositor Detection

**File:** `src/app/MainWindow.h`

Add members:

```cpp
bool m_compositorHasBlur = false;
void initWindowEffects();
void applyOpacityToAll();
void setWindowBlurEnabled(bool enabled);
void onCompositorCapabilitiesChanged();
```

**File:** `src/app/MainWindow.cpp`

`initWindowEffects()` (called from constructor):
- Check `DWindowManagerHelper::instance()->hasBlurWindow()` → store in `m_compositorHasBlur`
- Connect `DWindowManagerHelper::hasBlurWindowChanged` → `onCompositorCapabilitiesChanged()`
- Connect `AppSettings::opacityChanged` → apply opacity to all terminal panes
- Connect `AppSettings::backgroundBlurChanged` → `setWindowBlurEnabled()`
- Apply initial opacity and blur from settings

`setWindowBlurEnabled(bool)`:
- If enabled and compositor supports blur: call `setEnableBlurWindow(true)` and ensure `WA_TranslucentBackground` is set on the window
- If disabled: call `setEnableBlurWindow(false)`, remove `WA_TranslucentBackground` if opacity is also 1.0

`onCompositorCapabilitiesChanged()`:
- Update `m_compositorHasBlur`
- If compositor lost blur support: disable blur, force opacity to 1.0 on all terminals
- Show/hide settings dialog options accordingly
- If compositor gained blur support: restore opacity from settings

`applyOpacityToAll()`:
- Iterate all tabs, find each `TermPane`, call `TermPane::setOpacity(qreal)`

### 4. TermPane Propagation

**File:** `src/app/TermPane.h/.cpp`

Add:

```cpp
void setOpacity(qreal opacity);
```

Implementation: calls `m_terminalWidget->setOpacity(opacity)`.

### 5. Compositor-Aware Settings Visibility

**File:** `src/app/SettingsDialog.cpp` or `src/app/AppSettings.cpp`

When compositor does not support blur:
- Hide the opacity slider and blur checkbox from the settings dialog
- This is done by accessing the `DSettingsOption` and setting its visibility

When compositor support changes, update visibility accordingly.

### 6. Keyboard Shortcuts

**File:** `src/app/MainWindow.cpp` (in `keyPressEvent` or a dedicated handler)

- **Ctrl+Shift+Up**: increase opacity by 5% (capped at 100%)
- **Ctrl+Shift+Down**: decrease opacity by 5% (floor at 20%)

Both call `AppSettings::instance()->setOpacity(newOpacity)` which updates the
DSettings value and propagates through the signal chain, keeping the settings
dialog slider in sync.

Only active when compositor supports blur (same guard as deepin-terminal).

## Signal Flow

```
Settings Dialog Slider / Checkbox
  → DSettingsOption::setValue()
    → QSettingBackend persists to disk
    → DSettingsOption::valueChanged signal
      → AppSettings::opacityChanged(qreal) / backgroundBlurChanged(bool)
        → MainWindow::applyOpacityToAll() / setWindowBlurEnabled()
          → TermPane::setOpacity() → TerminalWidget::setOpacity()
          → DMainWindow::setEnableBlurWindow()

Keyboard Shortcut (Ctrl+Shift+Up/Down)
  → MainWindow key handler
    → AppSettings::setOpacity(qreal)
      → DSettingsOption::setValue()
        → (same signal chain as above)

Compositor State Change
  → DWindowManagerHelper::hasBlurWindowChanged
    → MainWindow::onCompositorCapabilitiesChanged()
      → Force opacity=1.0 if unsupported
      → Show/hide settings dialog options
```

## Files Modified

| File | Changes |
|------|---------|
| `src/app/settings/default-config.json` | Add opacity slider and blur checkbox definitions |
| `src/app/AppSettings.h` | Add opacity(), backgroundBlur(), setOpacity(), signals |
| `src/app/AppSettings.cpp` | Implement new settings accessors and signal wiring |
| `src/libqtghostty/TerminalWidget.h` | Add setOpacity(), m_opacity member |
| `src/libqtghostty/TerminalWidget.cpp` | Modify 3 fillRect sites; setOpacity() implementation |
| `src/app/MainWindow.h` | Add window effects members and methods |
| `src/app/MainWindow.cpp` | Blur, compositor detection, opacity propagation, shortcuts |
| `src/app/TermPane.h` | Add setOpacity() |
| `src/app/TermPane.cpp` | Implement setOpacity() forwarding |
| `src/app/SettingsDialog.cpp` | Dynamic visibility for opacity/blur options |

## Testing

- Manual verification: adjust opacity slider, confirm background changes while text stays opaque
- Manual verification: toggle blur checkbox, confirm compositor blur effect appears/disappears
- Manual verification: Ctrl+Shift+Up/Down adjusts opacity in 5% steps
- Manual verification: on a compositor without blur support, settings are hidden and opacity forced to 1.0
- Existing automated tests should continue passing (no behavior change when opacity=1.0, blur=false)
