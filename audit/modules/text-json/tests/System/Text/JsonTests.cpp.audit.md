# Audit: `modules/text-json/tests/System/Text/JsonTests.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Text.Json`.
- Evidence: focused `SharpRuntimeTests_Text_Json` passed 147/147; direct behavior probe at `/tmp/sharp-runtime-text-json-audit/text_json_probe.cpp`; node-lifetime ASan/UBSan probe at `/tmp/sharp-runtime-text-json-audit/node_lifetime.cpp` where applicable.

## SR-AUD-324 — medium — disposed JsonDocument leaves captured JsonElement values usable

Only getRootElementProperty is checked after Dispose. A root/property captured before Dispose is not checked and currently remains usable.

## SR-AUD-325 — medium — JsonElement raw-text and JsonProperty string contracts lose source representation

The fixture asserts parsed values and ordering, not source-preserving raw text or complete JsonProperty::ToString output.

## SR-AUD-326 — medium — JsonDocument parsing flags are exposed but not applied

MaxDepth is tested, but no test proves AllowTrailingCommas or AllowDuplicateProperties changes parsing behavior.

## SR-AUD-329 — medium — JsonEncodedText accepts malformed UTF-8 bytes unchanged

No module test exercises the public narrow-string encoded-text path with malformed UTF-8 or asserts a safe/replacement result.

## Assessment

The DOM fixture passed as part of the 147-test component target and was reviewed for parsing, traversal, errors, disposal, and maximum-depth coverage.

## Missing assertions and diagnostics

The reviewed surface should retain exact-result, malformed-input, lifetime, and error-path assertions appropriate to its public contract.

## Final assessment

AUDITED; evidence is recorded at component scope. Confirmed finding links above require evidence-backed remediation after the audit phase.
