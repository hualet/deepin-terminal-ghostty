# Workspace overview implementation plan

**Goal:** Add a Ghostty-style tab overview accessible through the main menu
and configurable shortcut, following the user's revised entry design.

**Architecture:** An app-owned `WorkspaceOverview` overlay displays entries
keyed by stable tab IDs. `MainWindow` captures existing tab widgets without
resizing them, supplies entries, and handles existing tab actions. Terminal
session ownership remains unchanged.

**Tech stack:** C++20, Qt6 Widgets, DTK6, existing QTest and CMake targets.

Execute inline in `.worktrees/workspace-overview`; return verified changes
to the original checkout without creating a commit.

## 1. Integration tests before implementation

- [x] Add QTest cases in `tests/test_main_window.cpp` that verify the absence
  of overview navigation buttons in both tab modes, enter through the menu
  and shortcut, activate cards, and restore terminal focus with Escape.
- [x] Add cases for search, keyboard selection, new and close buttons,
  reorder/removal safety, external mode changes, and preview preservation
  of terminal geometry and identity.
- [x] Configure with `cmake -S . -B build -DBUILD_TESTING=ON`, build
  `test_main_window`, and run the added methods with
  `QT_QPA_PLATFORM=offscreen ./build/tests/test_main_window <method>`.
  Observe failing entry assertions before updating implementation.

## 2. App overview component

- [x] Create `src/app/WorkspaceOverview.h/.cpp`. Expose an entry struct
  containing `id`, `title`, `preview`, and `current`, a `setEntries` method,
  and `tabActivated(int)`, `tabCloseRequested(int)`, `addTabRequested()`,
  and `dismissRequested()` signals.
- [x] Build a search/header row and scrollable responsive grid. Keep card
  widgets across preview refreshes to preserve keyboard focus. Paint scaled
  previews on focusable card buttons, use a separate close button, and
  navigate matching cards with arrow keys and Enter. Escape dismisses.
- [x] Use palette colors, accessible names, elided titles, visible focus,
  and an empty-search message. Add the component to application,
  translation, and MainWindow test source lists in both CMake files.

## 3. Menu/shortcut entries and lifecycle integration

- [x] Keep `TabBar` and `VerticalTabSidebar` free of overview buttons and
  signals. Use the main-menu action and configurable shortcut for entry.
- [x] Add overlay creation, toggling, ID-based action dispatch, focus
  restoration, host-size tracking, and a visible-only refresh timer to
  `MainWindow`. Populate from existing tab records and widget rendering.
- [x] Add `workspace_overview` to settings and shortcut display, default
  `Ctrl+Shift+O`; provide a main-menu action. Suppress hidden terminal
  action shortcuts while the overlay owns focus where needed.
- [x] Resolve the tab ID again after close confirmation; keep overview
  keyboard focus after asynchronous tab activation/removal. Hide remote
  management on entry. Update the menu action check state on exit.

## 4. Verification and delivery

- [x] Run new focused MainWindow tests, then the full MainWindow binary.
- [x] Build the application and all tests; run
  `QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`.
- [x] Inspect rendered overview images for light/dark palettes and narrow
  layout. Inspect capture evidence for hidden tab output and splits.
- [x] Format changed C++ files and run `git diff --check`.
- [x] Copy only feature changes to the original checkout, rerun its build
  and relevant tests, and report the usage and observed validation.

## Final verification

Changes transferred to the original checkout without a commit. Its full
build and format checks passed. Final command:

```sh
dbus-run-session -- env QT_QPA_PLATFORM=offscreen QT_QPA_PLATFORMTHEME=deepin \
  ctest --test-dir build --output-on-failure
```

All 11 suites passed. The default offscreen environment also passed focused
overview layout, palette inheritance, sidebar access, and Escape tests.
Chinese light/dark/narrow screenshots are under `build/overview-qa/`.

The final menu-only revision removes both navigation entry buttons and their
signals/icon helper. Menu checked-state, default shortcut toggling, custom
shortcuts across windows, and the original add-tab controls are covered by
regression tests. Rebuild, formatting, refreshed Chinese screenshots, and
all 11 CTest suites passed after this revision.

## Navigation revision

- Hide the entire vertical splitter container while overview is visible,
  preserving its child geometry and splitter weights; restore on exit.
- Make the overview fill the whole content host.
- Connect horizontal tab click intent to the same ID-based activation
  method as preview cards. Keep programmatic selection/reorder updates
  independent so session exit or reordering does not dismiss the mode.
- Verify current/different/reordered horizontal clicks, vertical hide/restore
  at normal/narrow sizes, mode changes, and unchanged terminal grid sizes.

Navigation revision verification: all 11 CTest suites passed after a fresh
build; MainWindow reported 119 passing cases with no failures or skips.
Focused tests cover real horizontal tab click signals for current, other,
and reordered tabs, vertical card activation, full-width vertical preview,
sidebar restoration, unchanged PTY dimensions, and saved splitter sizes.

## Launch and focus review fixes

Both findings reproduced in Qt/PTY regression tests before implementation.
Foreground tab creation now dismisses overview, and Tab/Shift+Tab traversal
wraps only through its visible, enabled controls. Coverage checks all three
launch routes in both tab modes, shell input after launching, absence of
PTY writes during preview navigation, and Escape restoration. Targeted
tests, the fresh app build, formatting, and all 11 CTest suites passed.
