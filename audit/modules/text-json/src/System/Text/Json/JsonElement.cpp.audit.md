# Audit: `modules/text-json/src/System/Text/Json/JsonElement.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Text.Json`.
- Evidence: focused `SharpRuntimeTests_Text_Json` passed 147/147; direct behavior probe at `/tmp/sharp-runtime-text-json-audit/text_json_probe.cpp`; node-lifetime ASan/UBSan probe at `/tmp/sharp-runtime-text-json-audit/node_lifetime.cpp` where applicable.

## SR-AUD-324 — medium — disposed JsonDocument leaves captured JsonElement values usable

All out-of-line accessors call `require`, but require only checks value kind and never document lifetime. Captured values therefore remain readable after disposal.

## SR-AUD-325 — medium — JsonElement raw-text and JsonProperty string contracts lose source representation

The source implementation builds child aliasing shared_ptr values over a parsed DOM, so it has no original token spans from which GetRawText or property rendering could be exact.

## Assessment

Out-of-line DOM accessors, numeric conversion, child aliasing, and object enumeration were reviewed.

## Missing assertions and diagnostics

The reviewed surface should retain exact-result, malformed-input, lifetime, and error-path assertions appropriate to its public contract.

## Final assessment

AUDITED; evidence is recorded at component scope. Confirmed finding links above require evidence-backed remediation after the audit phase.
