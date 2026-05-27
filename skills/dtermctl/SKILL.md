---
name: dtermctl
description: Use when controlling deepin-terminal-ghostty via CLI — listing windows/tabs/panes, creating windows and tabs, splitting panes, sending text or executing commands in panes. Triggers include requests to automate terminal sessions, inspect terminal state, or interact with running panes programmatically.
---

# dtermctl

CLI tool for controlling a running `deepin-terminal-ghostty` instance via D-Bus.

## Prerequisites

- `deepin-terminal-ghostty` must be running (it registers on the session bus as `org.deepin.TerminalGhostty`)
- `dtermctl` binary must be in `PATH`

## Quick Reference

| Command | Syntax | Description |
|---------|--------|-------------|
| `list` | `dtermctl list` | List all windows, tabs, panes with screen content (JSON) |
| `new-window` | `dtermctl new-window` | Open a new terminal window |
| `new-tab` | `dtermctl new-tab [--window <id>]` | Open a new tab (in first window or specified window) |
| `split` | `dtermctl split --pane <id> (--horizontal\|--vertical)` | Split a pane |
| `send` | `dtermctl send --pane <id> --text <text>` | Send raw text to a pane's PTY |
| `exec` | `dtermctl exec --pane <id> -- <command>` | Execute a shell command in a pane |

## Response Format

All commands return JSON to stdout:

```json
{"ok": true, ...}
{"ok": false, "error": "error message"}
```

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | D-Bus error (service unavailable or error response) |
| 2 | Usage error (missing arguments, unknown command) |

## Common Workflows

### Inspect terminal state

```bash
dtermctl list
```

Returns JSON with structure: `windows → tabs → panes`. Each pane has a `paneId` (UUID) used by other commands.

### Create a workspace

```bash
WIN=$(dtermctl new-window | jq -r '.windowId')
TAB_PANE=$(dtermctl new-tab --window "$WIN" | jq -r '.paneId')
dtermctl split --pane "$TAB_PANE" --vertical
```

### Run a command in a pane

```bash
dtermctl exec --pane "<pane-uuid>" -- ls -la /tmp
```

### Send keystrokes

```bash
dtermctl send --pane "<pane-uuid>" --text $'hello\n'
```

ANSI-C quoting (`$'...'`) ensures a real newline is sent to the PTY, acting as pressing Enter.

## D-Bus Details

- **Bus**: Session bus
- **Service**: `org.deepin.TerminalGhostty`
- **Path**: `/org/deepin/TerminalGhostty/Control`
- **Interface**: `org.deepin.TerminalGhostty.Control`

## Common Mistakes

- **"Service not available"**: The terminal app is not running.
- **"Pane not found"**: The pane UUID is stale (tab/window was closed). Run `list` to get current IDs.
- **Forgetting `--` in exec**: Without `--`, flags in the command may be parsed by `dtermctl` instead of being forwarded.
