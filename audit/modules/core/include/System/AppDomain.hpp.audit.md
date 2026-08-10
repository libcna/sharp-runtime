# Audit: `modules/core/include/System/AppDomain.hpp`

## Metadata

- Audit status: AUDITED (333-line public header, fully read with its
  implementation and setup consumer).
- Validation: no dedicated `AppDomain` fixture exists.  The adjacent
  `AppContextExtraTests.*:AppDomainSetupTests.*` filter passed 11/11 on
  2026-07-26, and `/tmp/sharp-runtimervc-appdomain-audit-probe` reproduces the
  data/switch and policy results below.
- Reference basis: local .NET `System/AppDomain.cs`, especially
  `GetData`/`SetData`, `IsCompatibilitySwitchSet`, `ApplyPolicy`, and the
  event accessors.

## SR-AUD-103 — medium — AppDomain discards public data and switch state instead of forwarding it to AppContext

Current .NET implements `AppDomain.GetData` and `SetData` as direct
`AppContext` forwarding calls, and `IsCompatibilitySwitchSet` forwards to
`AppContext.TryGetSwitch` while preserving an unset `bool?` result.  This
header instead makes `SetData` a no-op, `GetData` always return `nullptr`, and
`IsCompatibilitySwitchSet` always return `false` (`AppDomain.hpp:177-180,
260-262`).  The result is observable in the direct probe: after a successful
`AppContext` data round trip it prints `domain_reads_context=0`, after
`AppDomain::SetData` it prints `domain_roundtrip=0`, and an explicit true
AppContext switch prints `domain_switch=0`.

The C++ `bool` return type also cannot expose the .NET distinction between an
unset switch and a switch explicitly set false.  The nearby tests exercise
only `AppContext` itself and `AppDomainSetup`; no test invokes any of these
public `AppDomain` members.

## SR-AUD-104 — medium — ApplyPolicy accepts invalid assembly identity strings that .NET rejects

The local .NET implementation rejects null-or-empty `assemblyName` and a
leading NUL before returning a valid identity unchanged.  The C++ method
blindly returns every `std::string` (`AppDomain.hpp:152-154`).  Its string
reference adaptation makes null unrepresentable, but empty and embedded/leading
NUL input remain valid C++ arguments and must still be validated.  The direct
probe prints `empty_policy_length=0` and `nul_policy_length=2` rather than
observing a deterministic `ArgumentException` equivalent.

## Other missing assertions and diagnostics

- The four public event add/remove pairs are silent no-ops.  Their comments
  disclose the stub status, but callers receive successful registration with
  no unavailable-feature diagnostic; no test covers registration, removal, or
  delivery policy.
- No test covers singleton identity across threads, friendly-name derivation,
  `ToString`, null/empty policy input, data forwarding, switch true/false/unset
  states, or the documented non-unloadable domain behavior.
- `RelativeSearchPath`, `DynamicDirectory`, shadow-copy, and obsolete path APIs
  are explicit unsupported stubs.  Their caller-visible no-op policy needs an
  API-baseline decision if this surface is intended to be more than compile
  compatibility.

## Final assessment

The singleton and simple constant properties are coherent with a one-domain
port, but data/configuration methods silently lose state and policy validation
is incomplete.  No source or test was modified during this audit.
