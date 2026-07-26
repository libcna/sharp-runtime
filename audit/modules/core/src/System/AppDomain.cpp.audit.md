# Audit: `modules/core/src/System/AppDomain.cpp`

## Metadata

- Audit status: AUDITED (64-line platform-specific constructor implementation,
  fully read with its public declaration).
- Validation: `AppDomainSetupTests.*` passed 5/5 on 2026-07-26.  The direct
  AppDomain probe initializes the singleton and confirms a nonempty Linux
  executable-directory result in this build environment.
- Reference basis: local .NET `AppDomain.BaseDirectory`, which delegates to
  `AppContext.BaseDirectory`; platform executable-path APIs reviewed against
  their documented contracts.

## Assessment

The Linux implementation terminates a successful `/proc/self/exe` result
before extracting its directory, and each supported branch produces the
documented trailing separator or the explicit `"./"` fallback.  The C++
one-domain construction is thread-safe through the function-local static.
No separate source-level defect was confirmed here: the observable
BaseDirectory named-data divergence belongs to SR-AUD-102 in `AppContext.hpp`.

## Other missing assertions and diagnostics

- The five direct tests only require a nonempty application base and equality
  with AppContext.  They do not verify trailing separators, executable-location
  changes, fallback behavior, or whether a configured AppContext base directory
  is honored.
- No Linux test simulates unavailable `/proc/self/exe`; no Windows test covers
  `GetModuleFileNameW` truncation/UTF-8 conversion; and no macOS test covers
  the `_NSGetExecutablePath` resize-required result.  Cross-platform fallback
  diagnostics are therefore unverified.
- The constructor stores a process-start directory once.  No test documents
  that this differs from .NET's named-data-overridable AppContext route.

## Final assessment

The directly implemented path extraction is reasonable on the exercised Linux
path, but platform fallbacks and the higher-level configuration contract lack
dedicated diagnostics.  No source or test was modified during this audit.
