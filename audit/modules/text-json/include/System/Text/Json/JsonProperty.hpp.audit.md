# Audit: `modules/text-json/include/System/Text/Json/JsonProperty.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Text.Json`.
- Evidence: focused `SharpRuntimeTests_Text_Json` passed 147/147; direct behavior probe at `/tmp/sharp-runtime-text-json-audit/text_json_probe.cpp`; node-lifetime ASan/UBSan probe at `/tmp/sharp-runtime-text-json-audit/node_lifetime.cpp` where applicable.

## SR-AUD-324 — medium — disposed JsonDocument leaves captured JsonElement values usable

A JsonProperty retains the same non-invalidated JsonElement value after its source document is disposed.

## SR-AUD-325 — medium — JsonElement raw-text and JsonProperty string contracts lose source representation

`JsonProperty::ToString()` returns only its value; the probe gives `1` for `{\"name\":1}`. Current .NET preserves the complete property source form, including name and separator.

## Assessment

The public declaration was reviewed against its implementation, focused tests, and the installed current .NET runtime reference surface. No additional distinct confirmed finding is assigned to this file.

## Missing assertions and diagnostics

The reviewed surface should retain exact-result, malformed-input, lifetime, and error-path assertions appropriate to its public contract.

## Final assessment

AUDITED; evidence is recorded at component scope. Confirmed finding links above require evidence-backed remediation after the audit phase.
