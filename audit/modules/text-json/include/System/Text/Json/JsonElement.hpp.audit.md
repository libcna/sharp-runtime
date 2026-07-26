# Audit: `modules/text-json/include/System/Text/Json/JsonElement.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Text.Json`.
- Evidence: focused `SharpRuntimeTests_Text_Json` passed 147/147; direct behavior probe at `/tmp/sharp-runtime-text-json-audit/text_json_probe.cpp`; node-lifetime ASan/UBSan probe at `/tmp/sharp-runtime-text-json-audit/node_lifetime.cpp` where applicable.

## SR-AUD-324 — medium — disposed JsonDocument leaves captured JsonElement values usable

The element owns an aliasing shared_ptr to nlohmann data rather than observing document disposal, so all inline accessors remain usable after `JsonDocument::Dispose`.

## SR-AUD-325 — medium — JsonElement raw-text and JsonProperty string contracts lose source representation

`GetRawText()` calls `node_->dump()`, which cannot preserve original whitespace, exponent spelling, or escape form. The probe converts `1e+01` to `10.0` and `\"\\u0061\"` to `\"a\"`.

## Assessment

The public declaration was reviewed against its implementation, focused tests, and the installed current .NET runtime reference surface. No additional distinct confirmed finding is assigned to this file.

## Missing assertions and diagnostics

The reviewed surface should retain exact-result, malformed-input, lifetime, and error-path assertions appropriate to its public contract.

## Final assessment

AUDITED; evidence is recorded at component scope. Confirmed finding links above require evidence-backed remediation after the audit phase.
