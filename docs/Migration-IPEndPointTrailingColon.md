<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — a trailing `:` with no port is rejected (ticket #2045)

*2026-08-19.* `System::Net::IPEndPoint::TryParse` treated an **empty** port field as port 0, so
`"1.2.3.4:"` parsed as `1.2.3.4:0` and `"[::1]:"` as `[::1]:0`. The text asserts that a port
follows and none does; the result was a port the caller never wrote. Both are now rejected.

Landed under `docs/StandingApprovals.md` **SA-5**. No signature, layout or vtable change.

---

## 1. The gate was stale, and the inference it deferred is now verified

#2045's recorded blocker is explicit:

> *".NET's own `IPEndPoint.TryParse` is believed to reject the latter because it runs
> `int.TryParse` over an empty span, but `/rv/tmp/runtime/src/libraries/` is ABSENT from this
> container, so that is inferred and not verified."*

`/rv/tmp/runtime` **is present**, and the inference is right — with one correction to its detail.
`IPEndPoint.InternalTryParse` (`IPEndPoint.cs:99-152`) locates the port field **structurally**:

```csharp
int addressLength = s.Length;  // If there's no port then send the entire string to the address parser
...
if (addressLength == s.Length) { result = new IPEndPoint(address, 0); return true; }
else { ... uint.TryParse(portSpan, NumberStyles.None, ...) ... }
```

So it is `uint.TryParse`, not `int.TryParse`, and the rejection comes from that call failing on an
empty span under `NumberStyles.None`.

## 2. What changed

| Input | Was | Is |
|---|---|---|
| `"1.2.3.4:"` | `1.2.3.4:0` | **rejected** |
| `"[::1]:"` | `[::1]:0` | **rejected** |
| `"1.2.3.4"` | `1.2.3.4:0` | **unchanged** |
| `"[::1]"`, `"::1"`, `"fe80::1%7"` | port 0 | **unchanged** |
| every form with a port | — | **unchanged** |

`TryParse` returns `false`; `Parse` throws `System::FormatException` with its existing message.

## 3. Why the guard is a flag and not `!portPart.empty()`

Both *"no colon at all"* and *"a colon with nothing after it"* leave the port text empty, and they
are **not the same input**: the first is a bare address and means port 0, the second is a
malformed endpoint. .NET distinguishes them structurally (`addressLength == s.Length`); this
parser's shape makes a `hasPortField` flag the same distinction, set in both branches — bracketed
and unbracketed — at the point a `:` is actually found.

A mutation reverting the guard to `!portPart.empty()` is caught, and so is one that stops either
branch setting the flag, and so is one that defaults it to `true` (which would reject every bare
address).

## 4. Evidence

Five mutations, four caught, one equivalence:

| Mutation | Result |
|---|---|
| the guard reverts to `!portPart.empty()` | caught |
| the bracketed branch stops setting the flag | caught (6 cases) |
| the unbracketed branch stops setting the flag | caught (6 cases) |
| the flag defaults to `true` | caught (4 cases) |
| **the explicit `portPart.empty()` rejection deleted** | **equivalence** |

The last is reported rather than papered over, and it is proven rather than assumed: the digit
loop runs zero times and `std::stoul("")` throws `std::invalid_argument`, which the existing
`catch (...)` already turns into `return false` — verified by probe. The line is kept because
.NET's `uint.TryParse` *returns* false rather than throwing, so an explicit rejection is the shape
of the reference, and because resting a parser's correctness on an exception thrown by a
conversion function is a subtler contract than a test. The note is at the site.

The first mutation was **invalid as first written** — reverting the guard leaves `hasPortField`
unused and `-Werror` rejects it — and was reformulated with a `(void)` rather than counted.

Two further rows came out of reading the reference and are now asserted here rather than left
implicit: `NumberStyles.None` forbids a sign and whitespace, so `"1.2.3.4:+80"` and `"1.2.3.4: 80"`
are rejected (this parser's digit loop already did); and a colon at position 0 is not a port
separator, since .NET requires `lastColonPos > 0` and here the empty address fails to parse — both
reject `":80"`, for their own reasons, and agree.

## 5. Downstream, measured

`cna` and `mobile-eggbert` reference `IPEndPoint` in **zero** code sites. Neither was modified. A
caller that relied on `"host:"` yielding port 0 must drop the trailing colon.
