# Audit: `modules/buffers/include/System/Buffers/SearchValues.hpp`

## Metadata

- Audit status: AUDITED (98-line public header-only implementation, fully
  read).
- Validation: `SearchValuesTests.*:SearchValuesFactoryTests.*` passed 8/8
  within the complete 37/37 Batch16 focused filter in
  `SharpRuntimeTests_Buffers` on 2026-07-26.
- Reproducer: compiling
  `/tmp/sharp-runtimervc-searchvalues-equality-probe.cpp` with C++20 fails at
  construction and `Contains`: `std::hash<EqualityOnly>` is deleted. The type
  provides only `bool operator==`.
- Reference: local .NET `SearchValues.cs`, `SearchValues.T.cs`, and its
  exhaustive byte/char membership tests were reviewed.

## Assessment

For built-in hashable values the unordered-set implementation provides correct
membership and duplicate elimination, while returned value order is correctly
treated as unspecified by the source tests. Its public template documentation
and constructors promise a broader equality-based type surface than the chosen
container can instantiate.

## SR-AUD-077 — medium — SearchValues<T> documents equality-only support but imposes an undocumented std::hash requirement

The header says `T` must be equality-comparable and offers public generic
vector/initializer-list construction. Its `std::unordered_set<T>` backing also
requires a usable `std::hash<T>`. The standalone equality-only value type fails
to compile in both constructor and `Contains`, reporting deleted
`std::hash<EqualityOnly>`.

The current .NET base type is constrained by `IEquatable<T>` rather than a hash
contract. Its built-in public factories specialize byte/char/string selection,
but this C++ header explicitly broadens construction to arbitrary equality
comparable `T`; that exposed extension must either use equality-only storage,
accept a hasher template parameter, or state/enforce the additional requirement
with a concise public diagnostic.

## Other missing assertions and diagnostics

- The eight direct cases instantiate only built-in `int`, `uint8_t`, and
  `char`; no equality-only, custom-hash, move-only, throwing-hash, duplicate,
  empty, or large set is tested.
- No test asserts that `GetValues` is unordered/set-like, independent of the
  input vector after creation, or preserves membership after copy/move.
- Public constructors expose concrete creation where .NET reserves derived
  SearchValues construction to CoreLib and uses its static byte/char/string
  factories. The broader C++ API needs an intentional extensibility decision.
- The local source has optimized span search integration beyond `Contains`.
  This port offers only membership, so performance and `IndexOfAny`-family API
  breadth require a documented scope decision rather than inference from the
  type name.

## Final assessment

Built-in membership paths pass, but the advertised generic equality contract is
false at compile time for ordinary equality-only C++ values. No source or test
was modified during this audit.
