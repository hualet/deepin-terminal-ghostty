# Session restore forwards stale mouse reports

## Symptom

After restoring a session, clicking or moving the pointer could insert text such
as `0;32;24M` into the new shell. The issue only occurred for some snapshots.

## Root Cause

Terminal snapshots preserve terminal modes so restored content retains its
display and keyboard state. This also preserved mouse tracking and mouse
encoding modes enabled by the old foreground process, such as a text editor or
pager.

Session restore creates a fresh PTY and shell before replaying the saved VT
stream. The fresh shell never enabled those restored mouse modes, but
`TerminalWidget` observed them in Ghostty state and encoded pointer events as
mouse reports. The new shell then received and displayed those reports as input.

## Fix

After replaying saved VT content, `TerminalWidget::importVtContent()` disables
the restored mouse tracking and encoding modes and resets the mouse encoder.
Other terminal modes remain intact, and mouse reporting enabled later by a live
foreground process continues to work normally.

## Verification

- Added `testImportVtContentClearsStaleMouseTracking`, which exports a snapshot
  from a terminal with SGR mouse tracking enabled, imports it into a fresh
  terminal, clicks the restored terminal, and verifies the fresh PTY receives no
  mouse report.
- Retained focused coverage proving live mouse press, release, and wheel reports
  still use SGR encoding when the current foreground process enables it.
