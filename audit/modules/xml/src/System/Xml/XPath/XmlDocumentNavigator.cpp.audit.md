# Audit: `modules/xml/src/System/Xml/XPath/XmlDocumentNavigator.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Xml`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Xml && build/SharpRuntimeTests_Xml --gtest_color=no` passed 377/377 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-355 — medium — XPath navigator exposes adjacent text/CDATA nodes separately

XPath data-model navigation must collapse an adjacent text-like run into one logical text node.  The navigator returns the current native node directly: the direct probe first reports `xpath-first-text-value=left`, then successfully moves to a second text node with value `right`.  XPath position/value semantics consequently diverge for mixed CDATA/text content.

## Remediation record — #2081 (2026-08-04)

**REMEDIATED.** Evidence above retained. Plan: `docs/SystemXmlNamespaceReviewPlan.md` §4.7 and
§20.7. Reproduction: `build-probe/2081_probe1_textruns.cpp`, logs
`2081_probe1_before.log` → `2081_probe1_after.log`.

**The finding names the value; measurement found four coupled symptoms.**

| Measured on the run shapes in the probe | Before | After |
|---|---|---|
| `<r>left<![CDATA[right]]></r>` walked forward | `Text='left' Text='right'` | `Text='leftright'` |
| the same walked backward from the end | `Text='left'` — **asymmetric** | `` — one node, so nothing before it |
| `IsSamePosition` on two members of one run | `0` | `1` |
| a navigator built **directly** on a mid-run node | reported that node alone | reports the whole run, calibrated |
| `Select("text()")` over `<r>a<![CDATA[b]]>c<e/>f</r>` | four nodes | **two**: `"abc"` and `"f"` |

**The repair is named after the mechanism the file itself already identified.** The 17-line
`KNOWN GAP (audited, not fixed …)` comment in `getValueProperty` cited .NET's
`ValueText`/`CalibrateText` and said a correct fix *"needs coordinated changes across those
three navigation methods plus this method"*. That is exactly what landed, plus two the comment
did not anticipate — `getNodeTypeProperty` and `IsSamePosition` — and the comment was
**replaced** rather than left standing next to code that no longer matches it.

The model, stated once: **a run's logical position is its first member.** Every navigation
lands there, every accessor answers from there, `GetNode()` returns it, and `IsSamePosition`
compares there. Adjacency is **DOM sibling** adjacency, so any non-text-like sibling ends a run
— including one with no XPath representation at all, which is why a comment, a PI and an entity
reference each split a run.

**Deliberately unchanged:** element and root string-values, which already computed the correct
concatenation through `getInnerTextProperty`. A mixed run takes its **run start's** node type
(a `Whitespace`-led run reports `Whitespace` with the whole run's value), matching
`CalibrateText`'s position-on-first-member model; that is recorded and pinned rather than left
to chance.

**+16 permanent regressions, add-only — not one existing test was updated**, including all 74
pre-existing XPath tests. `SharpRuntimeTests_Xml` 458 → 474.

**Five mutations, reverted from an exact backup.** X1 (value is the node's own) → 10 tests; X3
(`MoveToPrevious` uncalibrated) → 2; X4 (`IsSamePosition` compares the native node) → 1; X5
(node type uncalibrated) → 1. **X2 — stepping off the current node instead of the run end —
makes navigation NON-TERMINATING**: `MoveToNext` lands on the next run member and the
calibration walks straight back to the first, so `while (MoveToNext())` never ends. That is the
sharpest result in the ticket: it proves the two halves of the repair are *coupled*, not merely
co-present, and it is reported as a timeout rather than as a test count.

**Two test-harness corrections are recorded rather than hidden.** X5 was **non-discriminating**
on the first attempt: the only mid-run node-type assertion used a `Text`-after-`CDATA` run, and
`CDATA` maps to `Text`, so an uncalibrated accessor passed it. A run of
`Whitespace` + `Text` — whose members map to *different* XPath node types — was added, and only
then did X5 fail. Separately, the tests originally encoded node types as **numeric literals**
and one was simply wrong (`Text` is 4, not 3); they use enum names now.

**Sanitizers, as §13 required for this ticket** (*"run collapsing keeps references across
native siblings"*): ASan, UBSan and LSan clean over **37,600** characters walked forward,
backward, through `Select("text()")`, and across DOM mutation between walks, with
`XmlDocumentNavigator.cpp`, the DOM bodies and `vendor/tinyxml2/tinyxml2.cpp` compiled **from
source** (`Xml` is `STATIC`) and instrumentation proven by a control heap-use-after-free
(`build-probe/2081_probe2_asan.cpp`, log `2081_probe2_asan.log`).

## Missing assertions and diagnostics

- XPath tests do not assert logical-node collapsing, navigation across adjacent text-like siblings, or combined string values.
- Navigator diagnostics should identify the native run collapsed into each XPath logical node.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.
