<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `RNGCryptoServiceProvider` is sealed and obsolete (ticket #2399)

*2026-08-19.* `System::Security::Cryptography::RNGCryptoServiceProvider` now has the shape .NET
declares for it. **Two spellings are outlawed**, for two different reasons.

Landed under **SA-10** (a `[[deprecated]]` is a public signature change and lands under SA-2's five
conditions) with **SA-2**'s machinery for the seal. Split out of **#2398** deliberately: that ticket
was confined to one `.cpp` body and two message strings and outlawed nothing, where every part of
this one is a public source break or new public surface.

**Downstream, measured:** **zero** sites of any kind in `cna` and in `mobile-eggbert`. Downstream
record: **#2400**.

---

## 1. What .NET declares

```csharp
[Obsolete(Obsoletions.RNGCryptoServiceProviderMessage, DiagnosticId = "SYSLIB0023", ...)]
[EditorBrowsable(EditorBrowsableState.Never)]
public sealed class RNGCryptoServiceProvider : RandomNumberGenerator
{
    public RNGCryptoServiceProvider() : this((CspParameters?)null) { }
    public RNGCryptoServiceProvider(string str) : this((CspParameters?)null) { }
    public RNGCryptoServiceProvider(byte[] rgb) : this((CspParameters?)null) { }
    public RNGCryptoServiceProvider(CspParameters? cspParams) { ... }
}
```
`RNGCryptoServiceProvider.cs:8-16`; the message is `Obsoletions.cs:82`.

This port had a **non-final** class with **no deprecation** and only the implicit default
constructor.

---

## 2. The two breaks

### 2.1 The class is `final`

Deriving from it no longer compiles. Measured before landing: **zero derivations** first-party and
**zero sites of any kind** downstream, so nothing had to be migrated.

### 2.2 The class is `[[deprecated]]`

.NET's message is transcribed verbatim:

> `RNGCryptoServiceProvider is obsolete. To generate a random number, use one of the
> RandomNumberGenerator static methods instead.`

**Under this repository's `-Wall -Wextra -Werror`, a use is a hard ERROR, not a warning.** That is
what the attribute means here, and it is exactly the boundary **#2289** measured and the user then
approved when this repository took its first `[[deprecated]]`. A consumer that builds
warnings-as-errors and names this type stops compiling.

**What a caller does about it.** .NET's own message names the replacement: the
`RandomNumberGenerator` static members (`Fill`, `GetBytes`, `GetInt32`) or
`RandomNumberGenerator::Create()`. A consumer that must keep the legacy spelling for a while can
wrap its uses:

```cpp
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    RNGCryptoServiceProvider rng;
#pragma GCC diagnostic pop
```

That is what this repository's own single remaining use does. The suppression is a deliberate,
visible acknowledgement rather than a silent one — and **in this repository it is also the
evidence**: deleting it fails the build with the diagnostic, which demonstrates the deprecation
rather than asserting it. Same idiom as #2289.

---

## 3. Two of .NET's three missing constructors were added, and the third cannot be

`(string)` and `(byte[])` now exist. **They ignore their argument entirely, exactly as .NET's do** —
both chain to `this((CspParameters?)null)` there. A test asserts this rather than assuming it: two
generators built from the *same* bytes must not agree, or the argument is a seed and not an ignored
parameter.

Both are `explicit`. **That is the faithful translation, not a narrowing this port invented**: a C#
constructor never participates in an implicit conversion, so a non-`explicit` C++ constructor would
be *adding* a conversion .NET does not have.

**`(CspParameters?)` is deliberately absent.** `CspParameters` does not exist in this port, and
inventing it to carry a type whose only behaviour is
`if (cspParams != null) throw new PlatformNotSupportedException()` would be inventing public surface
rather than porting it. There is no expression that names a type which does not exist, so what is
pinned instead is **the shape such an overload would introduce** — a constructor reachable with a
null:

```cpp
static_assert(!std::is_constructible_v<RNGCryptoServiceProvider, std::nullptr_t>);
```

A later ticket that adds `CspParameters` trips that pin and has to justify adding the type first.

---

## 4. What is *not* a divergence, checked and recorded

.NET overrides `GetBytes(byte[], int, int)`, `GetBytes(Span)`, `GetNonZeroBytes(byte[])` and
`GetNonZeroBytes(Span)` to forward to its inner generator. This port overrides only
`GetBytes(std::vector<bytecs>&)` and inherits the rest, whose base implementations call that same
virtual — **behaviourally identical for every reachable call**. Recorded here so a later reader does
not "complete" a set that is already complete.

---

## 5. Testing

Negative consumer fixture `test/consumer/security_rng_cryptoserviceprovider_sealed_deprecated_negative.cpp`,
**three sites**, each compiled separately: deriving (rejected by the seal), naming the type
(rejected by the deprecation) and the `(string)` constructor (rejected by the deprecation, worth its
own site because a consumer migrating off the default constructor could plausibly reach for an
overload and believe it escaped the diagnostic). **Site 1 suppresses the deprecation on purpose**, so
that it fails for the seal and only for the seal; without that, the two reasons would be
indistinguishable and the site would pass for the wrong one. Fixture set **48 / 245 → 49 / 248**.

Six mutations, **all caught, and one of them by an instrument the test suite does not have.**
Removing the `[[deprecated]]` is **not** caught by gtest — a scoped suppression compiles perfectly
well when there is no diagnostic to suppress — and it **is** caught by the negative fixture, sites 2
and 3, which is the only instrument that can see a diagnostic at all. Un-sealing, making the
`(string)` constructor implicit and adding a null-accepting constructor are all caught at **compile
time** by `static_assert`, which is the only way C++ reports a shape. One mutation was **invalid as
first written and was reformulated rather than counted**: storing the `byte[]` argument in a member
without reading it is a no-op, not a seed.

The shipped case for this type was `EXPECT_EQ(buffer.size(), 24u)` on a buffer whose size was fixed
**before** the call, so it passed against a generator that wrote nothing. It was replaced.
