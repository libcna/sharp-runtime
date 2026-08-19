<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — the WebSocket nonce and masking key come from a CSPRNG (ticket #2401)

*2026-08-19.* `ClientWebSocket` drew its `Sec-WebSocket-Key` nonce **and** its per-frame masking key
from `std::random_device`. Both now come from the platform CSPRNG, by .NET's own two routes.

Landed under **SA-5**. **No public signature, layout, vtable or `noexcept` change** — both helpers
are file-local to `ClientWebSocket.cpp`. **Nothing a caller can observe changed**; what changed is
where the bytes come from, plus one declared component edge.

---

## 1. Why `std::random_device` was wrong here

`std::random_device` is **not required to be a CSPRNG**. The standard explicitly permits a
deterministic implementation, and **this repository has already measured one**: `Random.cpp:69-70`
records, in its own comment, *"on a platform whose random_device is deterministic (MinGW-w64's
historically was)"* — and MinGW-w64 is a supported compile target under `CLAUDE.md`'s platform
policy. On such a platform every connection would have sent the **same** `Sec-WebSocket-Key`, and
every frame's masking key would have been **predictable**.

RFC 6455 does not leave this to taste. §5.3:

> The masking key needs to be unpredictable; thus, the masking key MUST be derived from a strong
> source of entropy, and the masking key for a given frame MUST NOT make it simple for a
> server/proxy to predict the masking key for a subsequent frame.

Masking exists to stop cache-poisoning of intermediaries (§10.3), so a predictable key defeats the
one attack it was introduced for. §4.1 requires the nonce to be *"selected randomly for each
connection"*.

## 2. The two routes are .NET's, and they are different

| | Source | Reference |
|---|---|---|
| `Sec-WebSocket-Key` nonce | `Guid::NewGuid().ToByteArray()` | `WebSocketHandle.Managed.cs:490-494` — `Guid.NewGuid().TryWriteBytes(bytes)`, base64-encoded |
| per-frame masking key | `RandomNumberGenerator::Fill` | `ManagedWebSocket.cs:762-763` — `WriteRandomMask` is `RandomNumberGenerator.Fill(...)` |

They are transcribed **separately rather than harmonised into one**, because that is how .NET has
them. Since **#2228** this port's `Guid::NewGuid()` draws from the platform CSPRNG, so the nonce
route costs **no component edge at all** — `Core.Base` was already a public dependency.

A v4 GUID fixes 6 of its 128 bits (version and variant), so the nonce carries **122 bits of entropy
rather than 128**. That is .NET's own arithmetic here, not a shortcut this port took.

## 3. The one build-facing change: a new component edge

`Net.WebSockets` gains **`Security.Cryptography.Random` as a `PRIVATE` dependency**. The module
graph goes **41 / 93 → 41 / 94** and `docs/ComponentCatalog.md` is regenerated.

It is `PRIVATE` because the only caller is a file-local helper in `ClientWebSocket.cpp`: no public
header names the type, and no consumer inherits the include path. A consumer doing a **selective
component build** of `Net.WebSockets` will now also configure and link
`Security.Cryptography.Random`; verified by running
`scripts/check_selective_components.sh Net.WebSockets net_websockets.cpp`, which passes with its
107 tests.

**The alternative was rejected on a recorded precedent.** Calling `getentropy()` directly from
`net-websockets` would avoid the edge and would be a **third** copy of the platform entropy call —
the duplication #2354 spent a whole ticket removing.

## 4. What can and cannot be verified here

**Stated rather than implied.** On glibc, `std::random_device` reads `/dev/urandom`, so **the source
change is not behaviourally observable on this platform**. Its evidence is the reference, RFC 6455,
this repository's own MinGW-w64 measurement, and **symbol inspection**, which is precisely
discriminating:

| Build | `random_device` | `Guid::NewGuid` | `RandomNumberGenerator::Fill` |
|---|---|---|---|
| after the repair | **0** | 1 | 1 |
| nonce reverted | 16 | **0** | 1 |
| mask reverted | 16 | 1 | **0** |

**What is newly pinned are the RFC properties themselves**, which nothing pinned before: a 16-byte
nonce that differs between connections, and a **fresh** masking key per frame. Both are reachable
from the existing mock-server harness, which already reads the key out of the handshake and the
4-byte mask out of each frame.

Five mutations. The two source reversions are **not caught by tests and are caught by the symbol
table** — reported as such rather than as passes. The three RFC-property mutations — caching one
mask per connection, a constant mask, and caching the nonce for the process lifetime — are **all
caught**, and each is the plausible optimisation rather than a contrived defect: a cached mask still
unmasks correctly at the server, because both ends agree on whatever key was sent. One mutation was
**invalid as first written and was reformulated rather than counted**: zeroing the mask left
`randomMaskingKey()` unreferenced, so `-Werror=unused-function` rejected it at compile time.
