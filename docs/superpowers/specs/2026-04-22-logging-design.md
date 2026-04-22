# Logging Support Design

**Date:** 2026-04-22

## Goal

Add baseline logging support to `deepin-terminal-ghostty` using the DTK logging library, following the `deepin-terminal` pattern, without expanding the change into a repo-wide logging retrofit.

## Scope

This design covers only:

- DTK log appender initialization in the application entry point
- a small shared logging module for `QLoggingCategory` declarations
- focused logging in the main application and terminal session core path
- logging conventions added to `AGENTS.md`

This design explicitly does not cover:

- file-based or user-configurable logging rules
- logging every application source file
- UI-level log settings
- tests that assert exact log message text

## Approach

### 1. Initialize DTK logging in `main.cpp`

`src/app/main.cpp` will initialize DTK logging early in process startup, following the same general pattern used by `deepin-terminal`:

- always register the journal appender when available
- register the console appender in debug builds

This keeps the startup behavior simple and production-safe while still giving developers immediate console visibility in debug builds.

The application entry point will also emit a small number of startup log messages:

- application startup
- translation loading result when relevant
- main window show/start completion

### 2. Add a shared logging category module

A new lightweight module under `src/` will centralize category declarations so the first logging pass stays consistent across files.

Planned categories:

- `org.deepin_terminal_ghostty.app`
- `org.deepin_terminal_ghostty.pty`
- `org.deepin_terminal_ghostty.terminal`

The module will expose declarations through a header and define categories in one `.cpp` file. As in `deepin-terminal`, debug builds will allow `QtDebugMsg`, while release builds will default categories to `QtInfoMsg`.

This avoids copy-pasting category definitions and establishes a clear naming convention for later expansion.

### 3. Add logs only to core paths

The first pass will log only meaningful lifecycle and failure points.

`MainWindow`:

- initial tab/session creation
- tab creation/removal
- closing the last tab and window shutdown path

`PtySession`:

- PTY startup attempts and failures
- shell resolution failures or fallback behavior
- PTY process/session closure
- write failures

`TerminalWidget`:

- terminal backend initialization failures
- PTY/session wiring failures if encountered
- session close propagation when it matters to window behavior

The logging level rules will be:

- `qCInfo`: significant lifecycle milestones
- `qCWarning`: recoverable failures, fallbacks, or unexpected states
- `qCCritical`: initialization failures that prevent a component from functioning
- `qCDebug`: developer-oriented state tracing only where it materially helps diagnose PTY or window lifecycle issues

### 4. Keep hot paths quiet

No logging will be added inside high-frequency paint/render/output loops unless the code is already on an error path. This is important for terminal performance and to avoid log spam from normal PTY traffic.

### 5. Test strategy

Tests will stay narrow:

- update build/test targets if the shared logging module must be linked into test binaries
- run the most relevant existing test target(s) to verify the logging changes do not break construction or lifecycle behavior

The implementation will not add brittle assertions on emitted log text.

## Files Expected To Change

- `CMakeLists.txt`
- `tests/CMakeLists.txt`
- `src/app/main.cpp`
- `src/app/MainWindow.cpp`
- `src/libqtghostty/PtySession.cpp`
- `src/libqtghostty/TerminalWidget.cpp`
- `AGENTS.md`

New files:

- `src/logging/Logging.h`
- `src/logging/Logging.cpp`

## AGENTS.md Logging Rules

`AGENTS.md` will gain a short logging section with these rules:

- use project logging categories instead of raw `qDebug()`/`qWarning()` in normal code
- keep category names under the `org.deepin_terminal_ghostty.*` prefix
- log lifecycle milestones at `info`
- log recoverable failures and fallbacks at `warning`
- log unrecoverable initialization failures at `critical`
- avoid logs in paint paths, per-byte PTY read paths, and other high-frequency loops
- include enough context in failure logs to diagnose the failing operation

## Risks

- DTK appender APIs may differ slightly across the installed DTK6 version; implementation should match the headers available in this repo's build environment
- if the shared logging module is linked incorrectly, test targets may fail to build; this will be handled in CMake rather than with ad hoc includes
- over-logging PTY traffic would create noise and potential performance cost, so the implementation must stay disciplined about log placement

## Success Criteria

The work is complete when:

- the app initializes DTK logging on startup
- the project has shared logging categories for app, PTY, and terminal layers
- core lifecycle/failure logs exist in `main`, `MainWindow`, `PtySession`, and `TerminalWidget`
- `AGENTS.md` documents the logging rules
- the relevant test/build commands still pass
