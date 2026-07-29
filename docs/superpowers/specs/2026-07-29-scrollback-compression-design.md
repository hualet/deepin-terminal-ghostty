# Scrollback Compression Design

## Scope

Drive the upgraded Ghostty terminal's caller-owned incremental scrollback
compression from `TerminalWidget`. Compression must only run after terminal
activity becomes idle and must preserve all logical terminal contents.

## Scheduling

The Qt integration follows Ghostty's renderer policy:

- wait 250 ms after compression-relevant activity;
- perform one bounded incremental compression step;
- when Ghostty reports pending work, schedule the next step after 1 ms;
- stop when Ghostty reports complete or unsupported.

`TerminalWidget` caches Ghostty's opaque compression activity token. A
terminal mutation only restarts the idle timer when the token changes.
Before each step, the token is checked again; changed activity postpones
compression for another idle interval.

All terminal access remains on the widget's Qt thread. This satisfies
Ghostty's serialization requirement without adding locks or a worker thread.

## Mutation Coverage

Scheduling is refreshed after:

- PTY output is written into Ghostty;
- restored VT content resets and repopulates the terminal;
- terminal grid resizing;
- viewport movement, because viewed history is ineligible for compression;
- runtime scrollback-limit changes.

Calls that only read render state do not restart the timer.

## Failure Handling

An unsupported result disables compression for that widget. API failures are
logged with the terminal logging category and stop the current pass; future
activity may retry. Compression never blocks initialization or terminal I/O.

## Verification

Tests will prove that:

- output schedules compression but no step runs before the idle interval;
- later output postpones the deadline;
- incremental steps run after idle and continue while pending;
- terminal text remains identical after a complete compression pass.

The focused tests and complete `TerminalWidget` suite run offscreen. A local
RSS probe with a large repetitive scrollback will compare resident memory
before and after a completed compression pass while also checking exported
content identity.

