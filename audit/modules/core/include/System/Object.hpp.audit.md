# Audit: `modules/core/include/System/Object.hpp`

## Metadata

- Audit status: AUDITED (140 lines, full read).
- Public API: abstract root object, default string/equality/hash behavior, and
  RTTI-backed `GetType()`.
- Validation: `./build/SharpRuntimeTests_Core_Base --gtest_filter='ObjectTests.*:TypeTest.*' --gtest_color=no`
  passed 47 tests on 2026-07-25.

## Assessment

The header accurately describes the intentional C++ adaptation: concrete
runtime classes must provide `GetTypeName()`, whereas C++ has no universal
object root.  The non-virtual `GetType()` correctly uses `typeid(*this)`, so
calls through an `Object*` retain the dynamic type.  `GetHashCode()` promises
a non-negative address-derived value, and the implementation maintains that
local contract by masking to `INT_MAX`.

The `Equals` documentation correctly calls out the equality/hash requirement
for derived classes.  No implementation defect was confirmed in this header.

## Other missing assertions and diagnostics

- The polymorphic RTTI tests cover one single-inheritance type only.  A future
  portability run should compile the dynamic-type path with the project's
  supported compiler/RTTI configurations rather than assuming one ABI's
  `type_info` behavior.
- The public contract has no explicit diagnostic for derived `Equals`
  overrides that do not also override `GetHashCode()`.  C++ cannot reliably
  enforce this in the base type; retain it as a documentation/review rule.

## Final assessment

The root-object adaptation is internally consistent and its dynamic-type path
is directly exercised.  The only confirmed test-quality issue in this shard is
recorded in `ObjectTests.cpp.audit.md` as SR-AUD-018.
