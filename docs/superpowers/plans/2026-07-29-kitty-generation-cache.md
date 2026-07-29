# Kitty Generation Cache Implementation Plan

**Goal:** Avoid redundant Kitty pixel copies without serving stale images.

- [x] Add a regression proving text output preserves a decoded image cache.
- [x] Confirm the regression fails with unconditional cache clearing.
- [x] Cache `QImage` together with the Ghostty image generation.
- [x] Remove per-write cache clearing while preserving reset clearing.
- [x] Add a same-ID replacement regression and verify rendered pixels update.
- [x] Run Kitty-focused tests, the complete widget suite, formatting, and
  diff checks.

No commit is created unless the user explicitly requests one.
