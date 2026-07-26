# Sharp Runtime deep-audit scope

## Snapshot and purpose

This directory is an evidence-only, repository-wide audit of sharp-runtime.
The source snapshot is the clean `feature/work` checkout at the beginning of
the audit, on 2026-07-25.  Audit reports record observed behavior, parity
gaps, safety and portability risks, build/test weaknesses, and documentation
inconsistencies.  They do not change production code or weaken tests.

Each report mirrors its source path and appends `.audit.md`; for example,
`modules/collections/include/System/Collections/Generic/List.hpp` is reviewed
in `audit/modules/collections/include/System/Collections/Generic/List.hpp.audit.md`.

## In scope

Every tracked, first-party, text-like file is eligible:

- runtime headers, implementations, component tests, and component CMake
  declarations under `modules/`;
- root build files, CMake helpers, scripts, CI, consumer fixtures, integration
  tests, benchmarks, and test tooling;
- first-party Markdown documentation, including `NEXT.md`, `plan.md`, and
  `prompt.md`, where it makes checkable claims about implementation or
  validation.

The initial `git ls-files` inventory contains 1,781 paths.  After the
exclusions below, 1,748 files are audit-eligible: 1,699 under `modules/` and
49 support/build/test/documentation files elsewhere.

## Excluded files

The following are explicitly excluded from per-file audit reports:

- `vendor/**` (14 paths): unmodified third-party code or submodule pointers;
- VCS/place-holder metadata (`.gitignore`, `.gitmodules`,
  `.bitbackupignore`, and 15 `.gitkeep` files);
- `LICENSE`, whose legal wording is not a runtime implementation artifact;
- generated build output, local databases, editor state, and this `audit/`
  directory itself.  None is part of `git ls-files` at the source snapshot.

No other tracked path is excluded.  This gives the invariant
`1,748 eligible + 33 excluded = 1,781 tracked`.

## Review method

For each file, the audit records its purpose, evidence inspected, relevant
dependencies/tests, concrete findings with severity, missing assertions or
diagnostics, and a final verdict.  A report must distinguish:

- **confirmed**: directly demonstrated by source, a test, a build result, or
  an authoritative .NET reference;
- **risk**: plausible but not yet reproduced, with the exact condition that
  needs validation;
- **intentional limitation**: a behavior explicitly documented in
  `CLAUDE.md` or the relevant public header, rather than a silently incomplete
  port.

Public .NET-shaped APIs are compared with the local authoritative runtime
source at `/rv/tmp/runtime/src/libraries/` when that source exists.  C++
adaptations and the documented permanent exclusions are not reported as bugs
without a concrete behavioral contradiction.

## Reproducibility baseline

- Branch: `feature/work`, clean before audit artifacts were created.
- Toolchain observed: GCC 14.2.0, CMake 3.31.6, Doxygen 1.9.8.
- Existing configured build: `build/`, `SHARP_RUNTIME_COMPONENTS=All`, tests
  enabled.
- Claimed project baseline: 12,681 tests in 37 executables, 41 physical
  components, and 90 production dependency edges.  These claims are audited
  rather than assumed.

## Completeness check

The end-of-audit reconciliation enumerates the same `git ls-files` snapshot,
applies these rules, and requires exactly one mirrored `.audit.md` report for
every eligible path.  `AUDIT_MANIFEST.md` and `AUDIT_PROGRESS.md` record the
live roll-up; they are audit controls, not source files to be counted again.
