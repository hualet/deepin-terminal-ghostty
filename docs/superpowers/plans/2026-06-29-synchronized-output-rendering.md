# Synchronized Output Rendering Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Prevent incomplete TUI frames from becoming visible while DEC synchronized output mode is active.

**Architecture:** Continue feeding every PTY chunk into `libghostty-vt`, then query `GHOSTTY_MODE_SYNC_OUTPUT` before publishing a new render state. Keep drawing the existing back buffer while synchronization is active, publish the completed frame when the mode closes, and use a one-second timer to release a frame if an application leaves the mode stuck.

**Tech Stack:** C++20, Qt6 Widgets/QtTest, `libghostty-vt` C API

---

### Task 1: Add failing synchronized-output rendering coverage

**Files:**
- Modify: `tests/test_terminal_widget.cpp`
- Modify: `src/libqtghostty/TerminalWidget.h`
- Modify: `src/libqtghostty/TerminalWidget.cpp`

- [x] Add test-only render-state update accounting.
- [x] Add a test that feeds `CSI ? 2026 h` plus a partial frame, forces a paint, and verifies that the render state is not published until `CSI ? 2026 l` arrives.
- [x] Run `QT_QPA_PLATFORM=offscreen ./build/tests/test_terminal_widget testSynchronizedOutputDefersIncompleteFrame` and confirm it fails because the partial frame is published.

### Task 2: Honor synchronized output and add timeout recovery

**Files:**
- Modify: `src/libqtghostty/TerminalWidget.h`
- Modify: `src/libqtghostty/TerminalWidget.cpp`
- Modify: `tests/test_terminal_widget.cpp`

- [x] Query `ghostty_terminal_mode_get(..., GHOSTTY_MODE_SYNC_OUTPUT, ...)` before updating the render state.
- [x] Keep painting the last completed back buffer while the mode is active.
- [x] Start a single-shot one-second timer when rendering is deferred; on timeout, clear the mode through `ghostty_terminal_mode_set` and request one repaint.
- [x] Stop the timeout when a normal completed frame is published.
- [x] Add a focused test proving a stuck synchronized-output sequence becomes visible after timeout.
- [x] Run both synchronized-output tests and confirm they pass.

### Task 3: Document and verify

**Files:**
- Create: `docs/root-cause/2026-06-29-synchronized-output-frame-jump.md`

- [x] Record the trace evidence, renderer mismatch, fix, and verification commands.
- [x] Format changed C++ files with the repository `.clang-format` configuration.
- [x] Build the application and run the focused `test_terminal_widget` binary offscreen.
- [x] Launch the traced application for manual verification without creating a commit.
