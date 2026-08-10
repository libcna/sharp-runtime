# Audit: `modules/text-json/include/System/Text/Json/Nodes/JsonValue.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Text.Json`.
- Evidence: focused `SharpRuntimeTests_Text_Json` passed 147/147; direct behavior probe at `/tmp/sharp-runtime-text-json-audit/text_json_probe.cpp`; node-lifetime ASan/UBSan probe at `/tmp/sharp-runtime-text-json-audit/node_lifetime.cpp` where applicable.

## SR-AUD-328 — medium — JsonValue integer accessors silently truncate floating JSON numbers

`GetInt32` and `GetInt64` accept any nlohmann number then call `get<int...>`. The direct probe returns `1` for JsonValue::Create(1.5), where the .NET integer conversion rejects a non-integral JSON number.

## Assessment

Primitive creation, kind checks, conversion, serialization, and clone behavior were reviewed.

## Missing assertions and diagnostics

The reviewed surface should retain exact-result, malformed-input, lifetime, and error-path assertions appropriate to its public contract.

## Final assessment

AUDITED; evidence is recorded at component scope. Confirmed finding links above require evidence-backed remediation after the audit phase.
