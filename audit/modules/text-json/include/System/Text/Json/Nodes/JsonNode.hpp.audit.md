# Audit: `modules/text-json/include/System/Text/Json/Nodes/JsonNode.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Text.Json`.
- Evidence: focused `SharpRuntimeTests_Text_Json` passed 147/147; direct behavior probe at `/tmp/sharp-runtime-text-json-audit/text_json_probe.cpp`; node-lifetime ASan/UBSan probe at `/tmp/sharp-runtime-text-json-audit/node_lifetime.cpp` where applicable.

## SR-AUD-327 — high — JsonNode children retain dangling raw parent pointers after parent destruction

The non-owning `parent_` has no lifetime guard and `getRootProperty` walks it directly. The parent container can be destroyed while external child shared_ptr ownership remains.

### Correction appended by ticket #1885 (2026-07-30) — design-only, still `confirmed`

The paragraph above is retained as the historical record. Measured against the
shipped bodies (probe `build-probe/1885_ccf019_lifetime_probe.cpp`, every
production translation unit compiled from source into the probe, one process per
case under a watchdog, in three builds from one source):

- **The measured surface is 41 public entries across 5 files**, not
  `getRootProperty` alone. **8** ASan `heap-use-after-free` accesses: J01/J03/J04
  (`getRootProperty` at one, two and three freed levels), J02 (a virtual call
  through the freed parent), J08 (a copy-constructed `JsonArray`), J11 (an
  iterator held across a reallocating `Add`), J16 (allocator reuse) and J17
  (`AssignParent`'s cycle guard walking a stale ancestor chain, at
  `JsonNode.cpp:22`). J01: `READ of size 8` at `0x506000000030`, 16 bytes into
  the 64-byte `make_shared<JsonArray>` region, freed by `~shared_ptr<JsonArray>`,
  process exit status 1, no timeout; **without a sanitizer it does not crash** —
  it returns a plausible-looking freed address.
- **Six further cases give a wrong answer with no diagnostic in any build**, and
  none is named by any audit report: the implicitly generated copy constructor
  makes two `JsonArray`s share one set of children that still report the
  *original* as their parent; the implicitly generated `operator=` slices,
  rewriting `parent_` on a node that is still stored in a container;
  `JsonObject::SetItem`'s throwing path detaches the stored value and leaves it
  stored, after which a **second** container accepts it; a `JsonObject` iterator
  survives `Clear()` and reads destroyed storage; the public `DetachParent()`
  lets one node sit in two containers; and allocator reuse makes a retained child
  report an unrelated `JsonObject` as its parent, with the wrong `GetValueKind`,
  at the identical address.
- **A retained child of a destroyed parent is permanently unusable**, not merely
  unsafe to read: `AssignParent` sees the stale non-null `parent_` and throws
  `The node already has a parent.` forever.
- **Two stack-overflow shapes exist here**, one of them reachable from untrusted
  input: `JsonNode::Parse` of 20,000 nested arrays, and a 20,000-deep
  programmatic nest that crashes on *release*. The guards are quadratic in depth.

**No new `SR-AUD-*` identifier was issued** — every fact above was found while
analysing this finding, in the files it already owns — and numbering stays frozen
at **364**. The selected repair, the rejected alternatives with their measured
evidence, the cycle and destruction proofs, and the exact approval request are in
`docs/OwnedTreeLifetimeContractPlan.md`. Implementation is #1886/#1887/#1888/
#1889/#1893/#1894, all `needs_user` or `blocked`. **Nothing has been
implemented; SR-AUD-327 remains `confirmed`.**

## Assessment

The mutable-node ownership model, parent assignment, root traversal, casting, cloning, and parse entry points were reviewed.

## Missing assertions and diagnostics

The reviewed surface should retain exact-result, malformed-input, lifetime, and error-path assertions appropriate to its public contract.

## Final assessment

AUDITED; evidence is recorded at component scope. Confirmed finding links above require evidence-backed remediation after the audit phase.
