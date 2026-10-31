# Terminal Paste Protection

Stage 2 of the approved Ghostty adoption sequence uses Ghostty's state-aware
paste API for clipboard, primary-selection and file-drop input. Keyboard and
IME input are not pasted data and keep their existing paths. No terminal media
permissions or rich clipboard reads are enabled in this stage.

`TerminalWidget` owns a generic pending paste with a monotonically increasing
request id and an immutable UTF-8 snapshot. It first calls
`ghostty_terminal_paste()` with `allow_unsafe=false`. Safe input is encoded and
written through the existing PTY callback; a rejected input writes no bytes and
emits `pasteConfirmationRequested(id, preview)`. The application owns a nonmodal
DTK confirmation dialog with a plain text preview. Cancel is the default.
Approving calls `confirmPaste(id, true)`, which pastes that same snapshot with
`allow_unsafe=true`; cancel, stale ids and destroyed sessions write nothing.
There is at most one pending paste per terminal. A new request replaces the
previous one, and the application replaces its old dialog as needed.

This avoids modal event-loop reentrancy inside Ghostty. Reusing the terminal
paste API also strips unsafe control bytes and handles bracketed framing rather
than writing raw clipboard bytes. File paths remain shell-quoted, but a filename
containing CR/LF must still pass the same confirmation policy. The library owns
no menu, dialog or DTK code. Restoring a terminal or closing its session clears
pending authorization.

Tests cover bare CR/LF and CRLF without bracketed paste, ordinary multiline
bracketed paste, an embedded bracketed-paste terminator, confirmation/cancellation,
clipboard changes while pending, stale ids, middle click, file-drop filenames,
and application dialog wiring. Use an idle explicit child command so live shell
prompts or input echo cannot affect assertions. Record the pre-existing unsafe
paste paths and root cause in docs/root-cause.
