# Session restore: stale PTY output survives import

## Symptom

The previous fix reset the Ghostty terminal before importing saved VT content,
but restored sessions could still show stray prompt output or inherit shell
integration command state from the newly started shell. In some restored
sessions, the newly started shell prompt could also overwrite restored content.

## Root Cause

`TerminalWidget::importVtContent()` only reset the Ghostty terminal state. It did
not clear `TerminalWidget`'s own queued PTY data or shell integration scanner
state, and it left the live terminal cursor at the position encoded in the saved
snapshot.

During session restore, the replacement terminal starts a fresh PTY before the
saved VT content is imported. Any prompt bytes that arrived before import were
stored in `m_pendingPtyData`; any shell integration OSC bytes were also scanned
immediately and could update `shellCommand`, `commandState`, or
`m_pendingExitCode`. Resetting Ghostty did not discard those app-layer buffers,
so they could still affect the restored terminal after import.

The saved VT stream also includes the old cursor position. After import, the new
shell keeps running in the same widget and writes its fresh prompt at that
restored cursor position. If the saved cursor was on top of previous content,
the fresh prompt overwrote part of the restored screen.

## Fix

Before importing saved VT data, `importVtContent()` now clears pending PTY data,
the OSC scan buffer, command result tracking, shell command state, command state,
and the kitty image cache, then resets Ghostty and writes the saved VT stream.
After replaying the saved stream, it moves the live cursor to a fresh line after
the restored screen so subsequent PTY output cannot overwrite the snapshot.

## Verification

- Added `testImportVtContentDropsPendingPtyOutput`.
- Added `testImportVtContentClearsStaleShellIntegrationState`.
- Added `testImportVtContentKeepsFuturePtyOutputAfterRestoredScreen`.
- Ran the focused terminal widget import tests under `QT_QPA_PLATFORM=offscreen`.
