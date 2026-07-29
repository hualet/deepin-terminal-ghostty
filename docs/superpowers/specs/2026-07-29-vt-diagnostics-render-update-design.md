# VT Diagnostics and Render Update Design

## VT processing diagnostics

After each VT mutation, query
`GHOSTTY_TERMINAL_DATA_VT_PROCESSING_ERROR`. On the first sticky error, log one
warning and emit a generic library-layer signal. Expose the current flag for
diagnostics without changing best-effort rendering behavior.

## Two-phase render audit

Do not switch to `ghostty_render_state_begin_update()` /
`ghostty_render_state_end_update()` in the current architecture. PTY callbacks,
VT writes, compression, searches, and painting are all serialized on the Qt
widget thread and no terminal mutex exists. The two-phase API only shortens a
terminal lock held between an IO thread and a renderer thread; calling both
phases consecutively here is equivalent to the existing convenience call and
adds no concurrency or latency benefit.

If terminal mutation later moves off the UI thread, introduce one shared lock
and keep it only around `begin_update`; that architectural change should have
its own race and performance validation.

## Verification

Verify ordinary output leaves the diagnostic false, all render-state update
sites remain serialized, and the full render/PTY suites pass.
