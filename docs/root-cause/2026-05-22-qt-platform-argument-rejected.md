# Qt Platform Argument Was Rejected

## Symptom

Running the application with a standard Qt platform argument failed before Qt
could process it:

```text
deepin-terminal-ghostty -platform offscreen --help
Failed to parse command line: Unknown options: p, l, a, t, f, o, r, m.
```

## Root Cause

`main()` parsed startup options before constructing `DApplication`. Qt consumes
standard arguments such as `-platform` during application construction, but our
`QCommandLineParser` saw the raw argument first.

Because the parser uses `ParseAsCompactedShortOptions`, `-platform` was treated
as compact short options and reported as unknown single-letter options.

## Fix

Construct `DApplication` before parsing `StartupOptions`. Qt removes its own
arguments from `argc` and `argv`, then the application parser handles only the
remaining terminal-specific options.

## Verification

- Added a startup integration check for `-platform offscreen --help`.
- Verified the startup command integration test passes.
