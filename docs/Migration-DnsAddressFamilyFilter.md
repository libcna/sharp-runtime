<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `Dns` applies the address-family filter to a name, and a mismatched literal is empty (ticket #2046)

*2026-08-19.* `Dns::GetHostAddresses` and `Dns::GetHostEntry` filtered a **literal** by the
requested `AddressFamily` and did not filter a resolved **name** — one function answering the same
question two ways depending on its argument's shape. Both paths now match .NET.

Landed under `docs/StandingApprovals.md` **SA-5**. No signature, layout or vtable change.

---

## 1. The acceptance criterion's premise is false, and the reference is why

#2046 asks that the two doors *"answer a non-IP AddressFamily the same way for a name as for a
literal"*, and records that this *"requires a decision about WHICH way"*. Measured, **.NET does not
make them the same**, and no decision is needed because the reference states all three answers:

| Path | .NET | citation |
|---|---|---|
| `GetHostAddresses`, literal, family mismatch | `Array.Empty<IPAddress>()` | `Dns.cs:213` |
| `GetHostAddresses`, name, unservable family | `SocketError.AddressFamilyNotSupported` | `NameResolutionPal.Unix.cs:41-42` |
| `GetHostEntry`, literal | **no short-circuit at all** — reverse-resolve, then forward-resolve *with* the family | `Dns.cs:290-320` |

The name path's error is reached through `getaddrinfo` returning `EAI_FAMILY`. Verified directly
in this container: a `getaddrinfo` with `ai_family` of `AF_UNIX`, `AF_PACKET` or `99` all return
`-6`, *"ai_family not supported"*. .NET reaches the same code by a second route too — its native
`TryConvertAddressFamilyPalToPlatform` returns `EAI_FAMILY` for a family it cannot convert — and
both end at `SocketError.AddressFamilyNotSupported`, which is why this port may refuse directly
rather than calling the resolver to be told.

## 2. What changed

| Call | Was | Is |
|---|---|---|
| `GetHostAddresses("localhost", Unix)` | `127.0.0.1` | `SocketException(AddressFamilyNotSupported)` |
| `GetHostEntry("localhost", Unix)` | resolved normally | `SocketException(AddressFamilyNotSupported)` |
| `GetHostAddresses("127.0.0.1", Unix)` | `SocketException(HostNotFound)` | **empty vector** |
| `GetHostAddresses("127.0.0.1", InterNetworkV6)` | `SocketException(HostNotFound)` | **empty vector** |
| `GetHostEntry("127.0.0.1", Unix)` | `SocketException(HostNotFound)` | `SocketException(AddressFamilyNotSupported)` |
| `GetHostEntry("127.0.0.1", InterNetworkV6)` | `SocketException(HostNotFound)` | **unchanged** (§4) |
| everything with a matching or `Unspecified` family | — | **unchanged** |

## 3. This reverses a deliberate #2039 decision, and #2039 said why it might be wrong

#2039 chose `SocketException(HostNotFound)` over an empty vector, and its comment is explicit:

> *"a silent empty vector [is] indistinguishable from 'checked and found nothing'. … §7.3
> predicted an empty result for the `AddressFamily::Unix` row; that prediction was made without
> those tests in view and is corrected in §17.6 rather than followed."*

That is a good usability argument and it is **not .NET's**. `Dns.cs:213` returns
`Array.Empty<IPAddress>()`. So §7.3's prediction was right and its "correction" was the error —
the argument was made with the reference absent, which is the condition SA-5 exists to end.

`GetHostEntry` keeps throwing, and not as a compromise: it returns **one entry**, not a list, so
the empty answer is not available to it at all.

## 4. One residual difference, stated rather than glossed

`GetHostEntry(literal, <mismatched IP family>)` still raises `HostNotFound`. .NET does not
short-circuit a literal there: it reverse-resolves the address to a name and forward-resolves that
name with the family, so it could legitimately find an address of the requested family where this
port answers from the literal alone.

Reproducing that means a reverse DNS lookup on every literal — a network round trip this door has
never made. The difference is pinned by a test so it stays a decision rather than an oversight.

## 5. Evidence

Five mutations, **all caught**:

| Mutation | Caught by |
|---|---|
| the name-path gate removed from `GetHostAddresses` | 2 cases |
| the name-path gate removed from `GetHostEntry` | 1 case |
| `resolverCanServeFamily` also accepts `Unix` | 3 cases |
| the literal path throws again instead of returning empty | 3 cases |
| the error code becomes `HostNotFound` | 3 cases |

Five tests were rewritten and one deleted as superseded. Every one of them asserted something true
of the old two-way answer.

## 6. To migrate

* A caller passing a non-IP `AddressFamily` and expecting a resolved name back now gets
  `SocketException(AddressFamilyNotSupported)`. That call was answering the wrong question before.
* A caller relying on `GetHostAddresses(literal, wrongFamily)` **throwing** must now check for an
  empty result. This is the change most likely to be felt, and it is the one the reference is
  clearest about.

## 7. Downstream, measured

`cna` and `mobile-eggbert` reference `Dns::` in **zero** code sites. Neither was modified.
