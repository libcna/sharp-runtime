# Audit: `modules/core/tests/System/AppDomainSetupTests.cpp`

## Metadata

- Audit status: AUDITED (33 lines, 5 tests, fully read).
- Validation: `AppDomainSetupTests.*` passed 5/5 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.
- Reference basis: the port's `AppDomainSetup`/`AppContext` API and local .NET
  base-directory/target-framework semantics.

## Assessment

The fixture checks construction, a nonempty base path, two instances agreeing,
and literal agreement with `AppContext`.  These are useful smoke checks for the
one-domain adapter but all derive their oracle from the same implementation;
they do not independently establish .NET behavior.

`TargetFrameworkName_IsEmpty` turns the port's current unsupported-reflection
adaptation into a permanent expected result.  More importantly, none of the
tests exercises named AppContext data, so the suite cannot reveal the inherited
SR-AUD-102 configuration gap.

## Other missing assertions and diagnostics

- Add a dedicated expectation for the documented unsupported target-framework
  adaptation, or compare against a controlled entry-assembly configuration
  when that capability is implemented.
- Exercise `APP_CONTEXT_BASE_DIRECTORY` and prove that `ApplicationBase`
  follows its intended policy; add a non-empty, trailing-separator, and
  platform-fallback assertion instead of only self-equality.
- The suite has no independent `AppDomain::CurrentDomain`, `GetData`,
  `SetData`, `IsCompatibilitySwitchSet`, `ApplyPolicy`, or event coverage.

## Final assessment

All five happy-path tests pass, but they share the production oracle and leave
the public configuration boundary unasserted.  No test was modified during
this audit.
