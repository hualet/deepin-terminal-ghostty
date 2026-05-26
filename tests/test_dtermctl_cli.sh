#!/bin/sh
set -eu

DTERMCTL="$1"

help_output="$("$DTERMCTL" --help)"
printf '%s\n' "$help_output" | grep -q "dtermctl list"
printf '%s\n' "$help_output" | grep -q "dtermctl exec --pane <pane-id> -- <command>"
printf '%s\n' "$help_output" | grep -q "current visible screen content, not full scrollback"

if "$DTERMCTL" split --pane abc >/tmp/dtermctl-split.out 2>/tmp/dtermctl-split.err; then
    echo "split without orientation unexpectedly succeeded" >&2
    exit 1
fi
grep -q "split requires --pane and --horizontal or --vertical" /tmp/dtermctl-split.err

if "$DTERMCTL" send --pane abc >/tmp/dtermctl-send.out 2>/tmp/dtermctl-send.err; then
    echo "send without text unexpectedly succeeded" >&2
    exit 1
fi
grep -q "send requires --pane and --text" /tmp/dtermctl-send.err

if "$DTERMCTL" exec --pane abc >/tmp/dtermctl-exec.out 2>/tmp/dtermctl-exec.err; then
    echo "exec without command unexpectedly succeeded" >&2
    exit 1
fi
grep -q "exec requires --pane and a command after --" /tmp/dtermctl-exec.err

if "$DTERMCTL" list >/tmp/dtermctl-list.out 2>/tmp/dtermctl-list.err; then
    echo "list without a running service unexpectedly succeeded" >&2
    exit 1
fi
grep -q "terminal control service is unavailable" /tmp/dtermctl-list.err
