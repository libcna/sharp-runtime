# Audit: `modules/text-json/include/System/Text/Json/JsonDocumentOptions.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Text.Json`.
- Evidence: focused `SharpRuntimeTests_Text_Json` passed 147/147; direct behavior probe at `/tmp/sharp-runtime-text-json-audit/text_json_probe.cpp`; node-lifetime ASan/UBSan probe at `/tmp/sharp-runtime-text-json-audit/node_lifetime.cpp` where applicable.

## SR-AUD-326 — medium — JsonDocument parsing flags are exposed but not applied

The documented trailing-comma and duplicate-property controls are absent from the sole parser invocation. Current .NET projects them into reader options and separately passes duplicate-property policy.

## Assessment

The public declaration was reviewed against its implementation, focused tests, and the installed current .NET runtime reference surface. No additional distinct confirmed finding is assigned to this file.

## Missing assertions and diagnostics

Keep construction validation separate from tests demonstrating that each exposed option changes the consuming operation.

## Final assessment

AUDITED; evidence is recorded at component scope. Confirmed finding links above require evidence-backed remediation after the audit phase.
