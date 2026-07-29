# libghostty-vt P0 Integrations Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use
> checkboxes for tracking.

**Goal:** Integrate precise live scrollback limits, semantic clipboard
writes, and absolute viewport scrolling from the upgraded `libghostty-vt`.

**Architecture:** Keep terminal protocol handling in `qtghostty`.
`TerminalWidget` configures Ghostty directly, translates semantic callbacks
to Qt types, and exposes no application-specific UI behavior.

**Tech Stack:** C++20, Qt6 Widgets, Qt Test, CMake, `libghostty-vt`

---

### Task 1: Apply line and byte scrollback limits at runtime

**Files:**

- Modify: `src/libqtghostty/TerminalWidget.h`
- Modify: `src/libqtghostty/TerminalWidget.cpp`
- Test: `tests/test_terminal_widget.cpp`

- [x] Add `testLiveScrollbackLimitsReachGhostty()`:

```cpp
TerminalWidget widget;
widget.setScrollbackLines(20000);
QVERIFY(widget.initialize());
QCOMPARE(widget.debugConfiguredScrollbackMaxLines(), size_t(20000));
QCOMPARE(widget.debugConfiguredScrollbackMaxBytes(), size_t(400 * 1000 * 1000));

widget.setScrollbackLines(6000);
QCOMPARE(widget.debugConfiguredScrollbackMaxLines(), size_t(6000));
QCOMPARE(widget.debugConfiguredScrollbackMaxBytes(), size_t(120 * 1000 * 1000));
```
- [x] Run the focused test and confirm it fails because the configured line
  limit is unavailable or unchanged.
- [x] Add `debugConfiguredScrollbackMaxLines()`,
  `debugConfiguredScrollbackMaxBytes()`, and
  `applyScrollbackLimits()` backed by Ghostty set/get calls.
- [x] Call the helper during terminal setup and from
  `setScrollbackLines()`.
- [x] Run the focused scrollback tests and confirm they pass.

### Task 2: Replace raw OSC clipboard parsing

**Files:**

- Modify: `src/libqtghostty/TerminalWidget.cpp`
- Test: `tests/test_terminal_widget.cpp`

- [x] Add `testIterm2CopyWritesClipboard()`:

```cpp
CountingTerminalWidget widget;
QVERIFY(widget.initialize());
QGuiApplication::clipboard()->clear();
const QByteArray encoded = QByteArray("copied from iterm2").toBase64();
feedTerminalOutput(widget, QByteArray("\033]1337;Copy=:") + encoded + QByteArray("\a"));
QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("copied from iterm2"));
```
- [x] Run the focused test and confirm it fails because the raw scanner only
  recognizes OSC 52.
- [x] Implement `GhosttyTerminalClipboardWriteFn`, including MIME data,
  location mapping, clearing, and result reporting.
- [x] Register the callback during terminal setup and remove clipboard
  handling from the raw shell-integration scanner.
- [x] Run all clipboard-focused tests and confirm OSC 52, chunked OSC 52,
  ignored reads, and iTerm2 Copy pass.

### Task 3: Use absolute viewport-row scrolling

**Files:**

- Modify: `src/libqtghostty/TerminalWidget.cpp`
- Test: `tests/test_terminal_widget.cpp`

- [x] Extend `testViewportScrollStateAndAbsoluteScroll()` to assert a middle
  offset and `debugLastScrollViewportTag()`:

```cpp
const int middleOffset = bottomState.maximumOffset() / 2;
widget.scrollViewportToOffset(middleOffset);
QCOMPARE(widget.viewportScrollState().offset, middleOffset);
QCOMPARE(widget.debugLastScrollViewportTag(), GHOSTTY_SCROLL_VIEWPORT_ROW);
```

- [x] Run the test and confirm it fails because the current path records
  `GHOSTTY_SCROLL_VIEWPORT_DELTA`.
- [x] Replace the read-plus-delta path with
  `GHOSTTY_SCROLL_VIEWPORT_ROW` and preserve repaint/state publication.
- [x] Run the absolute-scroll and scrollback viewport tests.

### Task 4: Verify the P0 batch

**Files:**

- Verify: `src/libqtghostty/TerminalWidget.h`
- Verify: `src/libqtghostty/TerminalWidget.cpp`
- Verify: `tests/test_terminal_widget.cpp`

- [x] Format changed C++ files with the project `.clang-format`.
- [x] Build the clean upgrade build directory.
- [x] Run the complete `TerminalWidget` test binary offscreen.
- [x] Run `clang-format --dry-run --Werror` on changed C++ files.
- [x] Run `git diff --check` and inspect `git status --short`.

No commit is created unless the user explicitly requests one.
