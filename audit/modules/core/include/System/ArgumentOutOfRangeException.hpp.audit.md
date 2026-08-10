# Audit: `modules/core/include/System/ArgumentOutOfRangeException.hpp`

## Metadata

- Audit status: AUDITED (245-line public declaration/templates, fully read).
- Validation: the three-fixture argument-exception filter passed 64/64 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.
- Compile reproducer: compiling
  `/tmp/sharp-runtimervc-argumentoutofrange-generic-probe.cpp` fails in
  `ThrowIfGreaterThan(OrderedOnly{1}, OrderedOnly{0}, "value")` because
  `std::to_string(OrderedOnly)` has no overload.
- Reference: local .NET `ArgumentOutOfRangeException.cs` leaves
  `ThrowIfEqual<T>` unconstrained and constrains ordering helpers only to
  `IComparable<T>`; it does not require a formatting interface to compare.

## Assessment

The declaration covers the principal constructor and static guard shape, and
the actual-value constructor has a documented message-order policy.  Every
template guard nevertheless formats its values with `std::to_string`, adding a
hidden arithmetic-only constraint beyond the declared equality/ordering
requirements.

## SR-AUD-091 — medium — generic ArgumentOutOfRange guards silently require `std::to_string` in addition to their declared comparison contract

`ThrowIfGreaterThan`, `ThrowIfGreaterThanOrEqual`, `ThrowIfLessThan`, and
their equality counterparts describe `T` as totally ordered or
equality-comparable.  Their throwing branches instantiate `std::to_string`
for `value` and sometimes `other`; C++ instantiates that expression with the
function template, so even an `OrderedOnly` type with all required comparison
operators cannot call the API.  The reproducer fails at the undeclared
formatter, not at a clear public constraint diagnostic.

Current .NET likewise permits arbitrary equality-comparable `T` for
`ThrowIfEqual` and only comparison-capable `T` for ordering helpers.  A C++
adaptation may intentionally narrow the contract, but must declare and
diagnose the formatting requirement or format optional actual values through a
separate policy.

### REMEDIATED — #2253 (review) and #2254, 2026-08-10

`docs/CoreArgumentOutOfRangeGuardDomainPlan.md`. **Both offered routes were taken
at once**: the guards now share one private ordered `if constexpr` formatter
(*format through a separate policy*) and each carries a `static_assert` naming the
one comparison expression it evaluates plus a rendering assertion naming the six
supported shapes (*declare and diagnose*).

**Measured, one `(type, guard)` pair per translation unit**, 22 candidate types x
9 guards (`build-probe/2253_probe1_matrix.log` -> `2253_probe2_after.log`, plus
`2253_probe3_extension.log` for the two branches no original candidate reached):

| Measure | Before | After |
|---|---|---|
| pairs accepted | 99 | **135** |
| pairs rejected | 99 | 63 |
| rejections inside libstdc++ | 99 | **0** |
| previously accepted pairs regressed | — | **0** |

Compatibility is structural: the formatter's **first** branch is the same
`std::to_string(value)` call, so every previously accepted type takes the identical
path and emits byte-identical text. All 121 first-party call sites instantiate an
arithmetic `T`.

**Four premise corrections to this report.**

1. The rejected surface is much wider than the `OrderedOnly` reproducer above
   suggests. `enum class`, `std::string`, `std::string_view` and every pointer type
   were also rejected — `ThrowIfEqual(std::string("a"), std::string("a"), "p")` did
   not compile.
2. **14 of the 99 failures are not this finding.** `EqualityOnly` on the six
   ordering guards, and `InequalityOnly` on the eight guards needing `==`/`<`/`<=`/
   `>`/`>=`, fail on an operator the type genuinely lacks. That is correct, and a
   repair that made them compile would be wrong. Separating the two causes is what
   turned the report's "no concept, `static_assert`, or diagnostic" remark into a
   second declared contract.
3. The port already carried an invisible, unintended split: an **unscoped**
   enumeration compiled and printed its integer (integral promotion selects
   `std::to_string(int)`), while a **scoped** one did not compile at all.
4. The report does not mention a second undeclared requirement that is also
   present: `ThrowIfZero`, `ThrowIfNegative` and `ThrowIfNegativeOrZero` compare
   against `T{}`, so all three require `T` to be default-constructible.

**Two deliberate narrowings, both declared and both diagnosed.** Raw pointers are
excluded — a `std::string_view` built from a null pointer is undefined behaviour,
and `ThrowIfEqual("a", "b", "p")` deduces `T = const char*` by array-to-pointer
decay and would compare addresses rather than text. And a comparison-only type
with no rendering, including this report's own `OrderedOnly`, is **still rejected**
— by a sentence naming the six supported shapes and the ADL `to_string` extension
point it can opt in through, instead of by nine `std::to_string` candidates in
`<bits/basic_string.h>`.

**`operator<<` was rejected on a measurement**, not a preference: supporting it
forces `<sstream>` into a header that **404** translation units in this build
depend on, measured at +2,819 preprocessed lines (+4.6 %) per unit, where the ADL
branch reaches the same types for +0.

**Recorded, not repaired:** all nine guards take `T` **by value**, so a
non-copyable comparable type can never bind regardless of rendering. That is a
public signature question (`const T&`), outside this finding's premise, and is
documented in the header.

No signature, layout, vtable, `noexcept` or symbol change; no new `SR-AUD-*`
identifier. Sanitizers are inapplicable — a translation failure produces no binary.

## Other missing assertions and diagnostics

- All tests use integers plus one `double`; none compiles a comparison-only,
  equality-only, enum, `std::string`, custom formatter, NaN, signed-minimum,
  or unsigned-maximum input. **Addressed by #2254**: `ArgumentOutOfRangeGuardDomainTests`
  adds 26 tests covering every formatter branch, scoped enumerations with negative
  underlying values, `std::string` with an embedded NUL, `std::string_view` outliving
  its source, an unsigned maximum, and the guard-does-not-fire case; and
  `test/consumer/core_argument_out_of_range_guard_domain_negative.cpp` compiles the
  rejected shapes per site. NaN and signed-minimum inputs remain uncovered.
- The template comments promise generic ordering but provide no concept,
  `static_assert`, or diagnostic naming `std::to_string` as the extra
  requirement. **Addressed by #2254**, with `static_assert` rather than a
  `requires`-clause: a constraint on the declaration would be the better diagnostic
  but changes the signature of nine public templates, which needs approval this
  finding does not carry.
- No test checks `actualValue`/message formatting for floating special values,
  exact parameter suffix order on every helper, empty parameter names, or
  comparisons with culturally formatted values.

## Final assessment

Numeric calls pass, but the advertised generic guard surface has a confirmed
compile-time restriction that is neither documented nor intentional.  No
source or test was modified during this audit.

**Status: REMEDIATED (#2254, 2026-08-10).** The restriction is now both documented
and intentional where it remains, and removed where it should not have existed.
