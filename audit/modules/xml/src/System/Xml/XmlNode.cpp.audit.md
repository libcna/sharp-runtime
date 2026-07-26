# Audit: `modules/xml/src/System/Xml/XmlNode.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Xml`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Xml && build/SharpRuntimeTests_Xml --gtest_color=no` passed 377/377 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-350 — high — invalid InnerXml destructively clears existing children without an error

`setInnerXmlProperty` removes all current children before parsing a wrapped fragment and ignores the parser return status.  Setting invalid `<bad>` on an element with a child returns normally; the direct probe prints `invalid-inner-xml=accepted` and then an empty `inner-xml-after-invalid`.  Invalid input therefore causes silent data loss instead of an XML exception with atomic replacement semantics.

## SR-AUD-351 — high — DOM mutators detach a node owned by an unrelated parent

`RemoveChild` only checks that native pointers exist, then detaches the supplied node from the document without checking that it belongs to the receiver.  Calling `a->RemoveChild(childOfB)` is accepted and removes B's child; the direct probe prints `remove-foreign-child=accepted` and `foreign-child-still-under-b=0`.  This permits arbitrary same-document structural mutation through the wrong parent.

## Missing assertions and diagnostics

- DOM tests do not cover invalid-fragment failure atomicity or report a parser location for `InnerXml`.
- They also omit foreign-child removal/replacement and ownership diagnostics for cross-parent mutation.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.
