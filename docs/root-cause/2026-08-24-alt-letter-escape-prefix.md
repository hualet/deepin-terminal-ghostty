# Alt+Letter Escape Prefix Root Cause

## Summary

Alt+B (and every Alt+printable combination) reached the PTY as a bare `b`
instead of `ESC b`, so readline `backward-word` (M-b) and other Meta bindings
never fired. Alt+Backspace kept working, which made the regression look
inconsistent.

## Investigation Notes

An end-to-end probe of the rebuilt app (xdotool + `--trace-vt`) first
confirmed Alt+Backspace was correct (`pty.write "\e\x7f"`, bash replied with
`\b\b\b\b\b\e[K`), so the encoder path itself was healthy. A C-level probe
against the bundled `libghostty-vt` then isolated the failing variable:

| event configuration                              | encoded bytes |
| ------------------------------------------------ | ------------- |
| key=B, mods=ALT, utf8="b", consumed_mods=ALT     | `62` (`b`)    |
| key=B, mods=ALT, utf8="b", consumed_mods=0       | `1B 62` (`\eb`) |

The only difference was `consumed_mods`. `TerminalWidget::keyPressEvent`
marked SHIFT/CTRL/ALT consumed for every printable key, a behavior introduced
by cf866e2 ("ensure shell standard keybindings reach the PTY") while fixing
Ctrl combos.

Ghostty's `KeyEvent.effectiveMods()` subtracts `consumed_mods` from the
binding mods whenever utf8 text is present. With ALT consumed, the legacy
encoder saw no Alt in the binding mods and wrote the plain text `b`; under
the kitty keyboard protocol it likewise emitted the sequence without the alt
modifier bit. Upstream Ghostty's GTK apprt derives `consumed_mods` from GDK's
`getConsumedModifiers()`, where a plain letter key consumes no Alt on Linux:
Alt is a binding-only modifier there and never alters the produced text.

Alt+Backspace was unaffected because backspace resolves through the
function-key table, whose `{ .mods = .{ .alt = true }, .sequence = "\x1b\x7f" }`
entry is matched against the full mods, not the effective mods.

## Root Cause

`TerminalWidget` reported Alt as a consumed modifier for printable keys.
Consumed mods mean "already folded into the text by the platform input
layer", which is false for Alt on Linux. The encoder therefore stripped Alt
before choosing a sequence and degraded every Alt+letter/Alt+symbol to its
unmodified text form.

## Fix

`src/libqtghostty/TerminalWidget.cpp` no longer reports ALT in
`consumed_mods`. SHIFT remains consumed (it genuinely changes the produced
character, `a` → `A`), and CTRL's bit is kept but is inert because Ctrl-held
text is C0, which is stripped from the utf8 field before encoding.

## Verification

- Encoder probe: consumed_mods without ALT produces `1B 62` for Alt+B under
  default (legacy) encoder options sourced from a fresh terminal.
- Regression test `TestTerminalWidget::testAltLetterSendsEscapePrefix`
  (synthesized Alt+B QKeyEvent, widget output observed via
  `PtySession::dataWritten`) passes with the fix and was confirmed to fail
  when the old consumed-ALT line is temporarily restored. Asserting on the
  widget's written bytes keeps the test independent of the spawned shell and
  of any process-wide environment change.
- Full `test_terminal_widget` suite: 183 passed, 0 failed.
- End-to-end on the real app: typing `hello world` then Alt+B wrote `\eb`,
  and bash replied `\b\b\b\b\b` (cursor moved back over `world`), twice in a
  row; no literal `b` insertion.
