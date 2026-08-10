# Audit: `modules/collections/include/System/Collections/IDictionary.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Collections.Core`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Collections_Core && build/SharpRuntimeTests_Collections_Core --gtest_color=no` passed 1,422/1,422 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## Assessment

The public declaration, its immediate implementation path, and focused call sites were reviewed.  No separate evidence-backed finding is assigned to this file beyond the related findings recorded in their owning reports.

## Missing assertions and diagnostics

- Keep invalid-input, lifecycle, ownership, and native-boundary diagnostics covered by focused tests as this surface evolves.

## Final assessment

AUDITED. No separate evidence-backed finding is assigned to this file.

---

## Post-audit change — ticket #1796 (2026-07-28)

**`getItem`'s return type changed on the interface**, from `void*` to an owning
`std::any` by value, under the four-item user approval recorded in
`docs/HashtableValueAccessSafetyDesign.md` §32. No finding was assigned to this
file at audit time and none is assigned now; this note exists because the
*interface* changed and a per-file report that says "no separate finding" should
not silently also mean "unchanged".

The declaration was `[[nodiscard]] virtual void* getItem(const void* key) const = 0;`
— a pure virtual, `const`, returning a **non-`const`** pointer. Both production
implementations honoured that literally: `Hashtable` returned
`const_cast<std::any*>(&it->second)`, a writable pointer into its live
`std::unordered_map`, so a caller holding only a `const IDictionary&` could
rewrite a stored value with the fail-fast mutation counter unmoved and the
pointer dangling after `Remove`/`Clear`/assignment/destruction.

Three consequences a reader of this header needs:

1. **Every implementer must migrate.** `void*` is not a covariant return for
   `std::any`, so an unmigrated implementation fails with `conflicting return
   type specified for …`. It cannot compile lazily — verified as a marked site in
   `test/consumer/collections_hashtable_value_access_negative.cpp`.
2. **Silent ABI break.** The mangled name and vtable slot (`0x38`) are
   **unchanged**; only the calling convention differs, because `std::any` is
   returned through a hidden `sret` pointer. A stale caller object **links with
   zero diagnostics and then segfaults**. Every consumer must be fully rebuilt.
3. **`setItem`/`Add`'s `void*` *value* parameter is unchanged, deliberately.**
   The doc-comment now says why: migrating it to `const std::any&` makes
   `setItem("literal", v)` prefer the raw-address overload — a standard
   `const char*` → `const void*` conversion beats the user-defined
   `const char*` → `std::string` one — and stores the entry under the
   stringified address of the literal, with no diagnostic under
   `-Wall -Wextra -Wpedantic -Werror`. That input-side type hole therefore
   **remains open** and is not claimed remediated.

The interface is exercised by `HashtableValueAccessSafetyTests.cpp`'s
`DictionaryGetItemContract` suite, parameterised over both production
implementations, and by `test/consumer/collections_hashtable_value_access.cpp`.
