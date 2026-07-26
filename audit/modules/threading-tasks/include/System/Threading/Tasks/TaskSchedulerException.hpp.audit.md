# Audit: `modules/threading-tasks/include/System/Threading/Tasks/TaskSchedulerException.hpp`

## Metadata

- AUDITED: public exception constructors and inheritance.
- Validation: `TaskSchedulerExceptionTests.*` passed 3/3 on 2026-07-27; a
  direct C++20/current-.NET 10 probe found HResult `0x80131500` in both.

## Assessment

The public constructor surface and default diagnostic route through the base
Exception correctly.  The tested default HResult agrees with current .NET.

## Other missing assertions and diagnostics

- Add HResult, inner-exception retention, copied exception, and null-equivalent
  inner-pointer tests; existing coverage asserts text only.

## Final assessment

No new finding was confirmed.  No source or test was changed during this audit.
