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

### DESIGN-COMPLETE, APPROVAL-BOUND — #2255 (review) and #2256, 2026-08-10

`docs/CoreAppContextNamedDataDesign.md`. **Both premises of this finding reproduce
exactly as filed**, by inspection: `getBaseDirectoryProperty()` delegates straight
to `AppDomain` and never reads the data store, and `TryGetSwitch` uses a separate
map and never falls back to it. Nothing in the report needed correcting.

**Neither half has a compatible repair.** Three routes, all blocked:

| Route | Verdict |
|---|---|
| **A** — give the data store a runtime type (`std::any`, a variant, or `shared_ptr<void>` + `type_index`) | the only faithful route; a **public signature change on four members across two classes** — `AppContext::SetData`/`GetData` and `AppDomain::SetData`/`GetData`, the latter only because **#2249** made them forwarders — and it retires three existing pins |
| **B** — `reinterpret_cast` the `void*` for the two special keys | **rejected as undefined behaviour by construction.** A `void*` carries no type, so the assumption is unfalsifiable at the point of use: the correct case and the corrupting case are the same instruction sequence, and no test or sanitizer can separate them |
| **C** — add a separate typed string channel | rejected as inventing public API; .NET has one named data store, not two |

A **fourth** obstacle applies to the `BaseDirectory` half alone and route A does not
solve it: `getBaseDirectoryProperty()` returns `const std::string&`, so an override
sourced from the store would hand out a reference to caller-owned storage with no
liveness boundary — the **CCF-019** shape. CCF-019 is recorded here as an adjacency
and is **not extended**.

**#2255 (`needs_user`)** carries the single decision with its three priced options.
**#2256** landed the compatible remainder — documentation and tests only, no
behaviour change — so this finding stays **`confirmed`**, the split convention
SR-AUD-103 and SR-AUD-259 already use. **#2250 is unreachable from here**: nothing
in #2256 changes what `TryGetSwitch` returns, so nothing changes what a future
approved `AppDomain::IsCompatibilitySwitchSet` forwarding would observe.

## Other missing assertions and diagnostics

- Tests do not distinguish an explicit switch from a string-valued named-data
  switch, nor do they exercise `APP_CONTEXT_BASE_DIRECTORY`. **Addressed by #2256**,
  as two `Divergence_*` tests that make both absences observable rather than merely
  asserted; they are expected to be *rewritten* if #2255 is approved.
- No test records the raw-pointer lifetime/ownership adaptation, a null data
  value, replacement of an existing key, concurrent map use, or the returned
  base-directory reference lifetime. **All five addressed by #2256**, including a
  4-thread × 250-iteration exercise of both maps and a pin that a stored `nullptr`
  is indistinguishable from an absent key.
- The always-empty `TargetFrameworkName` is an explicit unsupported-reflection
  adaptation, but no test distinguishes it from an unavailable entry assembly
  or records the limitation in a caller-visible diagnostic. **Addressed by #2256**
  in the doc-comment and one test. The limitation stays a declared deviation under
  `CLAUDE.md`'s parity philosophy; it is genuinely indistinguishable from .NET's own
  empty result for a host with no entry assembly, so no diagnostic is added.

## Final assessment

Ordinary pointer and explicit-switch round trips are synchronized and covered,
but named configuration does not drive the corresponding .NET behaviors.  No
source or test was modified during this audit.
