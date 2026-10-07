# Workspace overview

Status: approved on 2026-10-07. The revised entry design exposes the
main-menu action and configurable shortcut only; no navigation button.

## Reference and scope

Ghostty's Linux frontend exposes `toggle_tab_overview` and uses
`Adw.TabOverview` around its tab view. Its titlebar includes a grid button,
and the overview enables creating tabs. Reference:
https://ghostty.org/docs/config/keybind/reference#toggle_tab_overview

Local reference: `~/projects/g/ghostty/src/apprt/gtk/ui/1.5/window.blp`.

The recommended scope is the current window's tabs. Each card previews a
whole tab, including its split layout. Existing app commands use
"workspace" for individual terminal splits; this overview does not change
that meaning or introduce a new persistent workspace model.

## Alternatives

1. Whole-tab cards (recommended): matches Ghostty and preserves the context
   of related split terminals.
2. Individual-pane cards: enables direct selection of every terminal, but
   breaks the visual grouping of each tab.
3. All-window overview: includes more sessions but requires cross-window
   ownership, activation, and lifecycle handling beyond this first version.

## Interaction

- A main-menu action toggles the overview in both horizontal and vertical
  tab modes. The tab bars retain only their existing add-tab buttons.
- Add a configurable `workspace_overview` shortcut, defaulting to
  `Ctrl+Shift+O`, which is unused by the existing shortcut defaults.
- Show a scrollable, responsive grid with tab titles and scaled previews.
  Highlight the current tab. Preserve each preview's aspect ratio.
- Clicking a card activates its tab and exits the overview. Arrow keys
  navigate cards, Enter activates, and Escape returns to the current tab.
- A title search filters cards case-insensitively. Show an explicit empty
  result message when nothing matches.
- Provide a new-tab button and a close button on each card. New tab exits
  the overview and focuses the new terminal. Close uses the app's existing
  running-process confirmation and last-tab window-close behavior.
- The vertical sidebar is hidden during preview and restored on exit.
- Clicking a horizontal titlebar tab uses the same stable-ID activation
  path as a preview card, including exiting preview and restoring focus.
  Clicking the already current tab also exits. Reordering or asynchronous
  tab removal continues to update the preview without dismissing it.
- Foreground terminal launches dismiss the overview and activate the new
  shell. Background tab additions may update the grid without dismissing.
- Tab and Shift+Tab cycle only through enabled, visible overview controls;
  they cannot focus the underlying terminal or titlebar controls.
- Exiting restores focus to the selected tab's active split terminal.
- No drag rearrangement or cross-window aggregation in this version.

## Architecture and state

Keep the overview widget and tab actions in `src/app/`. `MainWindow` owns
mode transitions and supplies entries using stable `TabRecord::id` values,
titles, current state, and previews. Resolve IDs at activation and close
time so sorting, detachment, or asynchronous session exit cannot target the
wrong tab. Handle removed entries safely and update the visible grid after
tab creation, removal, reordering, title changes, and split changes.

Present the overview across the entire content host. In vertical mode,
hide the whole sidebar/terminal splitter container during preview and
restore it on exit. This preserves its child geometry and saved splitter
sizes. Keep terminal widgets in their original stack/split hierarchy;
previewing must not resize PTYs or recreate sessions. The overlay receives
keyboard focus, while PTYs continue running. The remote panel is hidden on
entry. Resize the overlay with its host and keep it above rebuilt layouts.

Capture previews at entry and refresh periodically while visible.
`TerminalWidget::renderSnapshot()` draws directly from terminal rendering
state; `TermPane::renderPreview()` composes the splitter tree. Neither path
invokes Qt hidden-widget layout or resizes PTYs. `qtghostty` exposes only
the generic terminal snapshot capability.

Use the application palette for controls, card borders, and focus state;
terminal snapshots retain their actual terminal theme. Preview failure
shows a title card rather than blocking navigation. Stop refresh work on
exit. Expose accessible names for entry, search, cards, close, and new tab.

## Verification

Add focused tests for entry/exit, mouse and keyboard activation, filtering,
new and close actions, focus restoration, session exit during overview,
stable-ID handling after reorder, horizontal/vertical mode compatibility,
and unchanged terminal grid/session identity during preview capture.
Verify previews contain terminal output and split structure, including a
hidden tab. Run the affected tests with `QT_QPA_PLATFORM=offscreen`, then
the complete CTest suite, and check formatting of changed C++ sources.
Inspect actual rendered overview images at normal and narrow window sizes
in light and dark palettes before reporting UI completion.

No version change is part of this feature.

## Transitions

Entering morphs the current tab's pane into its card's preview while the
overview fades in (240 ms, OutCubic). Exiting reverses it: the selected
card's preview grows back to the pane while the overview fades out
(200 ms, InOutCubic). When no card matches the tab, such as a newly added
tab or a card scrolled out of view, only the cross-fade runs.

`OverviewTransition` is a paint-only, mouse-transparent overlay on the
content host that interpolates between two captured frames. Mode state,
focus, and shortcut gating still switch immediately, so typing during
the animation reaches the destination view. The destination frame is
captured on the next event-loop turn so tab and layout changes settle.
A new toggle or a content resize finishes the running transition first.
The animation is skipped when DTK reports a non-special-effects
environment or the window is not visible.
