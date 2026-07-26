# Audit: `modules/core/tests/System/SingleTests.cpp`

## Metadata

- Audit status: AUDITED (174 lines, 102 tests, full read).
- Validation: `SingleTest.*` passed 102/102 in
  `SharpRuntimeTests_Core_Base` on 2026-07-25.

## Assessment

The suite gives broad normal-path coverage: ordinary classification and
arithmetic, signed-zero behavior for `Max`/magnitude APIs, basic parsing,
canonical special tokens, and one fixed-format example.  The prior parser
regressions are especially well documented.  However, every assertion around
the newly confirmed edge behavior uses a normal finite value, so the green
result cannot distinguish the current C++-library adaptation from the .NET
contract.

## Finding references

- **SR-AUD-021:** no assertion rejects an unknown format or verifies that a
  malformed precision raises `System::FormatException` rather than leaking a
  standard-library exception.
- **SR-AUD-029:** only `digits == 2` is covered; negative and greater-than-six
  rounding precision is not required to throw.
- **SR-AUD-030:** `IsPow2` covers normal values but not `Epsilon`, another
  subnormal power of two, or a non-power subnormal.
- **SR-AUD-031:** only a finite `ILogB(8)` is checked; zero, NaN, infinities,
  and the smallest subnormal are absent.
- **SR-AUD-032:** the Pi-scaled functions are entirely untested, including
  their exact turn-boundary and signed-zero behavior.
- **SR-AUD-033:** parse cases omit valid outer whitespace, thousands grouping,
  overflow-to-infinity, and whitespace-wrapped special tokens; formatting omits
  grouping and scientific exponent width.
- **SR-AUD-034:** `IsPositive`/`IsNegative` do not assert the sign-sensitive
  behavior of positive and negative NaN.

## Required post-audit verification

Add the precise vectors stated in the owning header report.  Use
`std::signbit` or raw bits for signed-zero results, exact exception types for
bad precision/formats, and direct expected text for invariant `N2`/`E2`.
Preserve the existing ordinary-value checks as complementary coverage.

## Final assessment

Good normal-path and prior-regression coverage, but missing high-information
boundary assertions allow six public `Single` parity defects to pass
unchallenged.
