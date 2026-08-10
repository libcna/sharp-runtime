# Audit: `modules/core/src/System/ArgumentNullException.cpp`

## Metadata

- Audit status: AUDITED (49-line implementation, fully read).
- Validation: the three-fixture argument-exception filter passed 64/64 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.
- Reproducer: the ASan/UBSan `null` mode of
  `/tmp/sharp-runtimervc-argumentexception-audit-probe.cpp` faults in
  `makeMsg`; its normal mode prints the doubled `item` marker.

## Assessment

Every constructor correctly assigns `E_POINTER`, but `makeMsg` assumes its
`const char*` is non-null and two constructors precompose a suffix already
owned by `ArgumentException(message, paramName)`.  The safe fallback in the
base constructor is consequently unreachable for the null pointer case.

## Finding references

- **SR-AUD-089:** unchecked C-string concatenation in `makeMsg` reaches an
  ASan-confirmed null read before exception construction.
- **SR-AUD-090:** `makeMsg` and the base two-argument constructor both append
  `(Parameter 'name')`, producing an observable duplicate diagnostic.

## Other missing assertions and diagnostics

- No test invokes either C-string or `std::string` parameter constructor with
  an empty/null name and checks the one-marker/zero-marker rule.
- Custom-message constructors are only substring-tested; they do not establish
  whether a null `message` C string follows .NET's default-message behavior or
  safely maps to an empty native string.
- The implementation has no shared message factory with ArgumentException, so
  future wording updates can reintroduce suffix ordering or duplication drift.

## Final assessment

The HResult and standard constructors are correct, but raw C-string handling
has a confirmed memory-safety failure and malformed diagnostic composition.
No source or test was modified during this audit.
