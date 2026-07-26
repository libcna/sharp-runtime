# Audit: `modules/core/tests/System/ValueTupleTests.cpp`

## Metadata

- Audit status: AUDITED (169 lines, 26 direct tests, fully read).
- Validation: the combined direct and supporting aggregate `ValueTuple*`
  filter passed 53/53 in `SharpRuntimeTests_Core_Base` on 2026-07-26.

## Assessment

The direct suite covers ordinary construction, equality, hash consistency,
lexicographic comparison, text rendering, and relational operators across
arities one through seven.  It improves earlier arity coverage for hashes,
comparison, and text.  The separate pending aggregate file supplies normal
zero-/eight-element factory smoke cases; it is validation evidence only and
is not marked audited until its complete file-wide review.

## Finding references

- **SR-AUD-046 (extended):** all direct comparison/equality inputs are
  `int`/`std::string`, so they cannot reveal that `ValueTuple` uses raw C++
  operators.  The audit probe demonstrates that a NaN component compares equal
  to a finite component and makes a tuple unequal to itself, unlike the local
  .NET default comparer/equality-comparer contract.

## Other missing assertions and diagnostics

- No direct test instantiates `ValueTuple8`, `MakeValueTuple` factories, or
  the zero-element `ValueTuple`; these only appear in the supporting aggregate
  smoke cases.
- No suite covers NaN, signed zero, heterogeneous string locale output,
  nested `Rest`, an unsuitable `Rest`, a move-only element, or a type that has
  only a custom comparison/equality policy.
- Hash tests prove equality gives an equal hash, but do not check that NaN
  equality and hashing agree or that `intcs` hash narrowing remains stable.
- Tests assert only the sign/zero of ordinary `CompareTo`; they do not verify
  `-1`/`0`/`1` behavior at a custom-comparer or floating boundary.

## Final assessment

The ordinary integral regression coverage is useful, but it does not exercise
the generic comparer contract that determines tuple parity for floating values.
No test was modified during this audit.
