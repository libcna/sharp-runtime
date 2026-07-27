# Audit: `modules/collections/include/System/Collections/ObjectModel/ReadOnlyDictionary.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Collections.Core`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Collections_Core && build/SharpRuntimeTests_Collections_Core --gtest_color=no` passed 1,422/1,422 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-359 — medium — ReadOnlyDictionary::Empty returns a mutable singleton reference

`Empty()` returns a non-const reference to a process-static wrapper, so default copy assignment can rebind its private shared backing map.  The direct probe prints `empty-before=0`, assigns a one-entry read-only wrapper into `Empty()`, then prints `empty-after-assignment=1`.  The globally published empty instance is therefore mutable and process-contaminable despite exposing no collection mutator.

## Missing assertions and diagnostics

- Tests do not verify Empty remains empty after copies, assignments, or access from another consumer.
- Return an immutable value/const reference or explicitly suppress assignment; log accidental mutation attempts in debug builds.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.

## Remediation note (ticket #1780, 2026-07-27)

SR-AUD-359 is **remediated**. This note is added alongside the original
evidence above, which is left unmodified per this repository's practice of
preserving historical audit narrative.

Root cause confirmed against the current .NET reference
(`/rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/System/Collections/ObjectModel/ReadOnlyDictionary.cs`):
.NET's `Empty` is a `{ get; }`-only auto-property with no setter, so
`ReadOnlyDictionary<TKey,TValue>.Empty = x;` is a `CS0200` compile error in
every C# consumer. sharp-runtime's port translated that get-only property into
a mutable C++ reference-returning static method, so ordinary assignment
through `Empty()`'s result rebound the process-wide singleton's private
backing map, exactly as the finding above describes. Design ticket #1779
(`docs/ReadOnlyDictionaryEmptyDesign.md`) evaluated three alternatives —
selecting a `const`-reference return type over returning by value (would have
broken the singleton-identity contract and its regression test) or deleting
every instance's assignment operators (broader than this finding's bounded
scope) — before this implementation ticket landed it.

Fix: `Empty()`'s declared return type changed from `ReadOnlyDictionary<K, V>&`
to `const ReadOnlyDictionary<K, V>&`, the literal C++ expression of ".NET has
no setter." No other member, constructor, or the class's copy/move assignment
operators changed, so ordinary, non-singleton instances remain freely
assignable exactly as before.

Pre-fix reproduction (gitignored `build-probe-readonlydict/probe1_mutable_empty.cpp`,
ASan+UBSan) re-ran against the still-unmodified production header and
reconfirmed `empty-before=0`/`empty-after-assignment=1`/
`second-caller-observes=1`/`same-instance=1`, matching the finding's own
symptom and confirming the contamination is visible to an unrelated second
call site on the identical singleton object. Post-fix,
`build-probe-readonlydict/probe4_production_header_rejects_assignment.cpp`
(compiled directly against the real, now-modified header, not a copy) fails
to compile with `error: passing 'const ReadOnlyDictionary<...>' as 'this'
argument discards qualifiers`, and
`probe5_production_header_preserves_behavior.cpp` runs clean under
ASan+UBSan with `all-assertions-passed=1`, confirming singleton identity,
emptiness, normal construction, `ContainsKey`, indexer access, and
independent copy-construction are unaffected.

Closure evidence: two new permanent regressions in
`ObjectModelTests.cpp::ReadOnlyDictionaryTests` — a `static_assert` pinning
the exact `const ReadOnlyDictionary<K,V>&` return type, and
`Empty_RemainsEmptyAfterConstructingUnrelatedInstances` — while the existing
`Empty_IsEmptyAndCached` case is retained verbatim;
`SharpRuntimeTests_Collections_ObjectModel` grew from 124/124 to 125/125; a
new standalone `Collections.Core` public-header consumer fixture
(`test/consumer/collections_object_model_readonlydictionary.cpp`) compiles
`-Wall -Wextra -Wpedantic -Werror` and runs successfully; a companion
negative-compile fixture
(`test/consumer/collections_object_model_readonlydictionary_negative.cpp`)
fails to compile with the same diagnostic through the repository's own
`test/consumer/CMakeLists.txt` harness; and the network-permitted
`scripts/local_ci_check.sh build` gate passes 13,022/13,022 tests across 37
executables with zero warnings/errors (was 13,021). Module boundaries stay at
41 modules/90 edges; validator tests 7/7; catalogue current; database
consistent; `git diff --check` clean; Doxygen stays at exactly 1,942/1,942.
No public signature other than `Empty()`'s return type changed and no virtual
member was added or removed (the class has none), so this is source-breaking
only for the exact hazardous explicit-non-`const`-reference/assignment
pattern (confirmed absent everywhere in this repository) and not an ABI
break: `Collections.Core` is a header-only `INTERFACE` CMake target with no
exported archive, and a direct `nm`/`c++filt` comparison of the mangled
`Empty()` symbol before and after the change shows byte-identical names (the
Itanium ABI does not encode a function's return type in its mangled name).
