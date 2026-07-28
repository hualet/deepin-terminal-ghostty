# libghostty-vt Upgrade Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use
> checkboxes for tracking.

**Goal:** Atomically upgrade the vendored Ghostty VT library, headers, and
affected terminal-construction call site to upstream `2dd79f3bc`.

**Architecture:** Keep `qtghostty` behavior unchanged while adapting to the
new constructor ABI. Continue deriving a byte budget from the line-oriented
application setting and pass it through the new explicit scrollback option.

**Tech Stack:** C++20, Qt6 Widgets, CMake, Qt Test, Zig 0.16,
`libghostty-vt`

---

### Task 1: Stage the pinned upstream artifacts

**Files:**

- Replace: `lib/libghostty-vt.so`
- Modify: `lib/include/ghostty/vt.h`
- Modify: `lib/include/ghostty/vt/*.h`
- Create: `lib/include/ghostty/vt/color_scheme.h`
- Create: `lib/include/ghostty/vt/unicode.h`

- [x] Copy the verified ReleaseFast baseline-CPU build for
  `2dd79f3bc6af649e68422b08e21ad0300fd8b391` into
  `lib/libghostty-vt.so`.
- [x] Synchronize the matching upstream `zig-out/include/` tree into
  `lib/include/`.
- [x] Query `ghostty_build_info()` and verify the pinned version, optimization,
  SIMD, Kitty graphics, and tmux-control values.
- [x] Compare exported symbols and confirm the SONAME remains
  `libghostty-vt.so.0`.

### Task 2: Verify the compatibility regression

**Files:**

- Verify: `src/libqtghostty/TerminalWidget.cpp`
- Verify: `tests/test_terminal_widget.cpp`

- [x] Configure a clean build with testing enabled.
- [x] Build `test_terminal_widget`.
- [x] Confirm compilation fails at `TerminalWidget::setupTerminal()` because
  `GhosttyTerminalOptions` was removed by the new headers.
- [x] Confirm the existing scrollback test still specifies a 100 MB default
  budget and a 400 MB budget after selecting 20,000 lines.

### Task 3: Migrate terminal construction

**Files:**

- Modify: `src/libqtghostty/TerminalWidget.cpp`

- [x] Replace the removed options-struct constructor call with:

```cpp
GhosttyResult err =
    ghostty_terminal_new(nullptr, &m_terminal, m_cols, m_rows);
```

- [x] After successful construction, preserve the existing budget with:

```cpp
size_t scrollbackMaxBytes = scrollbackByteBudget();
ghostty_terminal_set(m_terminal,
                     GHOSTTY_TERMINAL_OPT_SCROLLBACK_MAX_BYTES,
                     &scrollbackMaxBytes);
```

- [x] Build and run the focused scrollback test.
- [x] Run the complete `TerminalWidget` test binary offscreen.

### Task 4: Final verification

**Files:**

- Verify: all scoped files

- [x] Format `src/libqtghostty/TerminalWidget.cpp`.
- [x] Run the complete offscreen CTest suite and distinguish any pre-existing
  UI timeout from Ghostty-related regressions.
- [x] Run `clang-format --dry-run --Werror` for the changed C++ file.
- [x] Run `git diff --check`, inspect `git diff --stat`, and verify that no
  unrelated files changed.
- [x] Do not create a commit; repository instructions require a separate
  explicit request.
