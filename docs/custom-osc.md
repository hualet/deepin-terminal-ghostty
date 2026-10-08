# Custom OSC Sequences

deepin-terminal-ghostty defines a small set of private OSC sequences for its
built-in shell integration. They let the terminal know which command is
running and how it finished, which drives the process icons and command
status dots in the vertical tab sidebar and the `command` field reported by
`dtermctl`.

These sequences live under the `OSC 777` namespace and are distinguished from
the rxvt `OSC 777;notify` extension by their `ShellCommand` prefix. Both `ST`
(`ESC \`) and `BEL` terminators are accepted.

## Shell Integration

The integration is injected automatically when the terminal starts an
interactive `bash` or `zsh` without an explicit command. Your own `.bashrc` /
`.zshrc` (and `.zshenv`) still load first, and the hooks re-register after
them, so frameworks that reset hook arrays do not drop them.

Other shells, or shells on remote hosts, can emit the same sequences by hand.

## `OSC 777;ShellCommand=<base64>`

Reports that a command started running.

```text
ESC ] 777 ; ShellCommand=<base64 command line> ST
```

- The payload is the command line encoded as base64 (UTF-8, no line breaks).
- The terminal picks the pane's process icon from the command (for example
  `claude`, `codex`, or `ssh`) and reports it in the `dtermctl` pane snapshot.
- The pane's command state becomes **Running**.

Example, emitted from a `preexec` hook:

```sh
printf '\033]777;ShellCommand=%s\033\\' "$(printf '%s' "$1" | base64 | tr -d '\n')"
```

## `OSC 777;ShellCommandResult=<exit-code>`

Reports the exit code of the command that just finished.

```text
ESC ] 777 ; ShellCommandResult=<decimal exit code> ST
```

- Must be sent before the clearing `ShellCommand=` below, while `$?` still
  holds the command's exit code.
- The value is only stored; it takes effect when the command is cleared.

## `OSC 777;ShellCommand=` (empty)

Reports that the command finished and the shell is about to draw a new prompt.

```text
ESC ] 777 ; ShellCommand= ST
```

- If a `ShellCommandResult` was received, the pane's command state becomes
  **Succeeded** (exit code `0`) or **Failed** (any other exit code);
  otherwise it returns to **Idle**.
- The process icon falls back to the default terminal icon.
- It also marks a prompt boundary for the
  [program status protocol](https://www.superlogical.com/rex/docs/build/program-status)
  (OSC 7501): `working`, `blocked`, and `idle` records are cleared, while
  `done` and `error` records stay until the user views the pane.

A `precmd` hook typically sends the result and the clear together:

```sh
printf '\033]777;ShellCommandResult=%s\033\\' "$?"
printf '\033]777;ShellCommand=\033\\'
```

## Command Lifecycle

```text
preexec  → OSC 777;ShellCommand=<base64>     state: Running
precmd   → OSC 777;ShellCommandResult=<N>    exit code recorded
precmd   → OSC 777;ShellCommand=             state: Succeeded / Failed
```

When a command finishes in a background tab, the vertical tab sidebar shows a
green (succeeded) or red (failed) dot on that tab until you switch to it.

## Compatible Third-Party Sequences

Besides its own sequences, the terminal also reads the running command from
these widely used shell integration sequences, so existing setups keep
working:

| Sequence | Source |
| --- | --- |
| `OSC 633;E;<command>` | VS Code shell integration |
| `OSC 133;…;cmdline_url=<url-encoded command>` | FinalTerm-style semantic prompts |
| `OSC 1337;SetUserVar=WEZTERM_PROG=<base64>` | WezTerm shell integration |

These set the running command (and the **Running** state) only. Clearing the
command and tracking exit codes require `OSC 777;ShellCommand=` and
`OSC 777;ShellCommandResult`.
