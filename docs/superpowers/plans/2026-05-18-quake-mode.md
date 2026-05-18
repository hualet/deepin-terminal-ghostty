# Quake Mode Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a dedicated quake-mode top drop-down terminal window selected by `--quake-mode`.

**Architecture:** Add `QuakeWindow` as a subclass of `MainWindow` and keep terminal/tab/session behavior in `MainWindow`. Extend startup parsing and app settings with the narrow options needed for quake presentation.

**Tech Stack:** C++20, Qt6 Widgets, DTK6, Qt Test, CMake.

---

### Task 1: Startup Option

**Files:**
- Modify: `src/app/StartupOptions.h`
- Modify: `src/app/StartupOptions.cpp`
- Test: `tests/test_startup_options.cpp`

- [ ] **Step 1: Write failing tests for `--quake-mode`**

Add a test that parses `deepin-terminal-ghostty --quake-mode` and expects
`options.quakeMode == true`. Keep existing tests unchanged.

- [ ] **Step 2: Run startup option tests and verify failure**

Run: `cmake --build build --target test_startup_options && ./build/tests/test_startup_options`

Expected: compile failure because `StartupOptions::quakeMode` does not exist.

- [ ] **Step 3: Add the option**

Add `bool quakeMode = false;` to `StartupOptions`, add a `QCommandLineOption`
named `quake-mode`, register it, and set `options.quakeMode = parser.isSet(quakeModeOption);`.

- [ ] **Step 4: Re-run startup option tests**

Run: `cmake --build build --target test_startup_options && ./build/tests/test_startup_options`

Expected: all startup option tests pass.

### Task 2: Quake Setting

**Files:**
- Modify: `src/app/settings/default-config.json`
- Modify: `src/app/AppSettings.h`
- Modify: `src/app/AppSettings.cpp`
- Test: `tests/test_appsettings.cpp`

- [ ] **Step 1: Write failing setting tests**

Add assertions that `hideQuakeOnFocusLoss()` defaults to true and that the
default config contains `advanced.window.hideQuakeOnFocusLoss`.

- [ ] **Step 2: Run app settings tests and verify failure**

Run: `cmake --build build --target test_appsettings && ./build/tests/test_appsettings`

Expected: compile failure because `hideQuakeOnFocusLoss()` does not exist.

- [ ] **Step 3: Add setting API and JSON option**

Add an `advanced.window` group to the JSON. Add
`AppSettings::hideQuakeOnFocusLoss() const` reading
`advanced.window.hideQuakeOnFocusLoss`.

- [ ] **Step 4: Re-run app settings tests**

Run: `cmake --build build --target test_appsettings && ./build/tests/test_appsettings`

Expected: all app settings tests pass.

### Task 3: QuakeWindow Class

**Files:**
- Create: `src/app/QuakeWindow.h`
- Create: `src/app/QuakeWindow.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Test: `tests/test_main_window.cpp`

- [ ] **Step 1: Write failing quake window tests**

Add tests that construct `QuakeWindow`, verify `isQuakeMode()`, verify
`Qt::WindowStaysOnTopHint`, verify the titlebar is hidden, and verify target
geometry uses the active screen width and 40% height.

- [ ] **Step 2: Run main window tests and verify failure**

Run: `cmake --build build --target test_main_window && QT_QPA_PLATFORM=offscreen ./build/tests/test_main_window`

Expected: compile failure because `QuakeWindow` does not exist.

- [ ] **Step 3: Add `QuakeWindow`**

Create a subclass with:

- constructor forwarding `StartupOptions` to `MainWindow`
- `bool isQuakeMode() const`
- `QRect targetGeometry() const`
- titlebar hidden in constructor
- top-of-screen geometry from `QGuiApplication::screenAt(QCursor::pos())` or primary screen
- `Qt::WindowStaysOnTopHint`

- [ ] **Step 4: Wire build files**

Add `src/app/QuakeWindow.cpp` to app sources and `test_main_window` sources.

- [ ] **Step 5: Re-run main window tests**

Run: `cmake --build build --target test_main_window && QT_QPA_PLATFORM=offscreen ./build/tests/test_main_window`

Expected: tests pass or expose missing virtual hooks.

### Task 4: Animation And Focus-Loss Hide

**Files:**
- Modify: `src/app/QuakeWindow.h`
- Modify: `src/app/QuakeWindow.cpp`
- Test: `tests/test_main_window.cpp`

- [ ] **Step 1: Write failing behavior tests**

Add tests for `showQuake()`, `hideQuake()`, and focus-loss hide when
`hideQuakeOnFocusLoss` is true. Use a zero-duration test hook if needed.

- [ ] **Step 2: Run main window tests and verify failure**

Run: `cmake --build build --target test_main_window && QT_QPA_PLATFORM=offscreen ./build/tests/test_main_window`

Expected: compile failure or test failure because the methods do not exist.

- [ ] **Step 3: Implement animation helpers**

Use `QPropertyAnimation` on `geometry`. Add test-only animation duration control
under `QTGHOSTTY_TESTING`. `showQuake()` shows, raises, activates, and animates
to target geometry. `hideQuake()` animates to height 0 then hides.

- [ ] **Step 4: Implement focus-loss handling**

Override `changeEvent`. On `QEvent::ActivationChange`, if the window is visible,
inactive, the setting is enabled, and there is no active child modal/dialog,
call `hideQuake()`.

- [ ] **Step 5: Re-run main window tests**

Run: `cmake --build build --target test_main_window && QT_QPA_PLATFORM=offscreen ./build/tests/test_main_window`

Expected: main window tests pass.

### Task 5: Main Entry

**Files:**
- Modify: `src/app/main.cpp`
- Test: `tests/test_startup_options.cpp`

- [ ] **Step 1: Build app after wiring**

Run: `cmake --build build --target deepin-terminal-ghostty`

Expected: compile failure if `main.cpp` has not included `QuakeWindow`, or success after wiring.

- [ ] **Step 2: Create quake or normal window by startup option**

Use a `std::unique_ptr<MainWindow>` or stack branch to create `QuakeWindow` when
`startupOptions.quakeMode` is true and `MainWindow` otherwise. Keep startup
exit-code signal wiring unchanged.

- [ ] **Step 3: Rebuild app**

Run: `cmake --build build --target deepin-terminal-ghostty`

Expected: build succeeds.

### Task 6: Final Verification

**Files:**
- All changed files

- [ ] **Step 1: Format C++**

Run: `clang-format -i $(find src tests -name '*.cpp' -o -name '*.h')`

- [ ] **Step 2: Run focused tests**

Run:
`cmake --build build --target test_startup_options test_appsettings test_main_window`

Run:
`QT_QPA_PLATFORM=offscreen ./build/tests/test_startup_options && ./build/tests/test_appsettings && QT_QPA_PLATFORM=offscreen ./build/tests/test_main_window`

- [ ] **Step 3: Run format check**

Run:
`clang-format --dry-run --Werror $(find src tests -name '*.cpp' -o -name '*.h')`

- [ ] **Step 4: Review diff**

Run: `git status --short && git diff --stat`
