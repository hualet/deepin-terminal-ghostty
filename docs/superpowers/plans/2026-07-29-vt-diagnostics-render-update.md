# VT Diagnostics and Render Update Implementation Plan

**Goal:** Surface terminal semantic failures and adopt render APIs only where
they improve the current architecture.

- [x] Add a normal-output regression for the VT processing error flag.
- [x] Poll the sticky flag after VT mutations and report only the first error.
- [x] Audit all render-state update and terminal mutation call sites.
- [x] Record why consecutive two-phase calls provide no benefit without a
  terminal lock or separate IO thread.
- [x] Run rendering, PTY, and complete test suites plus formatting checks.

No commit is created unless the user explicitly requests one.
