# Audit: `modules/core/include/System/Attribute.hpp`

## Metadata

- Audit status: AUDITED (84-line public base type, fully read).
- Validation: the focused Core.Base attribute filter passed 77/77 on
  2026-07-26; `/tmp/sharp-runtimervc-attribute-audit-probe` prints
  `same_value_cls_equals=0` and `empty_marker_equals=0`.
- Reference basis: local .NET `System/Attribute.cs:14-141`.

## SR-AUD-114 — medium — Attribute is constructible and uses identity equality rather than .NET's abstract fieldwise attribute contract

`Attribute` is public and directly constructible (`Attribute.hpp:24-27`), even
though current .NET declares it abstract with a protected constructor.  More
importantly, its default `Equals` compares addresses and `GetHashCode` hashes
the address (`:40-52`).  Current .NET first requires the same runtime type and
then compares every instance field; its hash is derived from the first
non-array field (or the type).  Thus two independently constructed
`CLSCompliantAttribute(true)` objects and two empty `FlagsAttribute` objects
produce false in the direct probe, whereas their .NET counterparts compare
equal.

The green `AttributeTests` fixture explicitly requires `Attribute a, b` to be
unequal, locking both the instantiability and identity fallback.  It does not
exercise equal-valued derived attributes, unequal field values, type mismatch,
or the equality/hash relationship.

## Other missing assertions and diagnostics

- `TypeId`'s `std::type_info` adaptation is reasonable in C++, but no test
  covers distinct derived types or its lifetime/identity contract.
- No regression vector covers the .NET fieldwise array comparison rule or an
  attribute subclass which intentionally overrides `Match`.

## Final assessment

The virtual shape is usable by C++ subclasses, but its default observable
semantics differ materially from the base class it claims to port.  No source
or test was modified during this audit.
