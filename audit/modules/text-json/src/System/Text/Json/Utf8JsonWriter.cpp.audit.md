# Audit: `modules/text-json/src/System/Text/Json/Utf8JsonWriter.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Text.Json`.
- Evidence: focused `SharpRuntimeTests_Text_Json` passed 147/147; direct behavior probe at `/tmp/sharp-runtime-text-json-audit/text_json_probe.cpp`; node-lifetime ASan/UBSan probe at `/tmp/sharp-runtime-text-json-audit/node_lifetime.cpp` where applicable.

## Assessment

Writer escaping, structural state transitions, depth, raw-value behavior, and non-finite-number diagnostics were reviewed. Existing regressions cover the previously repaired cases.

## Missing assertions and diagnostics

Retain malformed UTF-8, structural-state, raw-input, depth, and exact-byte escaping assertions.

## Final assessment

AUDITED; evidence is recorded at component scope.
