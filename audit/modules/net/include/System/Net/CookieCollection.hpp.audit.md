# Audit: `modules/net/include/System/Net/CookieCollection.hpp`

## Metadata

- Audit status: AUDITED.
- Reference: [CookieCollection.cs](/rv/tmp/runtime/src/libraries/System.Net.Primitives/src/System/Net/CookieCollection.cs).
- Evidence: ASan/UBSan probe `/tmp/sharp-runtime-net-audit/cookie_collection_bounds.cpp`.

## Assessment

The thin vector adaptation provides ordered iteration, but its public indexers
convert signed input directly to `size_t` and use unchecked `operator[]`.

### SR-AUD-307 — high — negative or oversized collection indexes reach unchecked vector access

With one cookie, `collection[-1]` converts to a huge unsigned index and the
ASan probe terminates with a segmentation fault at the indexer call.  Managed
`CookieCollection` rejects invalid indexes deterministically.  The C++ API
must not expose raw-container undefined behavior at this public boundary.

Required remediation: validate `0 <= index < Count` and throw the repository's
standard index exception in both const and mutable indexers.

## Missing assertions and diagnostics

There is no direct CookieCollection test coverage.  Add negative, `Count`, and
empty-collection index cases under ASan/UBSan.

## Final assessment

Confirmed public out-of-bounds crash.

---

## Correction and remediation record — ticket #2041, 2026-08-04

*Everything above is the original audit text and is preserved verbatim. This section is
appended, not a rewrite.*

### Correction 1 — the finding's own reproduction is the less reliable of the two

The assessment says `collection[-1]` "terminates with a segmentation fault". Measured with one
probe source built four ways (`build-probe/2041_probe1_before_after.log`):

| Build | `collection[-1]`, 1 element | `collection[Count]` | `collection[-1]`, **empty** |
|---|---|---|---|
| plain `-O0 -g` | **returned normally**, `Name` printed as `(null)`, both overloads | **SIGSEGV** | **SIGSEGV** |
| `-fsanitize=address` | `SEGV` | `heap-buffer-overflow` | `SEGV` |

So the named case is the one that can **silently succeed** — worse, not better — and *which* of
the two outcomes appears depends on the heap layout rather than on the index, which is what
"undefined" means. Both directions and both overloads are now covered by tests.

### Correction 2 — the defect is not confined to the two indexes the report names

Eleven indexes were probed across both overloads: `-1`, `0`, `Count-1`, `Count`, `Count+1`,
`INTCS_MIN`, `INTCS_MAX`, and `0`/`-1` on an empty collection. **Nine of the eleven produced an
ASan report before the repair and none after.** `INTCS_MIN` is the worst case: its `size_t`
conversion produces the largest possible wrong subscript.

### Correction 3 — UBSan is *partially* discriminating, against the review plan's prediction

`docs/SystemNetNamespaceReviewPlan.md` §11 predicted UBSan would be non-discriminating here
because an `intcs` → `size_t` conversion is implementation-defined, not undefined. That half is
correct — UBSan says nothing about the conversion. But on the **empty** collection, where
`std::vector::data()` is null, it reported
`reference binding to null pointer of type 'const struct value_type'` and
`applying non-zero offset 18446744073709551464 to null pointer` before the repair, and nothing
after. Recorded as evidence, not as a non-result.

### Remediation

Both indexers route through one private `validatedIndex(intcs)`:

```cpp
if (index < 0 || static_cast<size_t>(index) >= cookies_.size())
    throw System::ArgumentOutOfRangeException(
        "index",
        "Index was out of range. Must be non-negative and less than the size of the collection.");
```

The negative half is tested on the **signed** value, so no `size_t` conversion is ever applied
to a negative index; and because the bound is compared in `size_t`, it cannot be truncated the
way a single `uintcs` comparison would be. The message is the one `List<T>`, `Array`,
`ArrayList`, `ArraySegment` and `ObservableCollection` already use in this repository (23
sites), so no reference tree was required — which matters, since
`/rv/tmp/runtime/src/libraries/` is absent from this container.

**Tests:** `modules/net/tests/System/Net/CookieCollectionIndexTests.cpp` (12) and
`NetLayoutPinTests.cpp` (2, including `static_assert` layout pins for `Cookie`,
`CookieCollection`, `IPAddress`, `IPEndPoint` and `SocketAddress`).

**Consequences:** no signature, `noexcept`, virtual, vtable, data member or object-layout
change — `sizeof(CookieCollection)` is 24 before and after. `CookieCollection` is header-inline,
so `net-http`, `net-http-json`, `net-websockets`, `net-sockets`, `net-network-information` and
the integration suite all recompiled and were re-run.

Status: `confirmed` → **`remediated`**.
