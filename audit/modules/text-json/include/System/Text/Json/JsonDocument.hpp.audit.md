# Audit: `modules/text-json/include/System/Text/Json/JsonDocument.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Text.Json`.
- Evidence: focused `SharpRuntimeTests_Text_Json` passed 147/147; direct behavior probe at `/tmp/sharp-runtime-text-json-audit/text_json_probe.cpp`; node-lifetime ASan/UBSan probe at `/tmp/sharp-runtime-text-json-audit/node_lifetime.cpp` where applicable.

## SR-AUD-324 — medium — disposed JsonDocument leaves captured JsonElement values usable

JsonElement aliases the parsed tree with a shared_ptr but has no shared disposal state. The direct probe prints `element-after-dispose=10` after `Dispose()`, whereas the current .NET `CheckUseAfterDispose` tests require ObjectDisposedException.

## SR-AUD-326 — medium — JsonDocument parsing flags are exposed but not applied

`Parse` validates options but passes only the comment setting to nlohmann. With `AllowTrailingCommas=true`, `[1,]` is rejected; with `AllowDuplicateProperties=false`, `{\"x\":1,\"x\":2}` is accepted as `x=2`.

## Assessment

The public declaration was reviewed against its implementation, focused tests, and the installed current .NET runtime reference surface. No additional distinct confirmed finding is assigned to this file.

## Missing assertions and diagnostics

The reviewed surface should retain exact-result, malformed-input, lifetime, and error-path assertions appropriate to its public contract.

## Final assessment

AUDITED; evidence is recorded at component scope. Confirmed finding links above require evidence-backed remediation after the audit phase.
