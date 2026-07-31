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

### Correction appended by ticket #1886 (2026-07-31) — the approved core repair landed; finding still `confirmed`

Both paragraphs above are retained as the historical record. `JsonArray` and
`JsonObject` now each declare a destructor whose only effect is to clear the
parent link of every child that still names *that* container, before `items_` /
`properties_` is released (`docs/OwnedTreeLifetimeContractPlan.md` §13, §31 item
1, §33). Measured by re-running **the same** probe
`build-probe/1885_ccf019_lifetime_probe.cpp`, unmodified, in the same three
builds from one source:

| | before (#1885) | after (#1886) |
|---|---|---|
| JsonNode cases producing an ASan `heap-use-after-free` | 8 (J01, J02, J03, J04, J08, J11, J16, J17) | **1** (J11 only) |
| JsonNode faulting accesses, recoverable ASan | 9 | **1** |
| Retained child of a destroyed owner: `getParentProperty()` | dangling non-null | **`nullptr`** |
| Retained child of a destroyed owner: `getRootProperty()` | freed address | **the child itself** |
| Retained child re-attachable | never (`The node already has a parent.` forever) | **yes** (J06 now succeeds) |
| `sizeof` JsonNode/JsonArray/JsonObject/JsonValue | 24/48/48/40 | **24/48/48/40** |
| Allocations added to construction / access / destruction | — | **0 / 0 / 0** |
| External defined symbols in `JsonNode.cpp` | 219 | **219, identical** |

J02 and J16 no longer produce a use-after-free; the probe's own bodies
dereference the returned parent **without a null check** (they were written
against a state where it was never null), so post-fix they fault in *probe* code
at `1885_ccf019_lifetime_probe.cpp:141` and `:342` — UBSan names it
`member access within null pointer of type 'struct JsonNode'`. The library-side
defect is gone; the permanent suite `JsonNodeLifetimeTests` asserts the defined
answer for the same two shapes.

**Residual exposure, all of it approval-blocked and none of it closed by this
ticket:**

- **J11** — a `JsonArray` iterator held across a reallocating `Add` is still an
  ASan-confirmed `heap-use-after-free`; **J12** — a `JsonObject` iterator still
  reads destroyed storage after `Clear()` with no diagnostic. Both are ticket
  **#1889** (object-layout change; §31 item 4).
- **J08 / J09 / J13** — `JsonNode`'s copy operations are still implicitly
  generated, so a copy-constructed container still aliases children that report
  the original as their parent, a slicing copy-assign still rewrites `parent_` on
  a stored node, and the public `DetachParent()` still lets one node sit in two
  containers. Ticket **#1888** (public source break; §31 item 3).
- **J10** — `JsonObject::SetItem` still detaches before it assigns, so its
  throwing path still leaves a parentless node stored. Ticket **#1887**
  (§31 item 2).
- **J19c / J19d / X28c** — deep-nesting teardown and `JsonNode::Parse` still
  overflow the stack at 20,000 levels and still time out at 100,000. Ticket
  **#1893** (accepted-input change; §31 item 6).
- **J15** — `JsonNodeOptions` are still not inherited from the parent; a
  deliberate permanent exclusion (`docs/OwnedTreeLifetimeContractPlan.md` §30.4).

Because J11 is still an ASan-confirmed use-after-free inside this finding's own
files, **SR-AUD-327 stays `confirmed (design-complete)`** and the post-audit
total is unchanged. No new `SR-AUD-*` identifier was issued; numbering stays
frozen at **364**.

## Assessment

The mutable-node ownership model, parent assignment, root traversal, casting, cloning, and parse entry points were reviewed.

## Missing assertions and diagnostics

The reviewed surface should retain exact-result, malformed-input, lifetime, and error-path assertions appropriate to its public contract.

## Final assessment

AUDITED; evidence is recorded at component scope. Confirmed finding links above require evidence-backed remediation after the audit phase.
