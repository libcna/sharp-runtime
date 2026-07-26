# Audit: `modules/text-json/include/System/Text/Json/Utf8JsonWriter.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Text.Json`.
- Evidence: focused `SharpRuntimeTests_Text_Json` passed 147/147; direct behavior probe at `/tmp/sharp-runtime-text-json-audit/text_json_probe.cpp`; node-lifetime ASan/UBSan probe at `/tmp/sharp-runtime-text-json-audit/node_lifetime.cpp` where applicable.

## Assessment

Writer state ownership, API declarations, validation options, and raw-value contract were reviewed against the implementation and focused tests.

## Missing assertions and diagnostics

Retain malformed UTF-8, structural-state, raw-input, depth, and exact-byte escaping assertions.

## Final assessment

AUDITED; evidence is recorded at component scope.
