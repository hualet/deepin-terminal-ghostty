# Pane Divider Theme Blending Root Cause

## Summary

Split-pane dividers could look like bright white lines on dark terminal themes even though their stylesheet
used a low-alpha white tint. The tint was composited against the `QSplitter` handle's palette background,
which was not guaranteed to match the terminal background.

## Investigation Notes

Pane and sidebar splitters both used a one-pixel handle with one of two styles:

- white at 8% alpha for dark terminal themes
- black at 8% alpha for light terminal themes

The style tracked only the theme polarity. It did not receive the terminal theme's actual background color.
The existing regression test checked only that the expected stylesheet string had been assigned.

An offscreen Qt rendering probe showed that the alpha itself worked: white at 8% over an explicit
`rgb(32,32,32)` background rendered near `rgb(49,49,49)`. A DTK splitter whose palette backing remained
light rendered the same nominal dark-theme handle near white instead. This reproduced the visual failure
mechanism and showed that lowering the alpha alone would not make the result reliable.

## Root Cause

The divider color was a translucent foreground tint rather than a final theme color. Because a splitter
handle occupies its own widget area, Qt composited that tint over the handle's native or DTK palette backing,
not over the adjacent `TerminalWidget` pixels. Palette timing, custom terminal themes, and transparent-window
composition could therefore make the divider much brighter than intended.

The test boundary stopped at the stylesheet text and did not inspect the rendered handle pixel, so it could
not detect a wrong composition background.

## Fix

`TermPane` now mixes the active terminal background and foreground at an 8% ratio and writes the resulting
opaque RGB color into the splitter stylesheet. Existing pane splitters are refreshed when the terminal theme
changes, and the computed style is cached so newly created splitters use the same color.

The vertical-tabs/sidebar splitter is styled separately from the DTK application palette's `Window` and
`WindowText` roles. This keeps application chrome aligned with DTK while pane dividers follow the selected
terminal color scheme.

## Regression Coverage

`TestMainWindow::testPaneDividerColorsFollowTheme` now renders a real `QSplitter` whose own background is
deliberately different from the requested terminal theme. It verifies that the handle pixel is the expected
opaque pre-blended color and that existing pane splitters receive the same style.

## Verification

- The focused divider test passed with Qt 6.8 on the offscreen platform.
- The complete application build passed.
- A real X11 application run used `dtermctl` to create a Carbonfox split. The captured terminal background was
  `#161616` and the divider was `#282828`, matching the expected 8% foreground blend without a white stripe.
- Full CTest completed with 9 of 11 test targets passing. Three existing `TerminalWidget` assertions and one
  existing `MainWindow` command-status assertion failed independently of this change. The `TerminalWidget`
  binary was unchanged, and the `MainWindow` failure reproduced from a clean `b32778c` source snapshot.
