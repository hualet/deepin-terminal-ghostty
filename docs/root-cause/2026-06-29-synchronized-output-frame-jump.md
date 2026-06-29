# Synchronized Output Frame Jump

## Symptom

Kimi and other modern TUI applications could visibly jump while replacing a large section of terminal content. The problem was most noticeable during full-screen or near-full-screen animated updates.

## Investigation

A run captured with `--trace-vt /tmp/deepin-terminal-vt.log` showed that terminal geometry remained stable at 146 columns by 50 rows throughout the reproduction. The visible jump was therefore not caused by a resize loop.

The affected TUI wrapped its redraws in DEC synchronized output mode using `CSI ? 2026 h` and `CSI ? 2026 l`. Several frames crossed PTY reads. One frame began with a 15,439-byte read at `09:10:35.650` and ended in a 25,896-byte read at `09:10:35.718`. Other split frames remained open for 65 to 90 milliseconds.

`TerminalWidget` feeds PTY data every 8 milliseconds during bursts and immediately flushes pending data at 32 KiB. It updated the Ghostty render state after each flush without checking synchronized output mode, so an incomplete frame could become visible before the closing sequence arrived.

The upstream Ghostty renderer checks the same terminal mode and skips render-state publication while synchronized output is active. The bundled C API already exposes `GHOSTTY_MODE_SYNC_OUTPUT`, `ghostty_terminal_mode_get()`, and `ghostty_terminal_mode_set()`.

## Root Cause

The Qt wrapper processed synchronized output correctly at the VT state level but did not honor its rendering semantics. PTY chunk boundaries therefore became visible frame boundaries for large TUI updates.

## Fix

`TerminalWidget::syncRenderState()` now keeps the last completed back buffer while `GHOSTTY_MODE_SYNC_OUTPUT` is active. PTY data continues to be processed so the closing sequence can update the terminal mode. Once synchronization closes, the next paint publishes the completed render state.

A single-shot one-second timer mirrors Ghostty's recovery behavior for malformed applications. If synchronized output remains active, the wrapper clears the mode and publishes the pending frame instead of leaving the display frozen.

## Regression Coverage

Focused tests verify that:

- an incomplete synchronized frame does not update the render state;
- the completed frame is published after `CSI ? 2026 l`;
- a missing closing sequence is released by the timeout.

Run the focused coverage with:

```bash
QT_QPA_PLATFORM=offscreen ./build/tests/test_terminal_widget \
  testSynchronizedOutputDefersIncompleteFrame \
  testSynchronizedOutputTimeoutPublishesFrame
```
