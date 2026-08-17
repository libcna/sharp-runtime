<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — response reads are bounded by `MaxResponseContentBufferSize` (ticket #2071)

*2026-08-17.* `HttpClientHandler`'s three response-reading paths accumulated **without bound**,
so a hostile or broken server could drive the client out of memory. They are now bounded.

Landed under `docs/StandingApprovals.md` SA-5 (the ceiling and its default are .NET's) and SA-3
(one private member on `HttpClientHandler`).

---

## 1. What was wrong

Three paths, none of them bounded:

* `recvAll` — a response with no `Content-Length` and no chunking, read until the peer hangs up;
* `recvExact` — bounded by the server's **claimed** `Content-Length`, a number the server never
  had to back with real bytes;
* the chunked reader — `std::stoul(chunkLine, nullptr, 16)` accepted a chunk size up to
  `SIZE_MAX`, so `FFFFFFFFFFFFFFF` was a request to accumulate eighteen exabytes.

## 2. What changed

| | Was | Is |
|---|---|---|
| default ceiling | none | **2,147,483,647 bytes** — .NET's `HttpContent.MaxBufferSize` |
| a `Content-Length` above the ceiling | accumulation began immediately | rejected **before any body byte is read** |
| an unbounded body above the ceiling | grew until memory ran out | rejected at the ceiling |
| a chunk size above the ceiling | attempted | rejected |
| the error | — | `HttpRequestException(HttpRequestError::ConfigurationLimitExceeded, …)`, naming the limit |
| responses under the ceiling | — | **unchanged** |

The default is .NET's, not one this port invented: `HttpClient.cs:149` assigns
`HttpContent.MaxBufferSize`, and `HttpContent.cs:25` defines it as `int.MaxValue`. A test asserts
the exact number, so a future change to something more "sensible" cannot happen quietly.

## 3. The new knob

```cpp
HttpClient client;
client.setMaxResponseContentBufferSizeProperty(4 * 1024 * 1024);   // 4 MiB
```

Validation is .NET's (`HttpClient.cs:117-131`): the value must be positive and at most
2,147,483,647, else `ArgumentOutOfRangeException`.

**Where it lives, and why it differs from .NET.** .NET carries this on `HttpClient`, because
.NET's handler *streams* and the client bounds it afterwards through `LoadIntoBuffer`. This
port's handler reads the whole body eagerly, so the value has to live where the bytes are
actually accumulated — `HttpClientHandler`. `HttpClient` forwards to it, so the knob has .NET's
name, .NET's default and .NET's validation at .NET's place.

A client built on a **custom** handler has nowhere to forward to: it validates the argument and
reports the default unchanged. That is honest rather than convenient — a custom handler does its
own reading, and this client cannot bound it.

## 4. To migrate

Nothing, unless you download responses larger than 2 GiB in one buffer, which never worked
reliably anyway. If you want a tighter bound — and a client talking to untrusted servers should
— set it explicitly.

## 5. Downstream, measured

Neither `cna` nor `mobile-eggbert` references `HttpClient` or `System::Net::Http` — **zero sites
in both**. Neither repository was modified.
