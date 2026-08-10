# Audit: `modules/text-json/include/System/Text/Json/Nodes/JsonArray.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Text.Json`.
- Evidence: focused `SharpRuntimeTests_Text_Json` passed 147/147; direct behavior probe at `/tmp/sharp-runtime-text-json-audit/text_json_probe.cpp`; node-lifetime ASan/UBSan probe at `/tmp/sharp-runtime-text-json-audit/node_lifetime.cpp` where applicable.

## SR-AUD-327 — high — JsonNode children retain dangling raw parent pointers after parent destruction

JsonArray owns children with shared_ptr but stores parent only as raw pointer. If a child is retained after its array dies, getRootProperty walks freed memory; ASan reports heap-use-after-free.

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

Array ownership, insertion, detach, cycle prevention, and serialization paths were reviewed.

## Missing assertions and diagnostics

The reviewed surface should retain exact-result, malformed-input, lifetime, and error-path assertions appropriate to its public contract.

## Final assessment

AUDITED; evidence is recorded at component scope. Confirmed finding links above require evidence-backed remediation after the audit phase.
