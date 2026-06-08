# Issue 28: File Drop Did Not Insert Paths

## Symptom

Dropping a local file onto the terminal pane did nothing. Users expected the
drop to insert the file path into the running shell prompt.

## Root Cause

`TerminalWidget` did not accept drag-and-drop events, and it had no handling for
file URL MIME data. Qt therefore rejected the drag before any terminal input
could be generated.

## Fix

Enable drop handling on `TerminalWidget`, accept drops containing local file
URLs, convert each local URL to a shell-quoted path, and write the joined paths
to the PTY as user input. Non-local URLs are ignored.

## Verification

- Added `testDropLocalFileWritesEscapedPathToPty`.
- Verified the test fails before the implementation because the drag is not
  accepted.
- Verified the test passes after the implementation.
