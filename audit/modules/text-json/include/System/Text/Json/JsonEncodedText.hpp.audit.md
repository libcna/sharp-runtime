# Audit: `modules/text-json/include/System/Text/Json/JsonEncodedText.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Text.Json`.
- Evidence: focused `SharpRuntimeTests_Text_Json` passed 147/147; direct behavior probe at `/tmp/sharp-runtime-text-json-audit/text_json_probe.cpp`; node-lifetime ASan/UBSan probe at `/tmp/sharp-runtime-text-json-audit/node_lifetime.cpp` where applicable.

## SR-AUD-329 — medium — JsonEncodedText accepts malformed UTF-8 bytes unchanged

The narrow-string Encode overload merely copies bytes despite the class contract calling the result validated UTF-8/JSON text. The direct probe accepts `C3 28` and retains two invalid bytes; a port must reject or replace malformed UTF-8 consistently with its documented text contract.

## Assessment

The public declaration was reviewed against its implementation, focused tests, and the installed current .NET runtime reference surface. No additional distinct confirmed finding is assigned to this file.

## Missing assertions and diagnostics

The reviewed surface should retain exact-result, malformed-input, lifetime, and error-path assertions appropriate to its public contract.

## Final assessment

AUDITED; evidence is recorded at component scope. Confirmed finding links above require evidence-backed remediation after the audit phase.
