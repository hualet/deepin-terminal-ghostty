# Bare Link Underline Misaligned After Wide Glyphs

## Symptoms

After a bare protocol link (e.g. `https://example.com`) was recognized on a
row that also contained wide glyphs (CJK characters, emoji), the hover
underline was drawn shifted left, under the preceding wide glyphs instead of
under the link text. Clicking still opened the correct URL.

## Root Cause

Bare-link detection runs on the concatenated row text produced by
`textForScreenRow()`, which iterates terminal **cells** and appends each
cell's graphemes. Wide glyphs occupy two cells but contribute graphemes only
to the head cell (the spacer cell yields nothing), and supplementary-plane
codepoints append a surrogate pair. The resulting string index therefore
diverges from the cell column whenever such characters appear.

`scanBareLinksInRow()` returned `LinkRange.startCol` / `endCol` as **string
indices** taken straight from that text. Both consumers, however, treated
those fields as **cell columns**:

- `renderOverlays()` computed the underline x from
  `m_hoverLink.startCol * m_cellWidth`.
- `bareLinkRangeAtCell()` compared the clicked cell column against the same
  fields.

Because click hit-testing merely checked `col >= startCol && col < endCol`,
an offset range could still contain the real cell column, so clicks kept
working. The underline, drawn from the smaller string-index-based column,
landed too far left.

## Fix

Build a parallel `cellOfChar` mapping (QChar index -> cell column) alongside
the row text in a new `textForScreenRowWithCellMap()` helper, cache it in
`LinkScanCacheEntry`, and pass it to `scanBareLinksInRow()`. The scanner now
translates the scheme start and the exclusive end through that mapping before
constructing the `LinkRange`, so the stored columns are real cell columns.
`textForScreenRow()` delegates to the new helper, preserving its existing
behavior for selection and search.

OSC 8 hyperlinks were already correct because their range is expanded by
querying each viewport cell directly.

## Verification

- Added `testBareLinkUnderlineAlignsWithLinkAfterWideCharacters`: after
  `中文 https://example.com`, the hovered range columns are 5..24 (cell
  columns), not 3..22 (string indices).
- Added `testBareLinkUnderlinePixelsFollowLinkAfterWideCharacters`: the
  rendered underline pixels fall within the URL's cell x-range.
- Confirmed both tests fail when the scanner is reverted to string indices
  and pass with the fix.
- Full `TerminalWidget` suite passes under `QT_QPA_PLATFORM=offscreen`.
  Pre-existing unrelated failures in `MainWindow`
  (`testGotoTabShortcutSwitchesInVerticalMode`) and `DtermctlCli` reproduce
  on the clean tree and are independent of this change.
