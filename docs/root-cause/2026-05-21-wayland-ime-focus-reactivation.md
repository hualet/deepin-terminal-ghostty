# Wayland IME Reactivated After Terminal Focus Return

## Symptom

On Wayland, switching focus away from the terminal and then back could
occasionally trigger the Chinese input method even when the user only intended
to return keyboard focus to the terminal.

## Root Cause

`TerminalWidget` kept Qt input method support enabled and reported itself as an
active input method target whenever focus changed. On focus loss it also left
the current preedit text intact and notified the global `QInputMethod` about the
terminal input rectangle.

Wayland text input state is tied closely to surface focus. If an input method
such as fcitx or ibus still had an active composition state, the terminal could
be treated as the next text input target when focus returned.

## Fix

On terminal focus loss:

- reset the current Qt input method context;
- clear local preedit text;
- stop sending input method cursor updates for the unfocused terminal.

This cancels stale composition state before Wayland focus is restored to the
terminal surface.

## Verification

- Added `testFocusOutClearsPreeditText`, which failed before the fix because
  preedit text stayed rendered after focus loss.
- Verified the focused input method test set passes under the Qt offscreen
  platform.
