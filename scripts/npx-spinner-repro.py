#!/usr/bin/env python3
"""Reproduce npm/npx's terminal loading spinner without dependencies."""

import argparse
import sys
import time


FRAMES = ("⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏")
FRAME_INTERVAL_SECONDS = 0.080
START_DELAY_SECONDS = 0.200
CLEAR_AND_HOME = b"\x1b[1G\x1b[K"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("duration", nargs="?", type=float, default=5.0, help="spinner duration in seconds")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.duration < 0:
        raise SystemExit("duration must not be negative")

    output = sys.stderr.buffer
    sys.stderr.write("npx: installing the requested packages...\n")
    sys.stderr.flush()
    time.sleep(START_DELAY_SECONDS)

    if args.duration == 0:
        sys.stderr.write("done\n")
        return 0

    started_at = time.monotonic()
    deadline = started_at + args.duration
    frame_index = 1
    output.write(FRAMES[frame_index].encode())
    output.flush()

    next_frame = started_at + FRAME_INTERVAL_SECONDS
    while time.monotonic() < deadline:
        now = time.monotonic()
        if now < next_frame:
            time.sleep(next_frame - now)
            continue

        frame_index = (frame_index + 1) % len(FRAMES)
        output.write(CLEAR_AND_HOME + FRAMES[frame_index].encode())
        output.flush()
        next_frame += FRAME_INTERVAL_SECONDS

    output.write(CLEAR_AND_HOME)
    output.flush()
    sys.stderr.write("done\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
