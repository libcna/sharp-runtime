<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — a request may be sent once, a status code is 0..999 (tickets #2067, #2069)

*2026-08-17.* Two `System.Net.Http` shapes gain the validation .NET has.

* **#2067** — `HttpClient` sent the same `HttpRequestMessage` as many times as you asked.
  `sizeof(HttpRequestMessage)` grows **192 → 200** under `docs/StandingApprovals.md` SA-3.
  **Downstream consumers must be recompiled.**
* **#2069** — `HttpResponseMessage` accepted any number as a status code. No layout change.

Both landed under SA-5 for the behaviour.

---

## 1. #2067 — send once

| | Was | Is |
|---|---|---|
| `client.Send(request)` twice with one message | both succeeded; the **same object** reached the handler twice | the second raises `InvalidOperationException` |
| the message afterwards | no observable state | `getWasSentProperty()` is `true` |
| a second `HttpClient` sending the same message | succeeded | raises — the flag belongs to the **message** |
| a different message | — | unchanged |

The message is .NET's own: *"The request message was already sent. Cannot send the same request
message multiple times."* (`HttpClient.cs:745-751`).

**Why it matters beyond tidiness.** The second send reuses a content object the first send may
already have consumed, and both sends share one headers map that the first handler may have
mutated. .NET has refused this since its first version.

The claim is atomic. .NET uses an interlocked compare-and-exchange
(`HttpRequestMessage.cs:26,173`) precisely so two concurrent sends cannot both win;
`std::atomic_flag::test_and_set` is the same operation, and a test releases eight threads at one
starting line, two hundred times, to assert it rather than trust the type name.

**To migrate:** build a new `HttpRequestMessage` per send. If you were reusing one to retry, the
retry was already sharing consumed content with the original.

## 2. #2069 — the status-code domain

| Status code | Was | Is |
|---|---|---|
| `-1`, `-1000`, `1000`, `99999` | constructed; `IsSuccessStatusCode` false | `ArgumentOutOfRangeException` |
| `0`, `1`, `100`, `599`, `998`, `999` | constructed | **unchanged** |
| `setStatusCodeProperty` with an out-of-range value | accepted | raises, and the old value survives |

The bound is **999, not 599**. RFC 9112 §4 makes a status code three digits, and .NET accepts
every three-digit value rather than only the registered ranges — `0` included. Both checks, in
both places, are transcribed from `HttpResponseMessage.cs:152-159` and `:65-76`.

**The wire path cannot be affected.** `HttpClient::parseStatusLine` has required exactly three
digits since #2064, so a server can never produce a code this rejects.

**To migrate:** nothing, unless you constructed a sentinel response with a code outside 0..999,
which .NET never allowed either.

## 3. Downstream, measured

Neither `cna` nor `mobile-eggbert` references `HttpClient` or `System::Net::Http` — **zero sites
in both**. Neither repository was modified. The full-rebuild requirement for #2067's layout
change is recorded here for any future consumer.
