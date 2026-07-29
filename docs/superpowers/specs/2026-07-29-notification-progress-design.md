# Terminal Notification and Progress Design

## Goal

Surface OSC desktop notifications and progress reports through native desktop
and per-pane UI without putting application policy into `qtghostty`.

## Boundaries

`TerminalWidget` registers Ghostty's semantic callbacks and emits normalized Qt
signals. `TerminalScrollContainer` owns a thin progress indicator for its pane.
`TermPane` forwards desktop notification requests, and `MainWindow` sends them
to the freedesktop notification service.

## Progress presentation

- remove: hide the indicator;
- set: show determinate progress from 0 to 100;
- indeterminate: show a busy indicator;
- pause: keep the last percentage and use a warning color;
- error: keep the reported percentage when present and use an error color.

The indicator is overlaid at the bottom of each terminal so it works in tabs,
splits, and either tab-bar layout without reducing terminal grid geometry.

## Notification presentation

Use the title supplied by OSC 777, falling back to the pane title and then the
application name. Forward the body unchanged as UTF-8. The D-Bus call is
asynchronous so terminal parsing never waits for the notification daemon.

## Verification

Test split OSC sequences at the `TerminalWidget` boundary, all progress states,
the container's visibility/range/value/state mapping, and `TermPane` signal
forwarding. The D-Bus send remains a thin platform integration.
