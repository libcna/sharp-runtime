<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `modules/core` `ArgumentOutOfRangeException` guard-template compile domain — plan

Ticket #2253. One frozen audit finding in
`modules/core/include/System/ArgumentOutOfRangeException.hpp`:

| Finding | Severity | Headline |
|---|---|---|
| SR-AUD-091 | medium | generic `ArgumentOutOfRange` guards silently require `std::to_string` in addition to their declared comparison contract |

Audit numbering is frozen at `SR-AUD-001..364`; this review creates no new
`SR-AUD-*` identifier. This is a **slice of one header**, not a `System::Exception`
family review and not a `modules/core` review.

---

## 1. Exact scope, and what is deliberately left out

In scope: the nine public static guard templates in
`modules/core/include/System/ArgumentOutOfRangeException.hpp` —
`ThrowIfZero`, `ThrowIfNegative`, `ThrowIfNegativeOrZero`, `ThrowIfGreaterThan`,
`ThrowIfGreaterThanOrEqual`, `ThrowIfLessThan`, `ThrowIfLessThanOrEqual`,
`ThrowIfEqual`, `ThrowIfNotEqual` — and nothing else in the file.

Out of scope, by decision rather than omission:

- **SR-AUD-092** (`Exception.hpp`, default message text), **SR-AUD-098**
  (`AggregateException.hpp`, message composition) and **SR-AUD-101**
  (`IOException.hpp`, absent overloads). NEXT.md §2.7 already established these
  are three different files with three different root causes; the previous
  batch's ranking called the four-member grouping "tempting but **mixed**",
  because 092 and 098 change emitted diagnostic text that green tests pin and
  101 is additive public API. Each keeps its own compatibility call. All three
  stay `confirmed` and unclaimed.
- **The six non-template constructors and the `.cpp` body.** The
  `ArgumentOutOfRangeException.cpp` audit report references SR-AUD-091 only as
  *context* — "this source receives only the resulting strings" — and records no
  separate defect. The constructors, their `COR_E_ARGUMENTOUTOFRANGE` HResult and
  the `"\nActual value was X."` suffix are untouched.
- **The typed-`actualValue` question.** The `.cpp` report notes under "other
  missing assertions" that this port stores the actual value as a `std::string`
  where .NET stores the boxed object, and says it "needs explicit documentation
  for a later API-compatibility decision". That is a public representation
  change (`std::string` → some variant/any), which is exactly the family this
  repository has declined four times. Not in this slice.
- **The `T value` pass-by-value signature.** Measured below: a non-copyable
  ordered type can never bind to any guard, because all nine take `T` *by value*.
  That is real, it is **not** the formatting defect, and repairing it means
  changing nine public template signatures to `const T&`. Recorded in §7, not
  changed.
- **The primary message wording.** Lines 91–101 of the header already carry an
  explicit, audited decision that this port paraphrases .NET's `ThrowIf*`
  sentences rather than matching them verbatim. This slice reuses that policy and
  does not reopen it.

---

## 2. The measured before-state

`build-probe/2253_probe1_matrix.cpp` compiles **one `(type, guard)` pair per
translation unit**, driven by `build-probe/2253_run_matrix.sh`, so every verdict
is attributable to its own case and one broken pair cannot mask another. 22
candidate types × 9 guards = **198 compilations**, `-std=c++23 -fsyntax-only`,
GCC 13, at most one compiler at a time. Full log:
`build-probe/2253_probe1_matrix.log`.

**Result: 99 OK / 99 FAIL — exactly half the advertised surface does not compile.**

### 2.1 What compiles today (11 types × 9 guards = 99)

`int`, `unsigned`, `long long`, `unsigned long long`, `char`, `short`, `bool`,
**unscoped `enum`**, `float`, `double`, `long double`.

The unscoped enum is the interesting one and it is an **accident**: `std::to_string`
has no enum overload, but an unscoped enumeration undergoes *integral promotion*
to `int`, which outranks every other candidate conversion, so
`std::to_string(UnscopedEnum)` resolves to `std::to_string(int)` and prints the
enumerator's integer value. Nothing in the header intends this.

### 2.2 What fails today (11 types × 9 guards = 99), classified

The finding names one cause. Measured, the failures are **two causes**, and only
one of them is a defect:

| Type | Failing guards | Cause | Is it a defect? |
|---|---|---|---|
| `enum class` (scoped) | all 9 | `no matching function for call to 'to_string(ScopedEnum&)'` | **yes** — undeclared formatter |
| `int*` | all 9 | `to_string(int*&)` | **yes** — undeclared formatter, but see §4.4 |
| `std::string` | all 9 | `to_string(std::string&)` | **yes** — undeclared formatter |
| `std::string_view` | all 9 | `to_string(std::string_view&)` | **yes** — undeclared formatter |
| `OrderedOnly` (the finding's own reproducer) | all 9 | `to_string(OrderedOnly&)` | **yes** — undeclared formatter |
| `NonTrivialOrdered` | all 9 | `to_string(NonTrivialOrdered&)` | **yes** — undeclared formatter |
| `StreamableOrdered` | all 9 | `to_string(StreamableOrdered&)` | **yes** — undeclared formatter |
| `StringConvertibleOrdered` | all 9 | `to_string(StringConvertibleOrdered&)` | **yes** — undeclared formatter |
| `EqualityOnly` | 3 (`ThrowIfZero`, `ThrowIfEqual`, `ThrowIfNotEqual`) | `to_string(EqualityOnly&)` | **yes** — undeclared formatter |
| `EqualityOnly` | 6 ordering guards | `no match for 'operator<'` etc. | **no** — the type genuinely lacks the operator the guard needs |
| `InequalityOnly` | 8 | `no match for 'operator=='` / `'operator<'` etc. | **no** — same |
| `InequalityOnly` | 1 (`ThrowIfNotEqual`) | `to_string(InequalityOnly&)` | **yes** — undeclared formatter |
| `NonCopyableOrdered` | all 9 | pass-by-value `T`; the copy is never available | **separate**, see §7 |

So of the 99 failures, **84 are the undeclared formatter**, **14 are a
legitimately absent comparison operator**, and **1 category (`NonCopyableOrdered`,
9 pairs) is the independent pass-by-value narrowing** — the 14 and the 9 overlap
the table above because a single pair can only report its first error.

### 2.3 What the failure *looks like*, which is half the finding

The audit report says the diagnostic "leaks a standard-library overload set".
Measured, verbatim, for `ThrowIfEqual(std::string, std::string, "p")`:

```
modules/core/include/System/ArgumentOutOfRangeException.hpp:232:72: error:
    no matching function for call to 'to_string(std::__cxx11::basic_string<char>&)'
/usr/include/c++/13/bits/basic_string.h:4162:3: note: candidate: 'std::string std::__cxx11::to_string(int)'
/usr/include/c++/13/bits/basic_string.h:4162:17: note:   no known conversion for argument 1 from
    'std::__cxx11::basic_string<char>' to 'int'
… (nine candidates, each with its own "no known conversion" note)
```

The error is reported **inside sharp-runtime's own header**, names a libstdc++
internal header, and enumerates nine standard-library candidates — none of which
tells the caller what contract they violated.

### 2.4 Premise corrections to SR-AUD-091 as filed

1. **The surface is wider than the finding states.** The report reproduces with
   `OrderedOnly`, a synthetic comparison-only user type, which reads as an edge
   case. Measured, the rejected set also contains **`enum class`, `std::string`,
   `std::string_view` and every pointer type** — all ordinary, none synthetic.
   `ThrowIfEqual(std::string("a"), std::string("a"), "p")` is an obvious call and
   does not compile.
2. **`std::to_string` is not the only cause of the 99 failures.** 14 of them are
   `EqualityOnly`/`InequalityOnly` failing on the ordering operators, which is
   *correct behaviour*, not a defect. A repair that made those compile would be
   wrong. This distinction is what §4.2 turns into a second declared contract.
3. **The port already has one invisible, unintended behaviour split**: an
   *unscoped* enum compiles and prints its integer; a *scoped* enum does not
   compile at all. Nothing declares or intends that difference.
4. **The header carries a second undeclared requirement the finding does not
   mention**: `ThrowIfZero`, `ThrowIfNegative` and `ThrowIfNegativeOrZero` compare
   against `T{}`, so all three require `T` to be default-constructible.

---

## 3. The decision: DIAGNOSE **and** WIDEN

The finding offers two routes — "declare and diagnose the formatting requirement"
**or** "format optional actual values through a separate policy". The selected
repair is **both**, and the argument is compatibility-shaped rather than
preference-shaped.

**Diagnose alone is insufficient.** A `static_assert` that requires
`std::to_string` would freeze the domain at "arithmetic types and, by accident,
unscoped enums". The audit's own reference note records that .NET leaves
`ThrowIfEqual<T>` **unconstrained** and constrains the ordering helpers only to
`IComparable<T>`; neither requires a formatting interface in order to compare.
Freezing an arithmetic-only domain would convert an accidental narrowing into a
deliberate one, and would leave `ThrowIfEqual(std::string, std::string, …)` —
the concrete call NEXT.md §2.7 named — permanently rejected.

**Widen alone is insufficient.** Any widening has a boundary, and a type outside
it must be told so by sharp-runtime rather than by nine libstdc++ candidates.

**Widening is strictly additive.** The formatter is an ordered `if constexpr`
chain whose **first branch is the existing `std::to_string(v)` call**. Every one
of the 11 types that compiles today matches branch 1, takes exactly the code path
it takes today, and emits byte-identical text. No branch below it is reachable for
those types. This is the whole compatibility argument, and it is why branch order
is a correctness property of this design, not a style choice.

---

## 4. The selected contract

### 4.1 The formatting domain — first match wins

| # | Branch | Admits | Rendering |
|---|---|---|---|
| 1 | `std::to_string(v)` is valid | every arithmetic type; unscoped enums | **unchanged, byte-identical** |
| 2 | `v.ToString()` converts to `std::string` | `System::Decimal`, `TimeSpan`, `DateTime`, `Guid`, every `System::Object` subclass | the type's own .NET text |
| 3 | `T` is an enumeration | scoped enums (`enum class`) | `std::to_string` of the underlying integer |
| 4 | `T` converts to `std::string_view`, and `T` is not a pointer | `std::string`, `std::string_view`, user types with `operator std::string_view` | the text itself |
| 5 | `T` converts to `std::string`, and `T` is not a pointer | user types with `operator std::string` | the text itself |
| 6 | unqualified `to_string(v)` found by ADL converts to `std::string` | any user type that opts in | the user's own text |
| 7 | otherwise | — | `static_assert` naming all six shapes |

Branch-order rationale, each of which is load-bearing:

- **1 before everything**: compatibility, as argued in §3.
- **2 before 3–6**: `ToString()` is this repository's established rendering
  convention (`System::Object::ToString()` is the base declaration; `Decimal`,
  `TimeSpan`, `DateTime` and `Guid` all provide it) *and* it is what .NET's own
  `ArgumentOutOfRangeException` uses for the actual value. No type that compiles
  today has a `ToString()`, so this branch changes nothing existing.
- **3 before 6**: a scoped enum renders as its integer, which is exactly what an
  *unscoped* enum already renders as via branch 1. Putting the ADL branch first
  would make a scoped enum with a user `to_string` print its name while an
  unscoped enum with the same user `to_string` still printed its integer — an
  inconsistency created by branch 1's promotion accident. Consistency with shipped
  behaviour wins; the cost is that an enum cannot override its rendering through
  ADL, which is documented.
- **4 before 5**: `std::string` satisfies both; the `string_view` branch avoids a
  copy.
- **6 last**: it is the escape hatch, so it must not shadow a more specific rule.

### 4.2 The comparison domain — declared, not only implied

The audit report's "other missing assertions" section says the templates "promise
generic ordering but provide no concept, `static_assert`, or diagnostic". Each
guard now `static_assert`s the *exact* expression it evaluates, so
`EqualityOnly` on `ThrowIfLessThan` gets a sharp-runtime sentence before the
`no match for 'operator<'` noise instead of only after it:

| Guard | Asserted expression |
|---|---|
| `ThrowIfZero` | `value == T{}` |
| `ThrowIfNegative` | `value < T{}` |
| `ThrowIfNegativeOrZero` | `value <= T{}` |
| `ThrowIfGreaterThan` | `value > other` |
| `ThrowIfGreaterThanOrEqual` | `value >= other` |
| `ThrowIfLessThan` | `value < other` |
| `ThrowIfLessThanOrEqual` | `value <= other` |
| `ThrowIfEqual` | `value == other` |
| `ThrowIfNotEqual` | `value != other` |

The three unary guards' assertions also capture premise correction §2.4.4: they
name `T{}`, so a non-default-constructible `T` is now diagnosed as such.

**These are `static_assert`s in the body, not constraints on the declaration.**
A `requires`-clause on the template would be a better diagnostic — the call would
simply not be viable — but it changes the *declaration* of nine public templates.
`static_assert` declares and diagnoses the whole domain with **no signature
change**, which is the conservative choice this repository's approval boundary
calls for.

### 4.3 The rejected route, and its measured cost

NEXT.md §2.7's design sketch listed "`operator<<`-insertable types" as a branch.
**That is rejected, on a measurement.** Supporting `operator<<` in a header-only
`if constexpr` requires `std::ostringstream` to be a complete type at template
*definition* time, so the header must include `<sstream>` unconditionally.
Measured on a translation unit that includes only
`System/ArgumentOutOfRangeException.hpp`:

| Header set | Preprocessed lines | Δ |
|---|---|---|
| today | 61,507 | — |
| + `<string_view>` + `<type_traits>` | 61,509 | **+2** (both already arrive transitively via `<string>`) |
| + `<sstream>` | 64,326 | **+2,819 (+4.6 %)** |

`ninja -t deps` reports **404 translation units** in `build/` that depend on this
header. Branch 6 (ADL `to_string`) gives any user type the same expressive power —
including a type whose only current rendering is `operator<<`, which needs a
three-line `to_string` shim — at **+0 preprocessed lines**. The finding's own
second route is literally "format optional actual values through a separate
policy"; branch 6 *is* that policy. This is recorded as a correction to §2.7's
sketch rather than silently dropped.

### 4.4 The rejected *type*, and why rejecting it is the repair

Pointers are **deliberately excluded** from branches 4 and 5, so `const char*`,
`char*` and `int*` reach the `static_assert`. Two reasons, and the first is a
memory-safety one:

- `std::string_view(const char*)` on a null pointer is undefined behaviour.
  Admitting `const char*` would introduce a UB path into a header whose entire job
  is to report an argument error.
- `ThrowIfEqual(const char*, const char*, …)` compares **pointer identity**, not
  text. `ThrowIfEqual("a", "b", "p")` deduces `T = const char*` by array-to-pointer
  decay and would silently compare addresses. That is a caller bug, and the
  correct behaviour is to refuse it.

This converts today's accidental `to_string(int*&)` failure into a deliberate
sharp-runtime diagnostic that names `std::string_view` as the fix. The finding
explicitly permits this: "A C++ adaptation may intentionally narrow the contract,
but must declare and diagnose".

---

## 5. Public-source consequences, stated explicitly

- **Nothing that compiles today changes.** Not the accepted set, not the emitted
  text, not the exception type, not the parameter name, not the HResult, not
  `getActualValueProperty()`. Branch 1 guarantees it, and the after-matrix in §8
  measures it.
- **Previously ill-formed programs become well-formed.** This is a widening of a
  public compile-time contract. The consequence worth naming: a consumer that
  *detects* the current rejection — an SFINAE probe, a `requires` expression, or a
  build that relies on `ThrowIfEqual(std::string, …)` failing — would observe a
  different answer. No such consumer exists in this repository (all 121 first-party
  call sites are arithmetic; see §6), and the widening direction is the safe one,
  but it is a public-contract change and is recorded as one rather than as a pure
  bug fix.
- **No signature change**, so no overload-resolution change: each of the nine
  guard names has exactly one template and gains no constraint. Argument deduction
  is untouched.
- **No new symbol.** All nine guards are header-inline templates instantiated at
  their call sites; the only out-of-line symbols in this type are the six
  constructors in `ArgumentOutOfRangeException.cpp`, which are not edited. No
  object layout, vtable, `noexcept` or exception-specification change.
- **Compile-time cost: +2 preprocessed lines** (§4.3), plus one `if constexpr`
  chain evaluated once per instantiated `T`.

---

## 6. First-party call sites

121 calls across `modules/`, distributed
`ThrowIfNegative` 55, `ThrowIfGreaterThan` 17, `ThrowIfLessThan` 17,
`ThrowIfNegativeOrZero` 16, `ThrowIfLessThanOrEqual` 5,
`ThrowIfGreaterThanOrEqual` 4, `ThrowIfZero` 3, `ThrowIfEqual` 2,
`ThrowIfNotEqual` 2. **Every one instantiates an arithmetic `T`** — `intcs`,
`longcs`, `uintcs`, `SharpRuntime::bytecs` — so every one matches branch 1 and is
unaffected. This is also why the defect survived: the port never exercised its own
advertised generic surface, which is precisely what the tests audit report
(`ArgumentOutOfRangeExceptionTests.cpp.audit.md`) records.

---

## 7. Recorded, not repaired

- **Pass-by-value `T`.** All nine guards take `T value` (and `T other`) by value,
  so a non-copyable comparable type can never bind, regardless of formatting.
  Measured: `NonCopyableOrdered` fails all nine pairs. Repairing it means changing
  nine public template signatures to `const T&`, which is a public API change
  outside SR-AUD-091's premise. Documented in the header.
- **Enums cannot override their own rendering** through branch 6, by the
  branch-order decision in §4.1. Documented in the header.
- **`actualValue` is a `std::string`, not a typed object** — the `.cpp` audit
  report's own deferred API-compatibility question. Untouched.

---

## 8. Validation plan

1. **After-matrix**: rerun `build-probe/2253_run_matrix.sh` against the repaired
   header and require (a) all 99 previously-OK pairs still OK, (b) the newly
   supported types OK, (c) the deliberately rejected types failing **inside
   sharp-runtime's `static_assert`**, not in libstdc++.
2. **Runtime tests** in `modules/core/tests/System/`: for each newly supported
   shape, the exception type, `getParamNameProperty()`,
   `getActualValueProperty()`, the message text and the HResult.
3. **Byte-identical regression pins** for the arithmetic path.
4. **A tracked negative consumer fixture** for the deliberately rejected shapes —
   a `static_assert` in a body that is never instantiated enforces nothing, and
   only a per-site compile can tell the difference (the #2213 precedent).
5. **A tracked positive consumer fixture** for the newly supported shapes, so the
   widening is pinned from outside the library too.
6. **The full 38-executable gate**, not the owning module alone. §2.5 of the
   previous handoff records exactly why: this header is included by 404
   translation units.
7. **Selective components**, because production header code changes.

**Sanitizers are not applicable to the primary claim.** SR-AUD-091 is a
*translation* defect: the affected programs do not produce a binary. ASan, UBSan,
LSan and TSan cannot discriminate a program that does not compile. The one runtime
surface this slice adds — new formatting branches that build a `std::string` — is
covered by the runtime tests in step 2.
