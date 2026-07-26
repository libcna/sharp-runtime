# Audit: `modules/core/tests/System/CultureInvariantFormattingTests.cpp`

## Metadata

- AUDITED: 91-line global-locale regression fixture, fully read.
- Validation: `CultureInvariantFormattingTests.*` passed 1/1 on 2026-07-27.
  This environment supplies `en_US.utf8`, so the test executed rather than
  taking its explicit skip path.
- Related implementation evidence: audited BitConverter, Convert, DateOnly,
  Int32, TimeSpan, and Version reports.  Their relevant stream-format paths
  explicitly call `std::locale::classic()` before formatted insertion.

## Assessment

The fixture deliberately switches the process-wide C++ locale, restores it
through RAII, and checks decimal, hexadecimal, date, duration, version,
base-conversion, and byte formatting.  It guards a real embedding-process
risk: stream formatting otherwise observes mutable global locale state unlike
the intended invariant-style output.  The selected paths preserve their
expected output under an installed non-invariant locale.  No new implementation
defect is demonstrated.

## Other missing assertions and diagnostics

- The fixture skips successfully when its four hard-coded host locales are not
  installed.  A custom `std::numpunct` facet could exercise grouping and
  decimal substitution without a host-locale package, making this regression
  deterministic in minimal CI/container images.
- It checks one representative value per path.  Negative values, decimal
  fractions, zero-padding, all supported bases, format widths, parsed
  round-trips, and non-ASCII culture symbols remain uncovered.
- The global-locale mutation is process-wide.  The RAII restoration is good,
  but no concurrent-thread stress demonstrates that unrelated formatting is
  isolated while the test changes the global locale.

## Final assessment

The fixture is a useful regression test and ran in this environment.  Its
host-locale fallback weakens portability of the assertion, but no new finding
or source/test change is warranted in this audit-only phase.
