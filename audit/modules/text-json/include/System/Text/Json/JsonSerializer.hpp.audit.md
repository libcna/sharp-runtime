# Audit: `modules/text-json/include/System/Text/Json/JsonSerializer.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Text.Json`.
- Evidence: focused `SharpRuntimeTests_Text_Json` passed 147/147; direct behavior probe at `/tmp/sharp-runtime-text-json-audit/text_json_probe.cpp`; node-lifetime ASan/UBSan probe at `/tmp/sharp-runtime-text-json-audit/node_lifetime.cpp` where applicable.

## SR-AUD-330 — medium — JsonSerializer Deserialize ignores its JsonSerializerOptions argument

Both Deserialize overloads explicitly discard `opts`; their nlohmann parse call has default policy and the JsonDocument overload does not forward the caller options. The probe rejects `[1,]` even with serializer `AllowTrailingCommas=true`.

## Assessment

The public declaration was reviewed against its implementation, focused tests, and the installed current .NET runtime reference surface. No additional distinct confirmed finding is assigned to this file.

## Missing assertions and diagnostics

The reviewed surface should retain exact-result, malformed-input, lifetime, and error-path assertions appropriate to its public contract.

## Final assessment

AUDITED; evidence is recorded at component scope. Confirmed finding links above require evidence-backed remediation after the audit phase.
