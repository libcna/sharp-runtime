<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `System::UnitySerializationHolder` is removed (ticket #2281)

*2026-08-18.* The type published a shape that was neither .NET's nor internal, existed only to
serve a mechanism this port permanently does not implement, and had no production consumer. It is
gone.

Landed under `docs/StandingApprovals.md` **SA-9** with SA-2's five conditions. **This decreases the
test count by 15**, which is the removed suites and nothing else.

---

## 1. A premise correction first

Both the ticket and the old header stated that *"in .NET the type itself is `internal`"*.

**It is not.** `UnitySerializationHolder.cs:15` declares
`public sealed class UnitySerializationHolder : ISerializable, IObjectReference`, with an explicit
comment on the line above: *"Needs to be public to support binary serialization compatibility"*.
What **is** `internal` there is only the `NullUnity` constant.

So the reason for removal is different from — and better than — the one the ticket gave.

## 2. The real reason

.NET's own summary says the type *"only exists for compatibility with .NET Framework"*. It carries
`[Obsolete(LegacyFormatterMessage)]`, and **every one of its public members takes
`SerializationInfo` and `StreamingContext`**:

```csharp
public UnitySerializationHolder(SerializationInfo info, StreamingContext context)
public void GetObjectData(SerializationInfo info, StreamingContext context) => throw new NotSupportedException(...)
public object GetRealObject(StreamingContext context)
```

`CLAUDE.md` records serialization infrastructure (`[Serializable]`, `SerializationInfo`) as a
**permanent deviation**, and BinaryFormatter is not implemented. The mechanism this type exists to
serve does not exist here and never will.

What the port published instead was a third thing — neither .NET's shape nor internal: a public
`NullUnity`, a raw `(intcs, string)` constructor, and getters for both fields, so an ordinary
caller could fabricate a holder state .NET only ever builds from a serialization stream.

The review's option B — *"retain recognizable compatibility signatures"* — would have required
adopting `SerializationInfo` and `StreamingContext`, i.e. reversing that permanent deviation. It
was correctly identified as unavailable, and it stays unavailable.

## 3. To migrate

The only observable behaviour the holder ever produced is `DBNull::Value()`, which is untouched
and is a real .NET public singleton. If you constructed a holder to obtain it, ask for it
directly:

```cpp
// before
System::UnitySerializationHolder h(System::UnitySerializationHolder::NullUnity);
auto& v = h.GetRealObject();

// after
auto& v = System::DBNull::Value();
```

## 4. The test-count decrease

The repository gate moves **17,298 → 17,283**. That is exactly 15 cases: the 7 in
`UnitySerializationHolderTests.cpp` and the 8 in `SystemTypesRemainingTests.cpp`'s
`UnitySerializationHolderTests` section. No other executable's count moved, and nothing was
disabled, weakened or skipped.

## 5. A note on the negative fixture

It deliberately does **not** assert the `DBNull::Value()` survivor, which would have been the
natural thing to pin. Including `System/DBNull.hpp` pulls in `Decimal.hpp` through
`IConvertible.hpp`, and `Decimal`'s `unsigned __int128` is rejected under the fixture checker's
`-Wpedantic -Werror` — so the fixture's **baseline** would not compile, and a fixture whose
baseline is broken cannot attribute any site's rejection to its own source. `DBNull` is covered by
the repository suites instead, which still run 9 cases over it.

## 6. Downstream, measured

Neither `cna` nor `mobile-eggbert` mentions `UnitySerializationHolder` — **zero sites in both**.
Neither repository was modified. The downstream ticket is **#2373**.
