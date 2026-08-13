# Npx Spinner Jitter Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reproduce npm/npx's 80 ms Braille loading animation, remove the glyph-size jitter in `TerminalWidget`, and verify the real application render path.

**Architecture:** Keep npm-compatible reproduction outside production code. Preserve Ghostty's cell widths and the existing generic overflow fitter, but fit Unicode Braille Patterns against one shared all-dots reference glyph so every animation frame uses the same font size and baseline.

**Tech Stack:** Python 3 reproduction script, C++20, Qt6 Widgets/offscreen tests, Ghostty VT C API, CMake/CTest.

---

### Task 1: Preserve Braille animation geometry

**Files:**
- Modify: `tests/test_terminal_widget.cpp`
- Modify: `src/libqtghostty/TerminalWidget.cpp`

- [x] **Step 1: Write the failing render regression**

Add `testBrailleSpinnerKeepsStableDotGeometry`, render npm's ten frames with the cursor hidden, and compare the painted area per encoded Braille dot. Require all frames to keep the same native dot scale rather than being independently resized from each frame's ink bounds.

- [x] **Step 2: Run the focused test and verify RED**

Run:

```bash
cmake --build build --target test_terminal_widget
QT_QPA_PLATFORM=offscreen ./build/tests/test_terminal_widget testBrailleSpinnerKeepsStableDotGeometry
```

Expected: FAIL because the current generic non-ASCII overflow fitter derives a different font scale from each Braille frame.

- [x] **Step 3: Add stable Braille reference fitting**

Add an `isBraillePatternCodepoint(QStringView)` range check for U+2800 through U+28FF. When fitting one of these glyphs, derive the scale and centered baseline from U+28FF (all eight dots) while drawing the requested frame. Leave Block Elements, emoji fallback, ASCII, and ordinary overflowing Unicode behavior unchanged.

- [x] **Step 4: Run the focused render tests and verify GREEN**

Run:

```bash
cmake --build build --target test_terminal_widget
QT_QPA_PLATFORM=offscreen ./build/tests/test_terminal_widget \
  testBrailleSpinnerKeepsStableDotGeometry \
  testOverflowingSingleCodepointGlyphFitsCell \
  testKimiBlockLogoKeepsCellFillingGeometry \
  testZoomedAsciiNotReshapedPerCharacter
```

Expected: the new Braille test and adjacent overflow/Block Element tests pass.
Record any pre-existing baseline failure separately.

### Task 2: Keep an exact npm/npx reproduction

**Files:**
- Create: `scripts/npx-spinner-repro.py`

- [x] **Step 1: Add the dependency-free reproduction**

Use npm's ten frames, 80 ms interval, 200 ms initial delay, and exact `CSI 1 G` plus `CSI K` update bytes on stderr. Accept an optional duration and clear the spinner line on exit.

- [x] **Step 2: Verify emitted bytes under a PTY**

Run the script in a pseudo-terminal and verify repeated writes are exactly `\x1b[1G\x1b[K` followed by one UTF-8 Braille frame, at approximately 80 ms intervals.

### Task 3: Document and verify the root cause

**Files:**
- Create: `docs/root-cause/2026-08-14-npx-braille-spinner-jitter.md`

- [x] **Step 1: Record cross-layer evidence**

Document the stable 80 ms PTY input, one dirty row per frame, approximately 1 ms render duration, absence of resize, the `881c309` regression point, and why per-frame overflow fitting changes dot geometry.

- [x] **Step 2: Run formatting and complete verification**

Run:

```bash
clang-format -i src/libqtghostty/TerminalWidget.cpp tests/test_terminal_widget.cpp
clang-format --dry-run --Werror src/libqtghostty/TerminalWidget.cpp tests/test_terminal_widget.cpp
cmake -B build -DBUILD_TESTING=ON
cmake --build build
QT_QPA_PLATFORM=offscreen ./build/tests/test_terminal_widget
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
git diff --check
```

- [x] **Step 3: Re-run the reproduction in deepin-terminal-ghostty**

Launch the built application with `--trace-vt` and `--execute` using the reproduction script, then confirm every spinner frame stays on one stable cell geometry with one dirty row and no resize/full-frame publication.
