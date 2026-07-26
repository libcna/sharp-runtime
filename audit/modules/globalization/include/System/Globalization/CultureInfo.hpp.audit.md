# Audit: `modules/globalization/include/System/Globalization/CultureInfo.hpp`

## Metadata

- Audit status: AUDITED.
- Reference: current [.NET CultureInfo.CurrentCulture](https://learn.microsoft.com/en-us/dotnet/api/system.globalization.cultureinfo.currentculture?view=net-10.0)
  is a per-current-thread/task setting.
- Validation: a two-thread standalone TSan probe reports a race on global
  `CultureInfo::currentCulture_`; its ordinary run records 84,379 cross-thread
  observations in 100,000 iterations per worker.

## Assessment

Both current-culture values are mutable inline process globals.  Setters copy
nontrivial strings and format state without synchronization, while getters
return references to the same concurrently mutable objects.  This is neither
per-thread/task behavior nor a memory-safe shared configuration mechanism.

### SR-AUD-280 — high — CurrentCulture and CurrentUICulture are racy process-global state instead of per-thread/task values

The TSan trace names writes in `setCurrentCultureProperty` and conflicting
access to `currentCulture_`.  A worker configured for one culture observes the
other worker's value.  Equivalent UI-culture storage has the identical shape.
Current .NET documents the properties as the culture used by the current thread
and task-based asynchronous operations.

## Finding references

- SR-AUD-280 — high — data race, reference invalidation risk, and wrong scope
  for public culture state.

## Other missing assertions and diagnostics

- Add two-thread isolation tests for both values and TSan coverage for getter,
  setter, and formatting reads.
- Test invalid culture names and LCIDs against a declared supported-culture
  policy instead of silently inventing name-derived metadata.

## Final assessment

SR-AUD-280 applies.
