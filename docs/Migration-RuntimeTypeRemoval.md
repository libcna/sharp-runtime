<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `System::RuntimeType` is removed (ticket #2334)

*2026-08-18.* `System::RuntimeType` was a public `enum class` with six values, occupying the name
of a .NET type it did not correspond to, used by nothing. It is gone.

Landed under `docs/StandingApprovals.md` **SA-9**, whose rule is that a type existing only because
.NET has one wears .NET's public shape — and that **members .NET does not have are removed**. With
SA-2's five conditions. **This decreases the test count by 8**, which is the removed test file and
nothing else.

---

## 1. What it actually was

.NET's `RuntimeType` is `internal sealed class RuntimeType : TypeInfo`
(`RuntimeType.BoxCache.cs:11` and siblings) — **not public API at all**, and not an enumeration.

Ticket #2333 measured and withdrew the two claims that had stood in the header:

* it is not a counterpart of any .NET type;
* the six values — `None`, `Primitive`, `ValueType`, `ReferenceType`, `Array`,
  `GenericParameter` — were **not** "documented internal constants used in CoreCLR"; they were
  this port's own invention.

## 2. Why removal rather than a rename

The review offered three options: remove, rename out of the .NET name, or keep and document.

Renaming exists to leave a migration target, and **there was nothing to migrate**: measured, the
only file in this repository that included the header was its own test, and neither `cna` nor
`mobile-eggbert` mentions the name. Keeping it would mean permanently occupying a .NET name with
invented values — and since `CLAUDE.md` lists reflection as a permanent deviation, the .NET class
whose name it held is never going to be ported and will never need the name back. That argument
cuts *for* removal as much as for keeping: nothing forces the name free later, so the squatting
would be permanent rather than temporary.

## 3. To migrate

If you used `System::RuntimeType`, you were using a port-local classifier with no .NET meaning.
Define your own enum; the six values are reproduced here for convenience:

```cpp
enum class TypeCategory { None = 0, Primitive = 1, ValueType = 2,
                          ReferenceType = 3, Array = 4, GenericParameter = 5 };
```

**`System::RuntimeTypeHandle` is untouched** — it is a real .NET public type, and the two names are
adjacent enough that a careless sweep could have taken both. The negative fixture asserts it still
works.

## 4. The test-count decrease

The repository gate moves **17,306 → 17,298**. That is exactly the 8 cases of
`modules/core/tests/System/RuntimeTypeTests.cpp`, which asserted the six integer values and
nothing else. No other executable's count moved, and nothing was disabled, weakened or skipped.

## 5. Downstream, measured

Neither `cna` nor `mobile-eggbert` mentions `RuntimeType` — **zero sites in both**. Neither
repository was modified. The downstream ticket is **#2372**.
