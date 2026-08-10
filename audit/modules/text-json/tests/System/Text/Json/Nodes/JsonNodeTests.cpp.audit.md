# Audit: `modules/text-json/tests/System/Text/Json/Nodes/JsonNodeTests.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Text.Json`.
- Evidence: focused `SharpRuntimeTests_Text_Json` passed 147/147; direct behavior probe at `/tmp/sharp-runtime-text-json-audit/text_json_probe.cpp`; node-lifetime ASan/UBSan probe at `/tmp/sharp-runtime-text-json-audit/node_lifetime.cpp` where applicable.

## SR-AUD-327 — high — JsonNode children retain dangling raw parent pointers after parent destruction

Tests cover RemoveAt/Clear detachment but not parent destruction while a child shared_ptr survives; the missing lifetime assertion allowed an ASan-confirmed UAF.

## SR-AUD-328 — medium — JsonValue integer accessors silently truncate floating JSON numbers

The existing wrong-kind test omits numeric-kind but nonintegral values such as 1.5, exponent literals, and numeric bounds.

### Correction appended by ticket #1886 (2026-07-31) — the approved core repair landed; finding still `confirmed`

`JsonArray` and `JsonObject` now clear the parent link of every child they still
own in their own destructor. 7 of this finding's 8 ASan `heap-use-after-free`
cases stop producing one (J01, J02, J03, J04, J08, J16, J17); the recoverable-ASan
faulting-access count for the JsonNode section falls 9 → 1. `sizeof`, vtables,
symbols and allocation counts are unchanged (measured). The full before/after
table and the complete residual list — J11/J12 (#1889), J08/J09/J13 (#1888), J10
(#1887), J19c/J19d/X28c (#1893) — are recorded once, on
`audit/modules/text-json/include/System/Text/Json/Nodes/JsonNode.hpp.audit.md`,
and in `docs/OwnedTreeLifetimeContractPlan.md` §33. **SR-AUD-327 stays
`confirmed (design-complete)`**; numbering stays frozen at 364.

## Assessment

The focused node fixture passed as part of the 147-test component target and was reviewed for ownership, cycle, parser, clone, and conversion assertions.

## Missing assertions and diagnostics

The reviewed surface should retain exact-result, malformed-input, lifetime, and error-path assertions appropriate to its public contract.

## Final assessment

AUDITED; evidence is recorded at component scope. Confirmed finding links above require evidence-backed remediation after the audit phase.
