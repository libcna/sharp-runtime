# Audit: `tests/integration/Task41Tests.cpp`

## Metadata

- Audit status: AUDITED (587 lines, 80 tests in 30 suites, full read).
- Runtime evidence: the focused pointer, globalization, fallback, component
  model, data-annotation, JSON, collection, and property-macro filter passed
  all 80 cases on 2026-07-25.

## Coverage observed

The test file checks practical `IntPtr` conversion/overflow behavior, basic
fallback singleton and exception paths, scalar attribute storage, a compact JSON
serializer happy-path set, and property macro expansion.  The pointer tests
correctly condition `ToInt32` overflow on native pointer width rather than
assuming a 64-bit host.

## Missing assertions and diagnostics

- `UIntPtr` has only construction/equality tests.  It lacks size, numeric
  conversion overflow, pointer conversion, arithmetic, comparison, formatting,
  and max-value assertions analogous to the `IntPtr` suite.
- `IntPtr::Add`/`Subtract` are checked only far from native bounds.  No test
  specifies behavior or diagnostics for pointer-sized overflow, where a C++
  adaptation must avoid assuming .NET’s implementation-defined native-int
  behavior.
- Fallback tests call the fallback objects directly; they do not prove that an
  encoder/decoder actually invokes the configured fallback, preserves fallback
  state, or reports invalid source positions.
- Data-annotation tests assert stored metadata only.  They do not validate
  values, error-message selection, range/length boundary failures, or regex
  behavior, so the public validation contract remains effectively untested.
- JSON option tests assert flags but not their observable parser/serializer
  effects: trailing commas, comments, case insensitivity, max depth, naming
  policies, ignore/order/converter attributes, and number handling have no
  end-to-end coverage.  The serializer tests contain only primitive/vector
  happy paths and one document non-null check.
- `Win32Exception` asserts that a message contains the code, but no stable
  platform diagnostic policy is tested; system error texts must not be made
  brittle across platforms.

## Required post-audit verification

Add boundary tests for both native-pointer types and end-to-end JSON option and
attribute cases.  For annotations, use representative values and exact
`System::Exception` types/messages only where the public contract guarantees
them.  Keep platform-derived Win32 messages semantically asserted rather than
string-equal across operating systems.

## Final assessment

The file gives broad smoke coverage and sound `IntPtr` essentials, but several
advertised JSON and validation features are currently represented only by
property-storage tests.  No new source defect is demonstrated from this file
alone.
