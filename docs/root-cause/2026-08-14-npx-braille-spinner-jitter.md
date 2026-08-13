# Npx Braille Spinner Jitter

## Symptom

The loading indicator shown by npm/npx appeared to pulse or jitter even though
it remained in the same terminal cell. The same ten Braille Pattern frames are
also used by tools such as Kimi:

```text
⠋ ⠙ ⠹ ⠸ ⠼ ⠴ ⠦ ⠧ ⠇ ⠏
```

## Minimal Reproduction

`scripts/npx-spinner-repro.py` reproduces npm's behavior without a network
request or JavaScript dependency. After npm's 200 ms initial delay it writes
the same first frame (`⠙`), then advances every 80 ms. Subsequent frames use
npm's update sequence on stderr:

```text
CSI 1 G  CSI K  <UTF-8 Braille frame>
```

The script clears the spinner line when it exits.

## Investigation

The PTY, VT state, and repaint cadence were stable:

- Two instrumented 10-second runs received 124 frame transitions with a mean
  interval of 80.00 ms and standard deviations of 0.57 ms and 0.77 ms.
- Ghostty reported one dirty row for each frame. `TerminalWidget` rendered that
  row in about 1 ms; no partial glyph frame was observed in a 90 fps capture.
- The fixed application run received 63 frames at a mean interval of 80.00 ms
  with a 0.79 ms standard deviation. The two startup resizes completed before
  the first spinner frame, with no resize during the animation.

The unstable geometry was introduced by commit `881c309` after the vendored
Ghostty update. That change correctly fitted overflowing single-codepoint
Unicode glyphs such as `※` into one terminal cell, but its fit scale and centered
baseline came from the current glyph's ink bounding box.

Braille animation frames contain different dot combinations, so their ink
bounds differ. The regression test measured a per-dot painted area of 9 pixels
for eight npm frames but 16 pixels for `⠸` and `⠇`: a 78% change even though
every dot belongs to the same font and terminal cell. Input timing was smooth,
but the renderer made those two frames visibly larger.

## Root Cause

`TerminalWidget::renderRow()` treated every Braille frame as an independent
overflowing glyph. It selected a new font size and baseline from each frame's
partial dot pattern. The animation therefore mixed stable terminal-grid
positioning with unstable per-frame glyph fitting.

## Fix

Braille Patterns U+2800 through U+28FF now use U+28FF, the all-eight-dots cell,
as their shared fitting reference. The renderer derives one scale, horizontal
position, and baseline from that complete cell, then draws the requested frame
with those values.

This preserves both required properties:

- every Braille frame keeps identical dot size and baseline;
- a fallback Braille font wider than the configured terminal font remains
  fitted and clipped to one terminal cell.

The generic overflow fitting for `※`, Block Element behavior, ASCII sizing, and
custom emoji fallback are unchanged.

## Regression Coverage

`testBrailleSpinnerKeepsStableDotGeometry` renders npm's exact ten frames at the
same cell and compares each one with an all-dots reference. It fixes the test
font and verifies that setup actually exercises the overflow fitter. Every
frame must stay on the reference dot grid with the expected per-dot area and no
pixels in the following cell. Logical cell coordinates are converted to image
pixels, so the check also remains valid when the device pixel ratio is not 1.

The initial regression exposed the size difference as:

```text
Braille spinner dot area changes across frames:
900,900,900,1600,900,900,900,900,1600,900
```

The hardened regression also fails against the old implementation because its
first frame moves nine pixels outside the all-dots reference grid. After
shared-reference fitting it passes. A real-window capture of the fixed
`deepin-terminal-ghostty` binary also shows all ten frames with stable dot
geometry and no intermediate partial frame.

The focused regression also passes with Qt scale factors 1, 1.5, and 2. The
full `TerminalWidget` binary reports 181/182: its sole failure is the existing
`testZoomedAsciiNotReshapedPerCharacter` baseline, which fails identically
before this change. Full CTest reports 9/11 suites because it includes that
failure and because `DtermctlCli` expects no running desktop service while a
user terminal service is active. Neither failure enters the Braille rendering
path.

## Verification

```bash
cmake -B build -DBUILD_TESTING=ON
cmake --build build

QT_QPA_PLATFORM=offscreen ./build/tests/test_terminal_widget \
  testBrailleSpinnerKeepsStableDotGeometry \
  testOverflowingSingleCodepointGlyphFitsCell \
  testKimiBlockLogoKeepsCellFillingGeometry

./build/deepin-terminal-ghostty \
  --trace-vt /tmp/npx-spinner.trace \
  --execute "$PWD/scripts/npx-spinner-repro.py 5" \
  --wait-for-child
```
