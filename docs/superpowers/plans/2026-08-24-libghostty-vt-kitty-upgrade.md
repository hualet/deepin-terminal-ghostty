# libghostty-vt Kitty Upgrade Implementation Plan

> **For agentic workers:** Execute this plan inline and track each checkbox.
> Do not commit unless the user asks; preserve the unrelated `.worktrees/`
> directory.

**Goal:** Upgrade the vendored Ghostty VT library and public headers to
`e77b2309fca3a27db1123a4f904b7fb432ee7162`, keep existing terminal behavior,
and document the remaining work needed for complete Kitty graphics support.

**Architecture:** Treat the shared library, complete public header tree, and
the Qt wrapper's API migration as one compatibility unit. Keep protocol parsing
inside Ghostty, keep all local-file media disabled, and defer new Kitty rendering
behavior to an explicit TODO dependency list.

**Tech Stack:** C++20, Qt6 Widgets, CMake, Qt Test, Zig 0.16,
`libghostty-vt`

---

### Task 1: Establish the compatibility regression

**Files:**

- Verify: `src/libqtghostty/TerminalWidget.cpp`
- Use: `/tmp/ghostty-kitty-audit.AIaPT5/zig-out/include`

- [x] Configure an isolated test build against the pinned upstream headers.
- [x] Build and confirm the old mode/color calls fail because the compatibility
  helpers were removed upstream.

### Task 2: Build and stage the pinned artifacts

**Files:**

- Replace: `lib/libghostty-vt.so`
- Replace: `lib/include/`

- [x] Build `libghostty-vt` in ReleaseFast mode for a baseline CPU with
  `-Dlib-version-string=0.1.0-dev+e77b2309f`.
- [x] Synchronize `zig-out/include/` and the versioned shared-library file as an
  atomic artifact set.
- [x] Verify the version string, build flags, SONAME, runtime dependencies, and
  exported symbol surface.

### Task 3: Migrate the Qt wrapper

**Files:**

- Modify: `src/libqtghostty/TerminalWidget.cpp`

- [x] Add small wrapper helpers that query and set `GhosttyTerminalModeConfig`
  through `ghostty_terminal_get()` and `ghostty_terminal_set()`.
- [x] Replace removed `ghostty_terminal_mode_get()` and
  `ghostty_terminal_mode_set()` calls with the helpers.
- [x] Replace `ghostty_render_state_colors_get()` with
  `ghostty_render_state_get(..., GHOSTTY_RENDER_STATE_DATA_COLORS, ...)`.
- [x] Disable `GHOSTTY_TERMINAL_OPT_KITTY_IMAGE_MEDIUM_TEMP_FILE` with a null
  pointer instead of the old incorrectly typed `bool*`.
- [x] Build and run focused terminal widget tests for synchronized output,
  focus reporting, bracketed paste, and Kitty direct-image rendering.

### Task 4: Record the remaining Kitty compatibility work

**Files:**

- Modify: `docs/TODOs.md`
- Modify: `docs/kitty-image-protocol.md`

- [x] Record implementation gaps for Unicode placeholders, virtual-rooted
  relative placements, animation scheduling, and optional local media.
- [x] Record required Ghostty C APIs, Qt scheduling/security dependencies, and
  protocol conformance fixtures separately from implementation work.
- [x] Update the current-version description to the pinned revision without
  claiming the deferred features are supported.

### Task 5: Final verification

**Files:**

- Verify: all scoped files

- [x] Run `clang-format` verification for the changed C++ source.
- [x] Run the complete offscreen CTest suite with a writable test home.
- [x] Run a real direct Kitty image smoke test if the application can be
  launched in the active desktop session.
- [x] Run `git diff --check`, inspect artifact/header integrity, and confirm no
  unrelated files are included.
