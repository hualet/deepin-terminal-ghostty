# Kitty Render-State Images and Animation Implementation Plan

> Use superpowers:subagent-driven-development, one implementer followed by spec
> and quality review. Leave implementation changes uncommitted.

**Goal:** Render virtual/relative Kitty placements and play animations through
Ghostty 1.4's public C APIs.

**Architecture:** libghostty-vt resolves protocol geometry and animation state;
TerminalWidget converts current image pixels, composes layers and schedules Qt
redraws. No application actions or new transfer media are added.

**Tech Stack:** C++20, Qt6 QPainter/QImage/QTimer, libghostty-vt 1.4, Qt Test.

## Task 1: Regression fixtures before rendering changes

Files: tests/test_terminal_widget.cpp.

- [x] Add captured direct-image plus U=1/U+10EEEE virtual placement stream and
  assert image-colored pixels in its source/destination cells; observe failure.
- [x] Add relative placement with virtual parent, cropping and z order cases.
- [x] Add delete/move/replace tests that inspect rendered pixels, including no
  stale image after deletion with clean text rows.
- [x] Add a two-frame animation stream from the upstream example and assert
  frame changes via Qt scheduling, and absence of background timer when hidden.

## Task 2: Unified drawing and cache ownership

Files: src/libqtghostty/TerminalWidget.h/.cpp.

- [x] Allocate/free GhosttyRenderStateImageIterator with other render handles.
- [x] Replace old storage iterator drawing with render-state image entries;
  check GhosttyResult, protocol and geometry, sort layers/z, clip drawing.
- [x] Reuse current RGB/RGBA/grayscale conversion, keyed by per-image generation.
- [x] Suppress placeholder glyphs and avoid duplicate fixed placement drawing.
- [x] Invalidate the back buffer on placement/pixel changes and removal. Preserve
  incremental text behavior without visible images and bound QImage retention.

## Task 3: Animation scheduling

Files: src/libqtghostty/TerminalWidget.h/.cpp, tests/test_terminal_widget.cpp.

- [x] Use QElapsedTimer and a single-shot QTimer. Tick once before a frame;
  reacquire handles afterwards; on SUCCESS schedule next_ms with minimum8ms;
  on NO_VALUE stop; log errors without a busy loop.
- [x] Stop timer on hide/minimize, restart via visible redraw; clear state on
  reset/import/session shutdown. Do not violate synchronized-output holds.
- [x] Verify finite loops, stop/resume/delete and hidden snapshot rendering.

## Task 4: Validation and current support documentation

Files: docs/kitty-image-protocol.md, docs/TODOs.md.

- [x] Run focused image/animation tests, existing render/cache/sync-output tests,
  then complete offscreen CTest with the stable test shell environment.
- [x] Format changed C++, run diff checks and inspect image/cache lifetime.
- [x] Update support documentation with exactly the verified paths; retain
  unchecked protocol-conformance work and disabled local media requirements.

## Validation log

- Old renderer: seven first regression cases ran; six failed as expected
  (virtual image pixels, placeholder suppression, last-image deletion, cache
  fixture, animation playback and synchronized animation). Crop/z/partial-scroll
  already passed. The three existing image/cache regressions were retained.
- The first implementation built successfully with the existing development
  environment. Runtime validation then stopped because `libQt6Test.so.6`, Qt6
  CMake/development files and clang-format were missing after the environment
  interruption. Rootless dependencies were subsequently restored separately.
- Layer/alpha and finite-loop/reset/import/session-end cases were added during
  the interruption; these were subsequently compiled and verified in the final
  validation below.
- Restored rootless QtTest runtime: first implementation passes all ten image
  and animation cases plus init/cleanup (12 passed). Eleven existing engine,
  dirty-row/cursor, synchronized-output, opacity, decorations and margin cases
  also pass plus init/cleanup (13 passed).
- Runtime `LD_LIBRARY_PATH` must put `build/tests` and `build` before the
  rootless dependency prefix so an installed system Ghostty library cannot
  override the vendored artifact. The version assertion verifies this choice.

## Final validation

- Rootless dependency restoration used `build/local-deps/env.sh`. Reconfigured
  cached Qt/DTK, pkg-config font dependencies and OpenGL paths into that prefix;
  `cmake --build build -j4` completed the full application and every test target.
- Final focused run: 15 Qt Test results passed, covering the engine pin, three
  existing image/cache cases and nine new image/animation cases.
- Split CTest: `TerminalWidget` passed (21.37 s) with isolated HOME,
  `SHELL=/bin/sh`, `PS1=$'\033[0m'`, `PS2=''`, offscreen. The remaining eleven
  entries passed (32.33 s) with a separate isolated HOME and `PS1='$ '`.
- Changed C++ files pass clang-format-17 `--dry-run --Werror`; `git diff --check`
  passes. Images retain owned pixel copies, all borrowed handles are reacquired
  after animation ticks, and the render-state image iterator is freed before
  its owning render state.
- An initial full widget run had two link-hover failures after wide characters;
  both passed in isolation, and neither recurred in the final rebuilt full
  widget run. The cause is unestablished. No unrelated link code was modified.
- No commit was created. Transfer/malformed-input, detailed placeholder and
  animation conformance cases remain unchecked in `docs/TODOs.md`.

- Restored the unchanged tracked settings translation source from HEAD after
  the DTK build generator removed it; no application source cleanup remains.
