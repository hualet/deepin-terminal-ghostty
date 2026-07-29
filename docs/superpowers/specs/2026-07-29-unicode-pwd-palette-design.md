# Unicode, PWD, and Palette Integration Design

## Goal

Use Ghostty's new semantic helpers where Qt-side approximations can disagree
with terminal state.

## Unicode width

Convert IME preedit text to Unicode scalar values and segment it with
`ghostty_unicode_grapheme_width()`. Size the preedit overlay in terminal cells,
so VS15/VS16, combining marks, ZWJ emoji, and East Asian wide characters use
the same width rules as committed terminal text.

## Working directory

Register `GHOSTTY_TERMINAL_OPT_PWD_CHANGED`. Decode local `file://` OSC 7
values and absolute bare paths from OSC 9/1337. Prefer this terminal-reported
directory over `/proc` process inspection, but reject non-local file URI hosts
and retain the existing PTY fallback when no usable report exists.

## Palette

Preserve each theme's first 16 ANSI colors and use
`ghostty_color_palette_generate()` to derive indices 16–255 from the theme
background and foreground. Enable harmonious generation so light themes keep
their own background-to-foreground direction.

## Verification

Cover grapheme widths, OSC 7 decoding/local-host policy, fallback behavior, and
generated palette endpoints. Then run existing IME, session-split, theme, and
rendering tests.
