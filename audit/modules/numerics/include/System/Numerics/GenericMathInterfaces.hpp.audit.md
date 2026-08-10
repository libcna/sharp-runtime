# Audit: `modules/numerics/include/System/Numerics/GenericMathInterfaces.hpp`

## Metadata

- Audit status: AUDITED (generic-math compatibility stubs).
- Direct probe: `/tmp/sharp-runtime-numerics-audit/generic_math_link.cpp`
  compiles but fails to link `IMinMaxValue<int>::MinValue`,
  `IAdditiveIdentity<int>::AdditiveIdentity`, and
  `IMultiplicativeIdentity<int>::MultiplicativeIdentity` on 2026-07-27.

## Assessment

The hierarchy can be instantiated, but static members advertised by the
identity, min/max, and function-family interfaces are declarations without
definitions. A consumer therefore gets a delayed unresolved-symbol failure
rather than an implemented API or a documented compile-time unsupported path.
The checked-in integration test only instantiates the hierarchy and cannot
observe this failure. This is SR-AUD-278.

### SR-AUD-278 — medium — Generic-math static interface members are unresolved link-time stubs

The reproduction compiles solely against the published header but the linker
reports all three referenced static members as undefined. The API therefore
looks available to a consumer until final linkage.

## Finding references

- SR-AUD-278 — medium — public generic-math static members are link-time stubs.

## Other missing assertions and diagnostics

- Add compile-and-link probes for every declared static member and for a
  representative type satisfying each interface family.
- Either provide constrained definitions or make the header explicitly
  nonfunctional/unavailable instead of declaring callable unresolved symbols.

## Final assessment

SR-AUD-278 applies. No implementation was changed during this audit.

---

## Post-audit correction and remediation record — ticket #2168 (2026-08-10)

*Appended by review #2167. The original report above is retained verbatim; nothing in it is
rewritten.*

**Premise corrected — the surface is 44 members, not 3.** The probe demonstrated
`IMinMaxValue<int>::MinValue`, `IAdditiveIdentity<int>::AdditiveIdentity` and
`IMultiplicativeIdentity<int>::MultiplicativeIdentity`. Enumerated from the header, there are
**37 static function declarations across 9 interface templates** — `IMinMaxValue` (2),
`IAdditiveIdentity` (1), `IMultiplicativeIdentity` (1), `ITrigonometricFunctions` (12),
`IHyperbolicFunctions` (6), `ILogarithmicFunctions` (4), `IExponentialFunctions` (3),
`IPowerFunctions` (1), `IRootFunctions` (4), `IFloatingPointConstants` (3) — plus 7 more reachable
through the `IBinaryFloatingPointIeee754` aggregate. Every one had the identical defect.

**Context the report does not cite, and which decided the repair.** The reduction this header
implements is already settled and documented in three places in this repository:
`System/Half.hpp:43-45` ("Generic math interface conformance … is out of scope, consistent with
this codebase's position on C# generic-math machinery elsewhere"),
`System/Numerics/DivisionRounding.hpp`, and this header's own preamble ("these stubs exist for API
name compatibility"). So the report's first option — "provide constrained definitions" — would
have meant inventing 44 numeric semantics from recollection with `/rv` absent, and would have
contradicted a settled position. The second option was taken.

**Repair.** Every declared static is `= delete`, and each interface's doc-comment names the C++
spelling to use instead. The failure moves from an unresolved symbol at final link — arbitrarily
far from the call, naming a mangled template specialisation — to a `use of deleted function`
diagnostic at the call site. **No program that builds today can be affected**: a call that exists
today already fails to link.

**Evidence.** `test/consumer/numerics_generic_math_negative.cpp`, 12 sites, every one rejected,
baseline clean — a compile check, because a runtime test cannot assert that something does not
compile. Undeleting `MinValue` makes site 1 compile again while the other 11 stay rejected.
`modules/numerics/tests/System/Numerics/GenericMathInterfacesTests.cpp` adds 6 tests for the
assertable half: `Radix` still 2 (a `static constexpr` initialiser, never part of this finding),
the hierarchy and all 13 operator interfaces plus the aggregate still instantiate, a derived
type's own statics still hide the deleted base members, and the documented replacements produce
the claimed values.

**Status:** `confirmed` → **`remediated`**. See `docs/SystemNumericsNamespaceReviewPlan.md` §4.7.
