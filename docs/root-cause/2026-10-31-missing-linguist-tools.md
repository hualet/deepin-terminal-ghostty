# Debian build dependencies omitted Qt translation tools

## Evidence

After `apt build-dep .`, system CMake configuration failed at the required
`Qt6::LinguistTools` component. `dpkg-checkbuilddeps` nevertheless succeeded
with the original control file: it required `qt6-base-dev` but omitted the
translation development packages.

The Deepin 6.8.0 package contents identify the missing pieces:

- `qt6-tools-dev` supplies `Qt6LinguistToolsConfig.cmake` and imported targets.
- `qt6-l10n-tools` supplies `lrelease` and `lupdate`.
- `qt6-tools-dev-tools`, a dependency of `qt6-tools-dev`, supplies `lprodump`,
  which the imported target file also checks during configuration.

The build requires LinguistTools to compile the Chinese and Spanish catalogs.
Earlier validation used an extracted development prefix containing these tools,
so it verified compilation without detecting incomplete Debian metadata.

## Change

Declare `qt6-tools-dev` and `qt6-l10n-tools` in `debian/control`. Also declare
`pkg-config`, which CMake directly requires for the font dependencies. Update
the README and agent build prerequisites to match, including the already
declared DTK tool and font development packages.

## Verification

The system-only configure reproduced the original missing-component error.
With the corrected metadata, `dpkg-checkbuilddeps` now reports the missing Qt
packages instead of accepting this environment. `apt-get --simulate --no-remove build-dep .` resolves them and their dependencies: 14 packages added, none
removed or upgraded on this host.

The three relevant, checksum-verified Qt packages were extracted only into
`build/dependency-check-prefix`. Pointing `Qt6LinguistTools_DIR` at this prefix
allowed configuration against the system Qt/DTK/font packages, followed by a
successful `app_translations` build. No system packages were installed for this
verification. Logs are `build/dependency-check-configure.log` and
`build/dependency-check-translations.log`.

The complete application and test-binary build also succeeded with
`cmake --build build/dependency-check-fixed --parallel 4`, using the system
Qt/DTK/font development packages and only the extracted Qt tool packages above.
Evidence: `build/dependency-check-build.log`. `git diff --check` passed.
