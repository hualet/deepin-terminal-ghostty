# Rich Clipboard Integration

Stage 4 adds OSC 5522 MIME reads/writes and user paste events after the engine,
paste safety and Kitty rendering stages. Preserve OSC 52/1337 writes. The
existing write callback has the wrong return contract: replace its enum return
with a void callback which invokes the borrowed request's reply synchronously.

## Capability and application boundary

The library transports and validates MIME bytes, owns copied permission request
values and replies through Ghostty. Expose a generic permission provider taking
owning Qt values (operation, location, program display name, MIME data, whether
remember is available) and returning Allow/Deny/Busy plus remember. No Ghostty
request or ctx pointer escapes the synchronous callback. Default reads deny
without a provider. Writes remain allowed within the configured size ceiling.
The app installs a DTK permission dialog with bounded plain text/image previews.
Use generic terminal-program wording; do not repeat the untrusted protocol program
name as an application identity. The dialog offers
Cancel default, explicit Allow action and an unchecked session-remember box only
when the protocol permits it. Ghostty owns password-scoped grants; never trust
program display names as identity or add a global session authorization flag.
List-only reads and requests already granted by Ghostty skip the prompt.

Snapshot requested clipboard data before consent, then preview and reply with
exactly those bytes even if clipboard ownership changes. Preserve valid binary
MIME data; distinguish clear from a single empty representation. Primary and
system clipboards stay distinct. Clipboard replies must fit the PTY transport
queue or use explicit backpressure; the existing1MiB truncation behavior must
never silently corrupt a large binary reply. Enforce pointer/length, format/count and total
size bounds with checked arithmetic; set engine write limit to64MiB and bound
embedder reads. Image preview decoding needs a pixel/dimension ceiling and must
not fetch external HTML/image resources. Failed preview does not transform data.

## Synchronous callback safety

Ghostty C read/write requests and reply context are borrowed stack data. Replies
must complete inside the callback; GTK's asynchronous app API is not usable here.
A modal dialog therefore needs a scoped terminal processing gate covering the
entire outer engine operation, including post-write bookkeeping. Detach a local
QByteArray from pending PTY data before parsing so incoming events cannot
invalidate its pointer. While consent is active, pause PTY read/child poll and
engine-facing render, resize, compression, sync-output, selection and animation
work. Paint cached output and defer mutations/import/session-end delivery.
Resume processing and publish terminalIdle only after outer processing unwinds.

The app defers pane removal, tab/window close and session replacement while a
terminal is busy. QPointer alone cannot protect an executing Ghostty callback
from ancestor deletion. Use stable ids/guarded pending actions and queued idle
retries. One app-level active consent dialog prevents nested terminal prompts;
a second request returns BUSY. Test reentrant PTY output, resize, exit and control
close rather than assuming modal UI prevents non-UI events.

## Rich paste

Extend the stage2 immutable paste to advertise available MIME types through
GhosttyPaste. Keyboard/menu/middle paste uses CLIPBOARD with the proper location;
file drops remain TEXT. Provide canonical text/image/png formats when available.
Mode5522 then emits a paste event and grants its follow-up read; other programs
receive the ordinary safe text path. Advertise image-only clipboards as pasteable
for clients supporting paste events. Preserve existing unsafe paste confirmation
and snapshot semantics. Avoid eagerly encoding large images solely to advertise
an event where practical.

## Acceptance

Test OSC5522 ACK/status, binary PNG/HTML/text writes and requested MIME reads,
default denial, allowed/remembered read, password/direction/terminal isolation,
list-only query, user paste event and follow-up grants, clipboard changes during
consent, primary fallback rejection, payload bounds and nested-loop lifecycle.
Test application dialog text/image preview and controls offscreen. Update Chinese
translations and document remaining interoperability limits; do not claim native
clipboard manager or arbitrary CLI compatibility without a live test.
