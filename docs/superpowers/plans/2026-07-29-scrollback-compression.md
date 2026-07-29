# Scrollback Compression Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use
> checkboxes for tracking.

**Goal:** Reclaim cold scrollback memory through Ghostty's incremental
compression API without blocking interactive terminal work.

**Architecture:** A single-shot Qt timer implements Ghostty's 250 ms idle and
1 ms continuation policy. The activity token prevents unrelated repaint work
from repeatedly postponing compression.

**Tech Stack:** C++20, Qt6 Widgets, Qt Test, `libghostty-vt`

---

### Task 1: Schedule compression after terminal activity

**Files:**

- Modify: `src/libqtghostty/TerminalWidget.h`
- Modify: `src/libqtghostty/TerminalWidget.cpp`
- Test: `tests/test_terminal_widget.cpp`

- [x] Add `testScrollbackCompressionWaitsForIdle()` that feeds terminal output,
  expects an active compression timer, verifies zero steps before 250 ms, and
  waits for at least one real incremental step.
- [x] Run the focused test and confirm it fails because no compression
  scheduling API exists.
- [x] Add the single-shot compression timer, activity token, availability
  flag, scheduling helper, and timeout handler.
- [x] Refresh scheduling after PTY writes, VT imports, resize, viewport
  movement, and limit changes.
- [x] Run the focused idle-scheduling test.

### Task 2: Postpone work on renewed activity

**Files:**

- Modify: `src/libqtghostty/TerminalWidget.h`
- Modify: `src/libqtghostty/TerminalWidget.cpp`
- Test: `tests/test_terminal_widget.cpp`

- [x] Add `testScrollbackCompressionActivityRestartsIdleDelay()` that waits
  100 ms after output, writes again, and verifies the timer deadline returns
  close to the 250 ms idle interval.
- [x] Run the test and confirm it fails before activity-token rescheduling is
  implemented.
- [x] Cache and compare `ghostty_terminal_compression_activity()` before
  restarting the timer and before executing each step.
- [x] Run both compression scheduler tests.

### Task 3: Preserve logical terminal contents

**Files:**

- Modify: `src/libqtghostty/TerminalWidget.h`
- Modify: `src/libqtghostty/TerminalWidget.cpp`
- Test: `tests/test_terminal_widget.cpp`

- [x] Add `testScrollbackCompressionPreservesTerminalContent()` that creates
  repetitive history, records exported VT content, drives incremental
  compression to completion, and compares the exported bytes afterward.
- [x] Run the test and confirm it fails because the test driver is absent.
- [x] Add testing-only accessors for the timer, step count, remaining delay,
  and a bounded run-to-completion driver using the same production step.
- [x] Run all compression-focused tests and the complete `TerminalWidget`
  suite.

### Task 4: Verify memory behavior

**Files:**

- Verify: `src/libqtghostty/TerminalWidget.cpp`
- Verify: `tests/test_terminal_widget.cpp`

- [x] Build the complete project.
- [x] Run a large repetitive-history probe and record RSS before and after
  compression plus content identity.
- [x] Run formatting and `git diff --check`.

No commit is created unless the user explicitly requests one.
