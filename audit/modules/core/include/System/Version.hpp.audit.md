# Audit: `modules/core/include/System/Version.hpp`

## Metadata

- Audit status: AUDITED (227 lines, full read; header-only implementation).
- Public API: `System.Version` construction, parse, comparison, formatting,
  and revision properties.
- Validation: 54 Version tests passed in the focused Core.Base run.

## Assessment

The parser correctly distinguishes malformed, overflow, negative, and
trailing-separator cases; comparisons avoid subtraction overflow.  The
field-count formatting overload emits unspecified fields instead of rejecting
them.

## Finding

### SR-AUD-011 — medium — `Version::ToString(fieldCount)` serializes unspecified components as `-1`

`ToString(intcs fieldCount)` only validates the numeric interval 0–4.  When a
two-component version is asked for three fields, it appends `Build`, whose
sentinel value is `-1`; a three-component version asked for four fields does
the same for `Revision`.

**Reproduction (observed in the audit probe):**

```cpp
Version(1, 2).ToString(3); // current result: "1.2.-1"
```

.NET `Version.ToString(int)` throws `ArgumentException` when `fieldCount`
requests a component that the instance does not define.  The result above is
not a valid version representation and contradicts the API's specified
components.

**Required post-audit verification:** add throw assertions for
`Version(1,2).ToString(3/4)` and `Version(1,2,3).ToString(4)`, alongside valid
2- and 3-field cases.  Repair must reject field counts beyond the defined
component count.

### Status: REMEDIATED (#2257 review, #2258 implementation, 2026-08-11)

The finding reproduced **exactly as filed** — no premise correction was
required, which is worth recording because the preceding five `modules/core`
units each corrected something.

`build-probe/2257_probe1_before.cpp` walked every (defined-component-count,
`fieldCount`) pair — eight subjects (the default constructor, all three counted
constructors, all three parsed spellings and an all-zero four-component
version) against `fieldCount` ∈ `{-1, 0, 1, 2, 3, 4, 5}`, 56 pairs — and read
**48 OK / 8 BAD**. The eight bad cells were exactly and only those where
`fieldCount` exceeded the instance's defined component count.

The probe established three facts this report does not state: the **parsed**
spellings reach the same cells, so the repair belongs in `ToString` and not in a
constructor; `Version()` is itself a two-component version, so
`Version().ToString(3)` was bad and no report or test named it; and zero is a
*defined* component, so the repair could not be written as "skip a falsy
component".

**No existing test pinned the defect.** All seven pre-existing
`ToString(fieldCount)` tests use a fully specified four-component subject, and
the two rejection tests asserted the exception type only. Nothing was retired.

The repair adds two guards to `ToString(intcs fieldCount)`, testing `Build` and
then `Revision` **independently, in .NET's own order** (`Version.ToString(int)`
checks `_Build` first with bound 2, then `_Revision` with bound 3) rather than
deriving a component count — because this port's public mutable fields can hold
an undefined `Build` beside a defined `Revision`, a state no .NET constructor
produces. They test `< 0` rather than `== -1`, matching the predicate the
no-argument `ToString()` already applies to each component. The pre-existing
out-of-interval branch keeps its exact message and still runs **first**, so no
already-correct rejection changed; .NET's own resource text and its
instance-dependent out-of-interval bound could not be verified with `/rv` absent
and are deferred to #2260.

After: **56 OK / 0 BAD** over the identical matrix, the `OK`-cell diff shows
additions only, and the no-argument control is byte-identical. +15 tests; five
mutations, all caught. No signature, layout (`sizeof`/`alignof` 16/4), vtable,
`noexcept` or symbol change. `docs/CoreVersionFieldCountPlan.md`.

### A separate divergence found while testing this one (#2259)

The **no-argument** `ToString()` tests `Build >= 0` and `Revision >= 0` in two
independent `if`s, so it omits an undefined leading component while still
emitting a defined trailing one — `Build = -5` on `1.2.3.4` renders `"1.2.4"`,
printing `Revision` in `Build`'s position. .NET's `ToString()` delegates to
`ToString(n)` with a short-circuiting field count and truncates at two fields
instead. This is **not** SR-AUD-011 — that finding is about the `fieldCount`
overload emitting a sentinel it was asked for — audit numbering is frozen, and
it therefore carries **no `SR-AUD-*` identifier** and takes ordinary ticket
#2259.

**#2259 closed 2026-08-11.** `ToString()` now derives its field count with
.NET's short-circuit (`Build < 0 ? 2 : Revision < 0 ? 3 : 4`) and delegates to
`ToString(fieldCount)`, so the two overloads agree by construction rather than
through two hand-maintained copies of the same predicate, and the derived count
can never name an undefined component. The only state whose text changes is
unreachable through every constructor and through `parse()`; the byte-identity
control added by #2258 passes unchanged and did not fail under any of the three
mutations, all of which were caught. +3 tests. No signature, layout, vtable,
`noexcept` or symbol change. SR-AUD-011's own status is unaffected by this
ticket.

## Final assessment

Parsing and comparison are robust; field-count formatting had a confirmed
observable parity defect (SR-AUD-011), remediated by #2258.
