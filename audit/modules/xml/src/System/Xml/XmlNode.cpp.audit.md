# Audit: `modules/xml/src/System/Xml/XmlNode.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Xml`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Xml && build/SharpRuntimeTests_Xml --gtest_color=no` passed 377/377 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-350 — high — invalid InnerXml destructively clears existing children without an error

`setInnerXmlProperty` removes all current children before parsing a wrapped fragment and ignores the parser return status.  Setting invalid `<bad>` on an element with a child returns normally; the direct probe prints `invalid-inner-xml=accepted` and then an empty `inner-xml-after-invalid`.  Invalid input therefore causes silent data loss instead of an XML exception with atomic replacement semantics.

**REMEDIATED — ticket #2074, 2026-08-04.** *(Appended; the original finding text
above is preserved verbatim.)*

Reproduced exactly. The repair is an **ordering change plus an error path this
module already had**: `XmlDocument::LoadXml` parses the same text through the
same substrate and throws a shaped `XmlException`, so no new policy and no
reference evidence were needed. `setInnerXmlProperty` now parses first, clones
every child **before** touching this node — so a failure inside `DeepClone`
cannot strip the node either, a step the review did not specify — and only then
removes the old children and inserts the new ones.

Valid fragments, text-only fragments, mixed content and the clearing empty
string are unchanged. +7 permanent regressions; mutation-checked by restoring
the original ordering, which fails exactly two of them and nothing else in the
394-test suite. A first mutation attempt was **discarded rather than reported**:
`if (false && Parse(...))` short-circuits, so `Parse` was never called and three
unrelated tests failed instead — a mutation that changes more than the guard
proves nothing about the guard.

Deliberate narrowing: a caller who relied on the clearing side effect of an
invalid fragment must call `RemoveAll()`.

## SR-AUD-351 — high — DOM mutators detach a node owned by an unrelated parent

`RemoveChild` only checks that native pointers exist, then detaches the supplied node from the document without checking that it belongs to the receiver.  Calling `a->RemoveChild(childOfB)` is accepted and removes B's child; the direct probe prints `remove-foreign-child=accepted` and `foreign-child-still-under-b=0`.  This permits arbitrary same-document structural mutation through the wrong parent.

**REMEDIATED — ticket #2075, 2026-08-04.** *(Appended; the original finding text
above is preserved verbatim.)*

**Corrected premise: four public mutators are affected, not one, and they fail
in three different ways.** Measured with `root -> {a, b}` and `b -> {childOfB}`:

| Call | Before |
|---|---|
| `a->RemoveChild(childOfB)` | B's child detached — as filed |
| `a->InsertBefore(n, childOfB)` | `refChild` **silently ignored**; `n` landed inside `a` |
| `a->InsertAfter(n, childOfB)` | `n` ended up **nowhere at all** |
| `a->ReplaceChild(n, childOfB)` | both wrongs at once |
| `root->RemoveChild(orphan)` | a **parentless** node accepted silently |

Root cause: the port already validated *document identity*
(`ThrowIfDifferentDocument`) and *ancestry* (`ThrowIfSelfOrAncestor`) but never
**parenthood**. One shared `ThrowIfNotChildOf` now guards all four;
`ReplaceChild` checks **before** delegating to `InsertBefore`, because otherwise
a foreign `oldChild` would leave `newChild` already spliced in when the removal
threw.

**The ownership question is answered by measurement, and the answer is a
non-result.** With `XmlNode.cpp`, `XmlDocument.cpp`, `XmlElement.cpp` and
`vendor/tinyxml2/tinyxml2.cpp` compiled **from source** into the probe — `Xml`
is a `STATIC` component, so the archive would have been uninstrumented — ASan,
UBSan and LSan are clean **before** the repair, including over 200 consecutive
drops. The dropped node comes from the document's own node pool and is freed
with the document: an **orphan**, not a leak, not a dangling wrapper. The defect
is therefore precisely the silent acceptance, the behavioural test is the
closure evidence rather than a sanitizer run, and instrumentation was proven by
a control heap-use-after-free in the same binary.

+8 permanent regressions; mutation-checked by emptying the guard, which fails
exactly the five ownership tests and nothing else. The same four calls with a
correct child, a null `refChild`, and the pre-existing cross-document and
ancestry guards are all unchanged. The exception wording follows this file's
existing `ArgumentException` style and is recorded as this port's choice; .NET's
exact text is not verifiable here (`/rv/tmp/runtime/` absent).

## Missing assertions and diagnostics

- DOM tests do not cover invalid-fragment failure atomicity or report a parser location for `InnerXml`.
- They also omit foreign-child removal/replacement and ownership diagnostics for cross-parent mutation.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.
