# Slow zsh Startup via Shell Integration ZDOTDIR Redirect

## Summary

Launching zsh in deepin-terminal-ghostty felt noticeably slower than in other
terminals. End-to-end measurement through the real `PtySession` code (real
zsh, full user config) showed ~930 ms to an idle prompt, while the same shell
in a plain terminal took ~410 ms. Roughly half a second of per-session
overhead was introduced by our own shell integration mechanism.

## Investigation Notes

Layered timing on the affected machine (zsh 5.9, oh-my-zsh, nvm):

- `zsh --no-rcs -i -c exit`: ~17 ms — zsh core startup is fast.
- `zsh -i -c exit` with `ZDOTDIR=$HOME`: ~410 ms — user rc files cost
  ~390 ms, dominated by nvm (~300 ms; `nvm_auto` filesystem scans) and
  oh-my-zsh (~90 ms with a warm completion cache).
- `zsh -i -c exit` with `ZDOTDIR` pointing at a fresh temp dir containing
  our wrapper `.zshrc`: ~910–1010 ms — a consistent ~520 ms penalty that
  reappeared for **every** fresh temp dir, i.e. every new tab/window.

`zprof` on the slow path showed a full completion-system rebuild on each
start: `compinit` ~685 ms (zprof-inflated), 846 `compdef` calls, plus
`compaudit`, `compdump`, and `zrecompile` all executing. On the normal path
these run once and are cached in `~/.zcompdump-<host>-<version>[.zwc]`.

oh-my-zsh derives its completion dump path as:

```zsh
ZSH_COMPDUMP="${ZDOTDIR:-$HOME}/.zcompdump-${SHORT_HOST}-${ZSH_VERSION}"
```

(oh-my-zsh.sh line 110). Since our zsh integration redirected `ZDOTDIR` to a
throwaway `mkdtemp` directory for the lifetime of the session, the dump
landed inside the temp dir, was never found again on the next start, and
compinit performed a cold rebuild on every single session.

A secondary correctness issue shared the same root: with `ZDOTDIR` on the
temp dir and no `.zshenv` in it, zsh never sourced the user's `~/.zshenv`
(for this user: cargo and zvm `PATH` setup), and a user-set `ZDOTDIR` was
discarded entirely (`spawn()` stripped it from the environment).

## Root Cause

The zsh shell integration kept `ZDOTDIR` pointed at a temporary wrapper
directory for the whole session. Startup files that resolve paths relative
to `ZDOTDIR` — most importantly oh-my-zsh's `.zcompdump` cache — resolved
them inside the throwaway directory, so per-user caches were defeated and
expensive initialization work was redone on every session.

## Fix

`src/libqtghostty/PtySession.cpp`, `shellIntegrationFor()` zsh branch now
uses a two-stage integration in a temp `ZDOTDIR`:

- The temp `.zshenv` restores the original `ZDOTDIR` state (set, empty, or
  unset — tracked separately via `DEEPIN_TERMINAL_GHOSTTY_ZDOTDIR` and a
  `_SET` flag, because `qgetenv()` cannot distinguish set-empty from
  unset), sources the user's real `.zshenv` from the correct location,
  defines the reporting hooks, and registers them immediately. It then
  records the resulting parameter's set state, exact value, and zsh export
  attribute. If the user's `.zshenv` relocated `ZDOTDIR` itself, the new
  location and its original export behavior are honored for their `.zshrc`
  and the rest of the session.
- The `.zshenv` also handles `unsetopt RCS` in the user's `.zshenv`: that
  option makes zsh skip every later startup file, including the wrapper.
  In that case the earlier registration is the fallback that keeps OSC 777
  reporting alive, and the user's own `ZDOTDIR` state is restored directly
  instead of leaving it on the temp dir.
- When rc files are still enabled, `ZDOTDIR` points back at the temp dir
  so zsh loads the `.zshrc` wrapper, which restores the user's `ZDOTDIR`
  state **before** sourcing their real `.zshrc` — so ZDOTDIR-derived paths
  such as oh-my-zsh's `.zcompdump` cache resolve exactly like a plain
  terminal — and re-registers the hooks afterwards.
- The child environment builder removes inherited instances of the private
  `DEEPIN_TERMINAL_GHOSTTY_ZDOTDIR` transport marker before appending the
  current session's value. A stale application-level marker can therefore
  neither override an unset `ZDOTDIR` nor create duplicate marker entries.
- Registration is idempotent (membership-checked appends), so having both
  stages never duplicates hook entries. Re-registering after the user's
  startup files keeps the hooks alive across configurations that reset
  the hook arrays (`preexec_functions=()` and friends). The wrapper also
  guards against sourcing itself recursively and quotes
  builtin/external command words so user aliases defined during startup
  cannot hijack the integration.

The emitted sequences (`ShellCommand=`, `ShellCommandResult=`, clear) are
unchanged. Registering only from a temp `.zshenv` (the Ghostty pattern)
would break under hook-array resets; registering only from the wrapper
breaks under `unsetopt RCS`; any mechanism that leaves `ZDOTDIR` on a
session-scoped temp directory while the user's rc files execute
re-triggers the compinit rebuild.

## Verification

- Full `test_pty_session` suite: 22 passed, 0 failed (offscreen), including
  `testZshShellIntegrationHookReportsCommands` (OSC 777 contract end-to-end
  with a real zsh) and five regressions:
  - `testZshShellIntegrationSurvivesHookArrayReset`: a custom `ZDOTDIR`
    whose `.zshrc` runs `preexec_functions=()` / `precmd_functions=()` —
    asserts the user's `.zshrc` is still sourced from the custom dir and
    command reporting still works afterwards.
  - `testZshShellIntegrationSurvivesUnsetoptRcs`: a custom `ZDOTDIR` whose
    `.zshenv` runs `unsetopt RCS` — asserts the user's `.zshenv` runs,
    their `.zshrc` stays skipped (plain-terminal behavior), and command
    reporting still works via the `.zshenv` fallback.
  - `testZshShellIntegrationPreservesEmptyZdotdir`: `ZDOTDIR` explicitly
    set to empty with `SHELL` forced to `/bin/zsh` — asserts hooks still
    register and report.
  - `testZshShellIntegrationPreservesUnexportedZdotdir`: a HOME `.zshenv`
    relocates `ZDOTDIR` with an unexported assignment — asserts the relocated
    `.zshrc` runs and still observes a non-exported scalar.
  - `testZshShellIntegrationDiscardsInheritedZdotdirMarker`: an inherited
    private marker points at a stale startup directory while `ZDOTDIR` is
    unset — asserts HOME startup files run and stale files do not.
- These tests save/restore process-wide `SHELL`, `HOME`, `ZDOTDIR`, and the
  private marker through an RAII guard, so a failed `QVERIFY` cannot leak
  modified environment variables into later tests.
- `strace` of a real `PtySession` + zsh child: with `ZDOTDIR=` (empty) zsh
  `access()`es `/.zshenv` and `/.zshrc` (zsh-native semantics, never
  `$HOME/.zsh*`); with `ZDOTDIR` unset it accesses and sources
  `$HOME/.zshenv`/`$HOME/.zshrc` as usual.
- Timing harness against the real `PtySession` + real zsh + full user
  config: first PTY output ~380–440 ms, idle prompt ~405–470 ms (previously
  ~930 ms) — matching a plain terminal within noise.
- `clang-format --dry-run --Werror` clean on changed files.
- Full offscreen CTest suite with a writable temporary HOME: 11 passed, 0
  failed. The first run with the sandbox's read-only HOME failed only four
  `MainWindow` snapshot assertions after `SessionManager::save()` could not
  write under `~/.qttest`; rerunning with a writable HOME passed all tests.

Remaining startup cost is user configuration, not the terminal: nvm accounts
for ~300 ms of the ~410 ms baseline (sourcing `nvm.sh` plus `nvm_auto`
filesystem scans). Lazy-loading nvm in `~/.zshrc` would recover most of it
in every terminal, independently of this fix.
