# Audit: `modules/text-json/tests/System/Text/Json/Serialization/JsonSerializationTests.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Text.Json`.
- Evidence: focused `SharpRuntimeTests_Text_Json` passed 147/147; direct behavior probe at `/tmp/sharp-runtime-text-json-audit/text_json_probe.cpp`; node-lifetime ASan/UBSan probe at `/tmp/sharp-runtime-text-json-audit/node_lifetime.cpp` where applicable.

## SR-AUD-330 — medium — JsonSerializer Deserialize ignores its JsonSerializerOptions argument

Tests assert option storage/default construction only; no Deserialize test verifies parser options change acceptance, depth, comments, or duplicate-name behavior.

## Assessment

The serializer fixture passed as part of the 147-test component target and was reviewed for converter, attribute, resolver, default, and option-effect assertions.

## Missing assertions and diagnostics

Keep explicit behavioral tests for every supported non-reflection adapter and distinguish documented reflection limitations from parser-policy regressions.

## Final assessment

AUDITED; evidence is recorded at component scope. Confirmed finding links above require evidence-backed remediation after the audit phase.
