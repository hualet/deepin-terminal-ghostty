# Overview foreground launch and keyboard focus

## Reproduction

Both review findings reproduced in QTest using actual app widgets and PTY
sessions. No desktop dock click or manual running-app reproduction is
claimed.

`testOverviewForegroundLaunchDismisses` failed for `controlNewTab`,
`controlOpenTab`, and `TerminalControlService::openTab` in both horizontal
and vertical modes. Each foreground request created the second tab but
left the overview visible.

`testOverviewTabFocusStaysInside` failed in both directions and tab modes.
Horizontal Shift+Tab escaped from overview controls into `TerminalWidget`
at step 1; forward Tab escaped to the DTK titlebar option button at step 6.
Vertical mode also allowed traversal into the titlebar. The old external
new-tab test explicitly expected overview search focus, encoding the wrong
foreground-launch behavior.

## Causes

Startup reuse routes through `forwardOpenTab` to the control service's
`openTab`, then `MainWindow::controlOpenTab` and `addTab(true)`. The last
method focused overview search instead of dismissing the overview before
creating the foreground session.

The overview is an ordinary sibling widget overlay. QWidget's default
focus traversal climbs to the window and includes other visible siblings,
including the underlying terminal in horizontal mode. Disabling application
shortcuts does not limit Qt's Tab focus chain.

## Fixes

Foreground `addTab(true)` dismisses overview before creating and activating
the requested terminal. Background tab creation retains its existing
behavior. The replacement tests assert that the new shell is visible,
focused, and receives the typed byte through its PTY.

`WorkspaceOverview::focusNextPrevChild` walks Qt's focus chain in either
direction but selects only enabled, visible descendants with Tab focus.
It wraps within the overview and consumes traversal when no eligible
control exists. This covers its search, buttons, cards, and scroll area
without modifying terminal input behavior or PTY geometry.

## Verification

Regression tests cycle Tab/Shift+Tab repeatedly in both tab modes and check
that typed text does not reach the PTY. They also exercise Enter with no
matching cards and Escape focus restoration. Foreground launch coverage
uses the same control-service method that receives startup forwarding.

The focused launch, traversal, search, Escape, and confirmation run reported
15 passing cases. The final fresh application build, full C++ format check,
and all 11 CTest suites passed in an isolated D-Bus session with the Qt
offscreen platform and Deepin platform theme.
