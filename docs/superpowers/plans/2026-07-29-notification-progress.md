# Notification and Progress Implementation Plan

**Goal:** Present semantic OSC notifications and progress in the application.

- [x] Add failing `TerminalWidget` callback tests for OSC 777/9 and OSC 9;4.
- [x] Register semantic Ghostty callbacks and emit normalized Qt signals.
- [x] Add a failing app test for per-pane progress presentation.
- [x] Overlay a progress bar in `TerminalScrollContainer`.
- [x] Forward notification requests through `TermPane`.
- [x] Send notifications asynchronously through freedesktop D-Bus.
- [x] Run focused library/app tests, complete suites, formatting, and diff
  checks.

No commit is created unless the user explicitly requests one.
