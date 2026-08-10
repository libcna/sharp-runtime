# Audit: `modules/core/include/System/AppDomainSetup.hpp`

## Metadata

- Audit status: AUDITED (43-line declaration, fully read with its dedicated
  five-test fixture).
- Validation: `AppDomainSetupTests.*` passed 5/5 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.
- Reference basis: local .NET `AppDomainSetup`/`AppDomain` base-directory and
  target-framework accessors, plus the port's documented one-domain adapter.

## Assessment

The class intentionally reduces setup information to two read-only delegating
accessors.  `ApplicationBase` consistently follows the C++ `AppContext` base
directory.  `TargetFrameworkName` is also a straight delegation, so its
unconditional empty result is the explicit missing-reflection adaptation in
`AppContext`, not an independent implementation fault.

The delegation also propagates SR-AUD-102: a named AppContext base-directory
configuration cannot affect `ApplicationBase`, whereas the .NET path reads the
AppContext setting.  The dedicated tests prove only self-consistency of the
two C++ adapters.

## Other missing assertions and diagnostics

- No test sets `APP_CONTEXT_BASE_DIRECTORY` before observing
  `ApplicationBase`, so it cannot expose the inherited configuration gap.
- The fixture treats an empty target-framework name as the expected result but
  does not state the entry-assembly/reflection limitation or differentiate it
  from a genuinely unavailable target-framework attribute.
- No test verifies returned-string independence/lifetime, process base-path
  fallback, or behavior across multiple threads.

## Final assessment

This small adapter is internally consistent but inherits the unimplemented
AppContext configuration route.  No source or test was modified during this
audit.
