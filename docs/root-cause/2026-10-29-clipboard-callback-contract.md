# Clipboard callback reply and modal processing lifetime

## Evidence

The pinned Ghostty 1.4 clipboard write callback returns `void`. The old Qt
callback returned a result enum without invoking the request's `reply` function.
OSC 52/1337 writes appeared to work because those protocols have no write ACK,
but OSC 5522 writes changed the clipboard without returning DONE. A regression
using the upstream begin/data/commit fixture reproduced the missing ACK before
the implementation changed. A list request also reproduced the missing read
integration.

Both read and write requests, their reply functions and reply context are
borrowed for the callback's duration. Deferring their use to an asynchronous Qt
callback is invalid. A synchronous application permission provider can run a
modal event loop, which introduces a second hazard: PTY data, timers and close
actions may reenter or destroy the engine executing the original callback.

Reentrancy starts before the permission provider: custom
`QMimeData::retrieveData()` may dispatch events while acquiring a snapshot.
`QClipboard::dataChanged` observers and the synchronous reply's `dataWritten`
observers can do the same. A consent-only guard left all three windows open.
A synchronous title effect with an existing selection also reproduced access
through export and copy outside the clipboard callback. Paste enqueue observers
likewise ran while the widget's paste stack was active.

An independent owner-replacement probe reproduced SIGSEGV when a virtual
`retrieveData()` replaced the clipboard, immediately destroyed its old
`QMimeData`, and the read loop accessed that pointer for the next MIME type.
The same ownership hazard exists in `formats()` and `hasFormat()` chains.
The busy engine guard cannot keep a clipboard-owned QObject alive. Each helper
now observes a `QPointer` after every virtual MIME call and a destroyed source
aborts the whole reply with BUSY and no content. Regressions cover all three
replacement points. Replacing the permission provider from within its own
callback also destroyed the executing callable's captures; invoking a local
provider copy now keeps that target alive until its call returns.

The initial geometry must be applied before parsing a queued VT batch. Otherwise
the first viewport update can describe the old 80-column terminal, with the
queued idle resize reflowing it to the widget's actual geometry after the caller
has scrolled. The existing scrollbar regression reproduced the offset changing
from 91 to 142 after the next output batch. Pending geometry now applies inside
the outer busy scope before parsing, and its viewport observers are covered by
a regression.

The legacy PTY write method also truncates its pending queue at 1 MiB. Ghostty
encodes an entire clipboard read response and sends it in one write callback;
therefore a valid multi-megabyte image response could previously be truncated.
The bulk enqueue path also inherited the low-level flush result: EAGAIN
returned false even though all bytes remained queued. Its return value now
reports queue acceptance; backpressure resumes through the write notifier,
while a hard close or queue-budget rejection remains a failure.

## Change

Writes synchronously reply with the correctly sized result object. MIME writes
are assembled before changing the clipboard and bounded to 64 MiB. Reads copy
only requested MIME representations into owning Qt values before asking the
application for permission, and reply with that same snapshot. Reads deny by
default; list-only and Ghostty-granted requests bypass consent. Canonical text
fallback applies only to `text/plain`; unavailable lookalike MIME names do not
receive manufactured text data. Remembered password grants remain owned by Ghostty. Unsupported primary selection never
falls back to the standard clipboard.

The outer VT flush owns detached pending bytes. The busy gate covers the
complete outer parse/import/paste operation, including reply and post-processing. Clipboard callback scope additionally guards snapshot
acquisition, provider execution and synchronous reply; public APIs, event and
timer entrypoints consult the outer busy state. Internal post-write helpers may
query the engine only after the parser returns. Modal consent pauses PTY reads
and child polling, blocks engine render/resize/compression and input entrypoints,
and paints the cached buffer.
Session exit and supported application close/restore paths defer until queued
terminal idle delivery. The app installs a DTK dialog with plain-text and bounded
PNG previews, default Cancel and optional unchecked Remember. A shared active
dialog guard returns BUSY for a second terminal's consent request.

Clipboard output uses a bounded 24 MiB all-or-nothing PTY queue. Read replies
preflight a conservative encoded-size estimate; oversized requests return BUSY
before producing data. Paste output is collected locally and enqueued once;
ordinary keyboard writes retain their existing small queue policy.

## Verification and limits

Focused offscreen regressions cover write ACK, list-only reads, exact binary
bytes, requested-MIME filtering, a decoded 2 MiB response, a 17 MiB read rejected
with BUSY before DATA, remembered passwords and terminal isolation, unsupported
primary selection, read/write direction isolation, invalid read pointers/counts
and MIME names, PNG/HTML/binary writes, empty representation versus clear,
immutable consent snapshots, and a paste event whose list query preserves its
one-time follow-up grant. Additional tests cover reentrant
MIME acquisition, clipboard change and reply observers, synchronous title effects
with an existing selection, paste enqueue observers, deferred session close and
actual child exit, image-only standard and simulated primary paste, and data
arrival during consent, replacement of clipboard ownership/provider callbacks,
and bulk queue acceptance under PTY backpressure.

Application tests inspect literal plain text, bounded PNG preview, default
Cancel, unchecked Remember only when available, BUSY for a second terminal's
nested request, and pane/window close requested during consent.

The final host build and all 12 offscreen CTest entries passed in isolated
homes and fresh session buses. Project-wide formatting and diff checks passed.
Evidence is recorded in `build/stage4-final-*-ctest.log` and the dated
implementation plan. No native clipboard manager, Wayland/X11
primary-selection integration or arbitrary CLI interoperability has been verified
by these offscreen tests. The primary MIME test uses the same protected source
seam as the primary-text test; it verifies routing, not platform clipboard support.
