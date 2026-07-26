# Audit: `modules/security-cryptography-random/README.md`

## Metadata

- AUDITED: 10-line Security.Cryptography.Random module README, fully read.
- Validation: dependency and platform-linkage statements were cross-checked
  against its `CMakeLists.txt` and generated component catalogue on
  2026-07-27; boundary/catalogue checks passed.

## Assessment

The README accurately describes a compiled OS-backed random-number component,
states its public `Core.Base` dependency, and correctly limits Windows
`bcrypt` linkage to the private implementation detail.  It does not contradict
the implementation reports.

## Missing assertions and diagnostics

- The entry point does not link to the documented full-domain GetInt32
  portability defect (SR-AUD-012) or communicate platform error-path coverage
  limitations; callers need the audit mirror for that evidence.
- It intentionally has no API inventory for range, non-zero, and virtual
  provider behavior.

## Final assessment

Accurate but intentionally minimal component metadata.  No new finding and no
source or test change.
