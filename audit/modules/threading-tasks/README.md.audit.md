# Audit: `modules/threading-tasks/README.md`

## Metadata

- AUDITED: component description and declared public dependency documentation.
- Validation: compared with `CMakeLists.txt`, component catalogue, and the
  reviewed public headers on 2026-07-27.

## Assessment

The short description and Core.Base/Threading dependency statement agree with
the module registration.  Detailed behavioral limitations properly belong in
the public header contracts rather than this catalogue-oriented README.

## Other missing assertions and diagnostics

- Link a concise supported-subset page for scheduling, await semantics,
  cancellation, and native lifetime requirements so component consumers do not
  have to discover them across individual headers.

## Final assessment

No documentation inconsistency was confirmed.  No source or test was changed
during this audit.
