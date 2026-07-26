# Audit: `modules/core/tests/System/SystemAttributeTests.cpp`

## Metadata

- Audit status: AUDITED (276-line mixed Core.Base attribute fixture, fully
  read).
- Validation: its selected suites passed within the 77/77 focused Core.Base
  attribute filter on 2026-07-26.
- Reference basis: local .NET `Attribute.cs`, `AttributeTargets.cs`,
  `AttributeUsageAttribute.cs`, `ObsoleteAttribute.cs`, and loader attribute
  sources.

## Findings

The test asserts direct construction of `Attribute`, identity inequality of two
base instances, and equality/hash behavior derived from addresses
(`SystemAttributeTests.cpp:31-76`), so it preserves SR-AUD-114 rather than
testing current .NET's abstract fieldwise behavior.  Its `ObsoleteAttribute`
checks also make default nullable properties appear as empty strings
(`:170-204`), preserving SR-AUD-116.  The fixture only creates marker objects;
it contains no declaration-level compiler test for `ObsoleteAttribute` or
deprecated loader values, leaving SR-AUD-115 and SR-AUD-117 unobserved.

## Other missing assertions and diagnostics

- `AttributeTargets` tests sample only Assembly/Class/Method and a few flag
  operations, not the full value matrix or `All` bit mask.
- `AttributeUsageAttribute` tests cover state storage but not a linked
  consumer, invalid/exotic bit combinations, metadata attachment, multiplicity
  enforcement, or inheritance.
- CLS, all marker attributes, and loader optimization are tested as ordinary
  C++ objects.  No test shows C++ declaration syntax, metadata discovery,
  serialization, params expansion, context/thread isolation, or loader effect.
- It duplicates portions of seven dedicated fixtures, which can conceal a
  missing behavior behind repeated constructor-only coverage.

## Final assessment

The suite provides useful scalar smoke coverage, but several green assertions
codify known .NET divergences rather than detecting them.  No source or test
was modified during this audit.
