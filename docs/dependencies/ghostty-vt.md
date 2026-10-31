# Bundled libghostty-vt

The shared library at `lib/libghostty-vt.so` and the entire `lib/include/ghostty`
public header tree are one dependency artifact set. Replace both from the same
upstream install tree; do not mix versions or retain obsolete headers.

## Pinned inputs

- Upstream: https://github.com/ghostty-org/ghostty, tag `v1.4.0`.
- Peeled commit: `83fb95624b0439ee660c2679b43b38c8b63769cd`.
- Library version: `0.1.0-dev+83fb95624` (the VT library has its own version).
- Official Zig: `0.17.0`, `x86_64-linux` distribution.
- Toolchain archive: https://ziglang.org/download/0.17.0/zig-x86_64-linux-0.17.0.tar.xz
- Archive SHA256: `1cbe9df9f27e6b78d14ccbca43b6703a404ef79ef1c463de901d7f088d4e2026`.
- Published archive size: 57,332,648 bytes.
- Checksum source: https://ziglang.org/download/index.json, entry
  `0.17.0.x86_64-linux`, verified before extracting the toolchain.
- Transitive dependencies: the exported upstream `build.zig.zon` and package
  manifests pin their hashes. Zig verifies the downloaded package contents.

The dependency build used temporary source and toolchain directories under `/tmp`.
These directories are disposable and may no longer exist; the upstream checkout
and the user's default Zig (`0.16.0`) were not modified.

## Rebuild

Run these commands with network access for the pinned upstream dependencies.
Choose empty temporary directories on subsequent rebuilds.

```sh
curl -fL https://ziglang.org/download/index.json -o /tmp/ghostty-1.4-zig-index.json
curl -fL https://ziglang.org/download/0.17.0/zig-x86_64-linux-0.17.0.tar.xz \
  -o /tmp/zig-x86_64-linux-0.17.0.tar.xz
printf '%s  %s\n' \
  1cbe9df9f27e6b78d14ccbca43b6703a404ef79ef1c463de901d7f088d4e2026 \
  /tmp/zig-x86_64-linux-0.17.0.tar.xz | sha256sum -c -
tar -xJf /tmp/zig-x86_64-linux-0.17.0.tar.xz -C /tmp
mkdir -p /tmp/ghostty-v1.4.0-83fb95624
git -C /home/hualet/projects/g/ghostty archive \
  83fb95624b0439ee660c2679b43b38c8b63769cd \
  | tar -x -C /tmp/ghostty-v1.4.0-83fb95624
cd /tmp/ghostty-v1.4.0-83fb95624
export ZIG_GLOBAL_CACHE_DIR=/tmp/ghostty-1.4-zig-cache
/tmp/zig-x86_64-linux-0.17.0/zig build \
  -Demit-lib-vt=true -Demit-themes=false \
  -Doptimize=fast -Dtarget=x86_64-linux-gnu.2.36 -Dcpu=baseline \
  -Dsimd=true -Dvt-features=+kitty-graphics \
  -Dlib-version-string=0.1.0-dev+83fb95624 \
  --prefix /tmp/ghostty-1.4-install -j2
```

The optimization spelling is `fast` in Zig 0.17, not `ReleaseFast`. The explicit
x86_64 baseline CPU avoids instructions specific to the build machine. The glibc
2.36 target preserves the previous artifact's maximum required glibc version.
SIMD remains enabled with upstream runtime CPU dispatch. All VT feature gates
remain at their defaults, with Kitty graphics explicitly enabled. Tmux control
mode is disabled by upstream's `GhosttyZig.zig`, which sets the VT module's
`oniguruma` option to `false`. Themes are excluded because Qt supplies its own UI
and resources. The upstream command also emits a static library; this project
bundles only the shared library and installed public headers.

For a restricted-network build, the original pinned dependency archives can be
placed in `$ZIG_GLOBAL_CACHE_DIR/p/<manifest-hash>.tar.gz`; Zig still validates
their content hashes when loading them. This build used downloaded upstream
uucode, translate-c and Aro archives alongside existing matching package archives.
Use the original archive, because Zig 0.17's standalone `zig fetch` without local
package storage can recompress an archive with an extra root directory.

## Bundled artifact verification

Measured from the installed shared library before staging:

| Property | Result |
| --- | --- |
| ELF architecture | x86-64, Linux GNU, baseline CPU build |
| Library version | `0.1.0-dev+83fb95624` |
| Optimization | `GHOSTTY_OPTIMIZE_RELEASE_FAST` (`3`) |
| SIMD / Kitty graphics / tmux control mode | enabled / enabled / disabled |
| SONAME | `libghostty-vt.so.0` |
| DT_NEEDED | `libm.so.6`, `libc.so.6` |
| Highest required glibc symbol version | `GLIBC_2.36`, unchanged from the previous artifact |
| Shared library size | 12,055,776 bytes |
| Shared library SHA256 | `8fbc84eb1ead5d249f1d4a7a89d7de5f08dbb07cc745102ccce46ff1e4f34a83` |
| Installed public headers | 35 headers |
| Header tree SHA256 | `bddc52713a32b2607ffe4050c42354aaa36f4553a1df4b0f7622f3bc16272527` |

The header tree digest hashes sorted include-relative paths, each followed by a
NUL byte and the binary SHA256 digest of that file. The whole `lib` directory was
staged from one install prefix and exchanged atomically to avoid a mixed library
and header state. The installed VT set omits the previous top-level app-embedding
`ghostty.h` and `module.modulemap`. No Qt source changes were needed for API
compatibility.

Verify the binary properties with:

```sh
readelf -h lib/libghostty-vt.so
readelf -d lib/libghostty-vt.so
objdump -T lib/libghostty-vt.so | rg GLIBC_
sha256sum lib/libghostty-vt.so
```

## Integration boundary

This update only replaces the VT engine. Qt still owns drawing, input, clipboard
presentation and application composition. Kitty image file, temporary-file and
shared-memory media remain disabled. Paste-confirmation UI, render-state image
lists/animation and rich clipboard integration are separate follow-up stages.
The existing clipboard callback acknowledgment contract is not covered by this
engine upgrade's text clipboard tests.

## Verification

The regression tests `testBundledEngineBuild` and `testEnginePasteSafety` were
added before replacing the artifacts. With the previous library they failed for
the intended reasons: version `0.1.0-dev+a4aacd918`, and a bare CR classified as
safe. Plain text, tab, LF, CRLF and bracketed-paste terminator controls behaved as
expected. The version test also verifies fast optimization, SIMD and Kitty
support, and disabled tmux control mode.

The existing OSC 7501 support-query test was updated for the expanded upstream
reply (`states=...:kinds=...`). Its child now reads through the BEL terminator
instead of assuming a nine-byte response, then verifies the complete capability
list. The old expectation failed against the new engine before this test-only
protocol migration; no production callback change was required.

Qt tests need a writable home and a deterministic shell prompt: `/bin/sh`'s usual
prompt contaminates output assertions, while an empty prompt starves the shell
readiness test. A nonprinting reset sequence supplies a prompt without changing
screen contents. MainWindow tests require visible prompt text to detect a ready
background shell, so run those with a normal prompt in a separate CTest invocation:

```sh
cmake -B build -DBUILD_TESTING=ON
cmake --build build -j2
mkdir -p /tmp/ghostty-1.4-test-home
env HOME=/tmp/ghostty-1.4-test-home SHELL=/bin/sh \
  PS1="$(printf '\033[0m')" PS2='' QT_QPA_PLATFORM=offscreen \
  ctest --test-dir build -R '^TerminalWidget$' --output-on-failure
env HOME=/tmp/ghostty-1.4-test-home SHELL=/bin/sh \
  PS1='$ ' PS2='' QT_QPA_PLATFORM=offscreen \
  ctest --test-dir build -E '^TerminalWidget$' --output-on-failure
```

The focused integration run passed all 27 selected test cases, including engine
identity/safety, compression, Kitty image caches, input, text clipboard, scrollback
and restored-session state.

The complete CTest suite passed across these two prompt environments: the
TerminalWidget entry passed all 196 cases (19.45 s), and the other 11 entries
passed (29.59 s). The MainWindow binary skips its existing session-D-Bus-dependent
case when no session bus is available. The full app and all test binaries built
successfully; changed C++ passes clang-format, and `git diff --check` is clean.
