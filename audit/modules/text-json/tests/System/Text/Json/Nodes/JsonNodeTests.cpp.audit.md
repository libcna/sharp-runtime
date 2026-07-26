# Audit: `modules/text-json/tests/System/Text/Json/Nodes/JsonNodeTests.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Text.Json`.
- Evidence: focused `SharpRuntimeTests_Text_Json` passed 147/147; direct behavior probe at `/tmp/sharp-runtime-text-json-audit/text_json_probe.cpp`; node-lifetime ASan/UBSan probe at `/tmp/sharp-runtime-text-json-audit/node_lifetime.cpp` where applicable.

## SR-AUD-327 — high — JsonNode children retain dangling raw parent pointers after parent destruction

Tests cover RemoveAt/Clear detachment but not parent destruction while a child shared_ptr survives; the missing lifetime assertion allowed an ASan-confirmed UAF.

## SR-AUD-328 — medium — JsonValue integer accessors silently truncate floating JSON numbers

The existing wrong-kind test omits numeric-kind but nonintegral values such as 1.5, exponent literals, and numeric bounds.

## Assessment

The focused node fixture passed as part of the 147-test component target and was reviewed for ownership, cycle, parser, clone, and conversion assertions.

## Missing assertions and diagnostics

The reviewed surface should retain exact-result, malformed-input, lifetime, and error-path assertions appropriate to its public contract.

## Final assessment

AUDITED; evidence is recorded at component scope. Confirmed finding links above require evidence-backed remediation after the audit phase.
