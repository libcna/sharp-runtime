# Audit: `modules/xml/src/System/Xml/XmlNamespaceManager.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Xml`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Xml && build/SharpRuntimeTests_Xml --gtest_color=no` passed 377/377 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-353 — medium — HasNamespace ignores namespaces inherited from outer scopes

The public contract says `HasNamespace` determines whether a prefix is declared in any scope, but the implementation inspects only `scopes_.back()`.  After declaring an outer prefix and pushing a scope, the direct probe prints `has-outer-in-inner-scope=0`.

**REMEDIATED — ticket #2077, 2026-08-04.** *(Appended; the original finding text
above is preserved verbatim.)*

Reproduced exactly, and **wider than filed**: the built-in `xml` prefix, which
this class installs itself in scope 0 and documents as permanent, was invisible
from any inner scope too. The predicate is now
`LookupNamespace(prefix).has_value()` — expressed in terms of the sibling that
has always searched outward, so the two cannot desynchronise again. The header's
own contract already stated the intended behaviour, so no reference evidence was
required.

**The defect was pinned by a pre-existing test.**
`XmlNamespaceManagerTests.HasNamespace_ChecksCurrentScopeOnly` asserted
`EXPECT_FALSE(mgr.HasNamespace("foo"))` after `PushScope()`, locking in exactly
the behaviour this report calls wrong, against the type's own documented
contract. That is why it survived a passing suite. The test was **corrected in
place and renamed**, not deleted, with a comment recording the history.

+6 permanent regressions, including the property assertion this report's
"missing assertions" section asks for in spirit: over a four-deep push/pop
cycle, `HasNamespace` and `LookupNamespace` agree for every probe prefix on the
way down and on the way back up. Mutation-checked: restoring the single-scope
search fails exactly four tests and nothing else in the 400-test suite. Widening
only — nothing that returned `true` returns `false`.

## Missing assertions and diagnostics

- Namespace tests cover local declarations and pop behavior but not inherited prefix visibility through nested scopes.
- Include the searched scope depth/prefix in namespace-resolution diagnostics.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.
