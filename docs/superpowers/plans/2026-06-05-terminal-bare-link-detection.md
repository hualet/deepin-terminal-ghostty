# Terminal Bare Link Detection Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add performant bare protocol link detection for `http`, `https`, `ssh`, `mailto`, and `file` links while preserving OSC 8 hyperlink semantics.

**Architecture:** First refactor OSC 8 hover underlining to store and paint a single hover range. Then add explicit combined `link*` APIs that prefer OSC 8 and fall back to lazy per-row bare link scanning with a bounded cache.

**Tech Stack:** C++20, Qt6 Widgets, Ghostty VT C API, Qt Test.

---

### Task 1: OSC 8 Range Hover Refactor

**Files:**
- Modify: `src/libqtghostty/TerminalWidget.h`
- Modify: `src/libqtghostty/TerminalWidget.cpp`
- Test: `tests/test_terminal_widget.cpp`

- [ ] Write/keep focused OSC 8 hover underline tests.
- [ ] Verify existing OSC 8 tests pass before refactor.
- [ ] Add `LinkRange` hover state and replace full-grid overlay scan with one-range drawing.
- [ ] Verify OSC 8 tests pass after refactor.

### Task 2: Combined Link API And Bare Scanner

**Files:**
- Modify: `src/libqtghostty/TerminalWidget.h`
- Modify: `src/libqtghostty/TerminalWidget.cpp`
- Modify: `src/app/TermPane.cpp`
- Test: `tests/test_terminal_widget.cpp`

- [ ] Add failing tests for `linkUriAtPosition()` bare link detection and `hyperlinkUriAtPosition()` remaining OSC 8-only.
- [ ] Add failing tests for Ctrl-click activation of bare links.
- [ ] Implement `linkUriAtPosition()`, `linkHovered()`, `linkActivated()`, and a lazy row scanner.
- [ ] Migrate `TermPane` context menu and activation wiring to combined link APIs.
- [ ] Verify focused terminal widget tests pass.

### Task 3: Edge Cases And Performance Guards

**Files:**
- Modify: `src/libqtghostty/TerminalWidget.h`
- Modify: `src/libqtghostty/TerminalWidget.cpp`
- Test: `tests/test_terminal_widget.cpp`

- [ ] Add tests for all five schemes.
- [ ] Add tests for punctuation trimming, backticks, balanced parentheses, `mailto`, and `file:///`.
- [ ] Add tests for cache reuse, no paint-triggered scans, scroll hover invalidation, and selection independence.
- [ ] Implement the bounded LRU cache and invalidation hooks.
- [ ] Run `clang-format`, focused tests, and the relevant `ctest` suite.
