<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — a composed `Delegate` carries its concrete type (ticket #2271)

*2026-08-18.* `Delegate::Combine` and `Delegate::Remove` accepted operands of unrelated concrete
types and always produced a base `Delegate`. .NET refuses the mismatch and preserves the type.

Landed under SA-5 on the user's decision of the same date. A **narrowing**: two spellings that
compiled and ran now throw. **No signature, layout, vtable or `noexcept` change** — and, notably,
**no data member**: `sizeof(Delegate)` is unchanged.

---

## 1. What changed

| Call | Was | Is |
|---|---|---|
| `Combine(alpha, beta)` — different derived types | accepted, produced a base `Delegate` | **`ArgumentException`**, *"Delegates must be of the same type."* |
| `Remove(combined, foreign)` | returned the source unchanged | **`ArgumentException`** |
| `RemoveAll(combined, foreign)` | returned the source unchanged | **`ArgumentException`** |
| `Combine(Combine(a, b), c)` — all one type | worked | **works** |
| `Combine(nullptr, b)`, `Combine(b, nullptr)` | returned `b` | **unchanged** |
| `Remove(nullptr, v)`, `Remove(s, nullptr)` | — | **unchanged** |
| anything using plain `Delegate` on both sides | — | **unchanged** |

The message is `MulticastDelegate.CoreCLR.cs:212-220` and `Delegate.cs:158-169` transcribed;
`Strings.resx:310-312` gives the sentence verbatim.

## 2. Why both halves had to land together

The ticket recorded that a same-type guard **alone** would break the chained form
`Combine(Combine(a, b), c)`: step one returns a multicast whose own `typeid` is `Delegate` rather
than the operands' type, so step two would compare `Delegate` against `C` and reject a
combination .NET accepts. That is why the finding could not be split.

## 3. No data member was needed

A multicast delegate's type **is** the type of its entries — and `Combine` itself guarantees they
are uniform, because it refuses to build a mixed list. So the type can be **read** from the
invocation list rather than stored beside it:

```cpp
const std::type_info& effectiveDelegateType(const Delegate& d, const InvocationList& list) {
    return list.empty() ? typeid(d) : typeid(*list.front());
}
```

`sizeof(Delegate)` is unchanged and this is not an SA-3 change. A mutation that makes a multicast
report its own `typeid` instead breaks the chained-form test **and three pre-existing
`MulticastDelegateTests`**, which is what shows the derivation is load-bearing rather than
decorative.

## 4. The null ordering is .NET's, deliberately

.NET checks the types *inside* `CombineImpl`, which a null `a` never reaches — so
`Combine(nullptr, b)` returns `b` **unchecked**. `Remove`'s check likewise runs after both null
tests. Both orderings are asserted so they cannot drift.

## 5. To migrate

If two delegates in your code have different concrete types and you were combining them, that was
never meaningful — the result could only be invoked through the base. Give them a common type, or
keep them in separate delegates. Plain `Delegate` instances all share one type, so the narrowing
bites only across two **different derived** types.

## 6. Downstream, measured

Neither `cna` nor `mobile-eggbert` calls `Delegate::Combine`, `Delegate::Remove` or
`Delegate::RemoveAll` — **zero sites in both**. Neither repository was modified.
