# Audit: `modules/core/include/System/AppContext.hpp`

## Metadata

- Audit status: AUDITED (138-line inline implementation, fully read).
- Validation: `AppContextExtraTests.*` passed 6/6 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.  The wider
  `AppContextExtraTests.*:AppDomainSetupTests.*` filter passed 11/11.
- Reproduction: `/tmp/sharp-runtimervc-appdomain-audit-probe` prints
  `context_data_switch=0:0` and `context_base_override=0` after storing a
  `"true"` string and `"/audit-base/"` under the corresponding public data
  keys.
- Reference basis: local .NET `System/AppContext.cs` (`BaseDirectory`,
  `GetData`/`SetData`, `TryGetSwitch`, and `SetSwitch`).

## SR-AUD-102 — medium — AppContext named data cannot configure BaseDirectory or compatibility switches

Current .NET first resolves `BaseDirectory` from named data key
`APP_CONTEXT_BASE_DIRECTORY`; it also has `TryGetSwitch` parse a named-data
string value when there is no explicit switch entry.  This port keeps the data
map and the switch map independent, and `getBaseDirectoryProperty` always
delegates directly to `AppDomain` (`AppContext.hpp:36`), so neither behavior is
reachable.  The direct probe stores a live `std::string` pointer for each key
and prints `context_data_switch=0:0` and `context_base_override=0`.

The public `void*` data adaptation supplies no runtime type tag or ownership,
so it cannot implement .NET's string-only BaseDirectory override or safely
recognize a string switch value.  This is not merely an absent reflection
feature: it makes the implemented public `SetData` configuration route
ineffective for two documented `AppContext` behaviors.  The direct tests cover
only map round-trip and explicit `SetSwitch` state.

## Other missing assertions and diagnostics

- Tests do not distinguish an explicit switch from a string-valued named-data
  switch, nor do they exercise `APP_CONTEXT_BASE_DIRECTORY`.
- No test records the raw-pointer lifetime/ownership adaptation, a null data
  value, replacement of an existing key, concurrent map use, or the returned
  base-directory reference lifetime.
- The always-empty `TargetFrameworkName` is an explicit unsupported-reflection
  adaptation, but no test distinguishes it from an unavailable entry assembly
  or records the limitation in a caller-visible diagnostic.

## Final assessment

Ordinary pointer and explicit-switch round trips are synchronized and covered,
but named configuration does not drive the corresponding .NET behaviors.  No
source or test was modified during this audit.
