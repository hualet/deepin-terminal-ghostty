# Theme-Blended Pane Dividers Implementation Plan

> **For agentic workers:** Execute this plan task-by-task with test-first checkpoints.

**Goal:** Make split-pane dividers blend with the active terminal theme instead of compositing a translucent tint over the splitter's unrelated palette background.

**Architecture:** Build an opaque divider color by mixing the relevant background and foreground colors at a small fixed ratio. Pane splitters use terminal-theme colors, while the vertical-tab/sidebar splitter uses DTK application-palette colors. Existing splitters cache and refresh the resulting style whenever the theme changes.

**Tech Stack:** C++20, Qt6 Widgets, DTK6, Qt Test

---

### Task 1: Add the rendering regression

**Files:**
- Modify: `tests/test_main_window.cpp`

- [ ] Replace the string-only divider test with a test that requests a style from background and foreground colors.
- [ ] Render that style on a `QSplitter` whose own palette background intentionally differs from the requested theme.
- [ ] Assert that the handle pixel equals the pre-blended opaque theme color and is independent of the splitter background.
- [ ] Build `test_main_window` and run only `testPaneDividerColorsFollowTheme`; expect the new API or pixel assertion to fail before production changes.

### Task 2: Pre-blend pane and sidebar divider colors

**Files:**
- Modify: `src/app/TermPane.h`
- Modify: `src/app/TermPane.cpp`
- Modify: `src/app/MainWindow.cpp`

- [ ] Change the splitter-style helper to accept background and foreground colors and emit an opaque final color.
- [ ] Cache the current pane-divider style in `TermPane` so splitters created after a theme change receive the same style.
- [ ] Refresh pane splitters with `TerminalTheme::background` and `TerminalTheme::foreground`.
- [ ] Style the vertical sidebar splitter separately from `DGuiApplicationHelper::applicationPalette()` window and window-text roles.
- [ ] Re-run the focused test and expect it to pass.

### Task 3: Verify and document

**Files:**
- Create: `docs/root-cause/2026-08-23-pane-divider-theme-blending.md`

- [ ] Format all changed C++ files with the repository `.clang-format` configuration.
- [ ] Run the focused `test_main_window` test under `QT_QPA_PLATFORM=offscreen`.
- [ ] Run the full offscreen CTest suite using a writable test home and report known environmental failures separately.
- [ ] Record the visual failure mechanism, fix, and verification evidence in the root-cause report.
- [ ] Leave the changes uncommitted unless the user explicitly asks for a commit.
