<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `ApplicationId` takes .NET's shape (ticket #2291)

*2026-08-18.* All four of the review's decisions, taken toward .NET.

Landed under `docs/StandingApprovals.md` **SA-8** and **SA-9**, with **SA-10** for the signature
changes, under SA-2's five conditions.

---

## 1. The four changes

| # | | Was | Is |
|---|---|---|---|
| 1 | **name validation** | `""` accepted silently | `ArgumentException` |
| 2 | **public key token** | `std::string`, stored verbatim, no clone | `std::vector<bytecs>`, **cloned in and out** |
| 3 | **Culture, ProcessorArchitecture** | `std::string` — absent = empty | `std::optional<std::string>` |
| 4 | **`ToString()`** | this port's own grammar, **token omitted** | .NET's grammar |
| | `GetHashCode()` | `noexcept`, hashed `Version::ToString()` | composes `Version::GetHashCode()`, not `noexcept` |

The review said (4) *"cannot be decided first"* because it depends on (2) and (3). It was decided
last, and all four landed together for that reason.

## 2. The token is bytes, and cloned at both ends

.NET's is `byte[]`, `(byte[])publicKeyToken.Clone()` on the way in (`ApplicationId.cs:19`) and
`=> (byte[])_publicKeyToken.Clone()` on the way **out** (`:34`) — a defensive copy on *every*
access. A `const&` return would have handed the caller the stored array and defeated the
constructor's own copy, so `getPublicKeyTokenProperty()` returns **by value**. A test mutates both
the caller's array and the returned one and asserts the stored token is untouched.

A `std::string` also could not carry binary key material: a test uses a token containing an
embedded NUL, which the old representation could not represent at all.

## 3. `ToString()` — two reference quirks transcribed rather than tidied

```
<name>[, culture="<c>"], version="<v>", publicKeyToken="<HEX>"[, processorArchitecture ="<a>"]
```

* `processorArchitecture` carries a **space before its `=`** where the other three keys do not.
  That is `ApplicationId.cs:63`, and it is reproduced deliberately because a caller may match on
  it.
* The token is emitted **even when empty**, because .NET's guard is a null test on the array and a
  zero-length array is not null.

The old text omitted the token entirely, so **two identities differing only by public key token
produced identical strings** — the reason this half of the finding existed.

## 4. `GetHashCode` — #2292 closed on the way past

It was `noexcept` while hashing `version_.ToString()`, which allocates, so an allocation failure
called `std::terminate` rather than propagating. It now composes `Version::GetHashCode()`, which
is both what .NET does (`Name.GetHashCode() ^ Version.GetHashCode()`) and allocation-free.

The `noexcept` is dropped anyway: a hash that composes another type's user-defined hash should not
promise more than that hash does.

**The hash still deliberately ignores the token, culture and architecture**, and .NET says why in
its own comment — *"purposely skipping … as they are less likely to make things not equal than
name and version"*. So two identities differing only by token are **unequal** and **hash the
same**, which is permitted; a test asserts both halves together so neither can be "fixed" in
isolation.

## 5. To migrate

```cpp
// before
ApplicationId id("token123", "MyApp", ver, "amd64", "neutral");
const std::string& c = id.getCultureProperty();
if (id.getPublicKeyTokenProperty() == "token123") { ... }

// after
const std::vector<SharpRuntime::bytecs> token{0xDE, 0xAD, 0xBE, 0xEF};
ApplicationId id(token, "MyApp", ver, "amd64", "neutral");   // literals still convert
const std::string c = id.getCultureProperty().value_or("");
if (id.getPublicKeyTokenProperty() == token) { ... }
```

A string literal still converts implicitly into the optional parameters, so a **construction** site
that supplies both components needs no edit beyond the token.

## 6. Downstream, measured

Neither `cna` nor `mobile-eggbert` mentions `ApplicationId` — **zero sites in both**. Neither
repository was modified.
