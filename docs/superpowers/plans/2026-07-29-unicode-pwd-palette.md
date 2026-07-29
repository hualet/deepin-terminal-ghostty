# Unicode, PWD, and Palette Implementation Plan

**Goal:** Align Qt integration with Ghostty's Unicode, PWD, and color state.

- [x] Add failing grapheme-width tests and use Ghostty widths for IME overlays.
- [x] Add failing OSC PWD tests and register the semantic PWD callback.
- [x] Prefer decoded local terminal PWD with the PTY process fallback intact.
- [x] Add a generated-palette regression.
- [x] Generate the extended palette from the active theme.
- [x] Run focused and complete tests, formatting, and diff checks.

No commit is created unless the user explicitly requests one.
