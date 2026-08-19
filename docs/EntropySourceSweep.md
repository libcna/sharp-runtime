<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# The entropy-source sweep (tickets #2398, #2401, #2402)

*2026-08-19.* One question, asked of the whole runtime:

> **#2228 put a real CSPRNG behind `Guid::NewGuid`. Where else do unpredictable bytes come from?**

There are **exactly five** places in production code that need random bytes. **Two were defects and
are closed. Two are parity and are recorded here so they are not re-opened.** This document exists
because the two parity sites *look exactly like the defect*: they call `std::random_device`, which
is what #2401 removed from `ClientWebSocket`. A later reader who "fixes" them would be introducing a
divergence.

---

## The five sites

| # | Site | Source | Verdict |
|---|---|---|---|
| 1 | `Guid::NewGuid` | platform CSPRNG (`getentropy`/`BCryptGenRandom`) | **correct** since #2228 |
| 2 | `RandomNumberGenerator` | platform CSPRNG | **was a defect** — threw on Emscripten; #2398 |
| 3 | `ClientWebSocket` `Sec-WebSocket-Key` nonce | `Guid::NewGuid()` | **was a defect** — `std::random_device`; #2401 |
| 4 | `ClientWebSocket` per-frame masking key | `RandomNumberGenerator::Fill` | **was a defect** — `std::random_device`; #2401 |
| 5 | `HashCode::GlobalSeed` | `std::random_device` | **parity** — see below |
| 6 | `Random()`'s unseeded constructor | `std::random_device` (mixed) | **parity** — see below |

*(Six rows for five sites: #2401 repaired two distinct things in one file, by two different routes,
because .NET uses two different routes.)*

---

## Why 5 and 6 are parity, measured rather than assumed

**.NET has two distinct entropy entry points and chooses between them deliberately.**
`Interop.GetRandomBytes.cs:18-27`:

```csharp
internal static unsafe void GetRandomBytes(byte* buffer, int length)
    => Sys.GetNonCryptographicallySecureRandomBytes(buffer, length);

internal static unsafe void GetCryptographicallySecureRandomBytes(byte* buffer, int length)
{
    if (Sys.GetCryptographicallySecureRandomBytes(buffer, length) != 0)
        throw new CryptographicException();
}
```

and `pal_random.c:13-27` shows the two reach genuinely different `minipal` functions. They are not
two names for one thing.

- **`HashCode`** — `HashCode.cs:58` is `s_seed = GenerateGlobalSeed()`, and `:70-75` is
  `Interop.GetRandomBytes(...)`: **the non-cryptographic one**.
- **Unseeded `Random`** — `Random.Xoshiro256StarStarImpl.cs:38` and
  `Random.Xoshiro128StarStarImpl.cs:38` are also `Interop.GetRandomBytes`.

So .NET deliberately does **not** use a CSPRNG at either site, and neither should this port. There
is also a structural reason not to: `HashCode` lives in `Core.Base`, so routing it through a
cryptographic generator would put a cryptography component under **every** consumer of `Core.Base` —
the inversion `Guid.cpp` already refused for its own reasons, and the argument that made #2401's
edge acceptable (`Net.WebSockets` is a leaf, the edge is private) does not transfer.

**What is required at 5 and 6 is that the value differ per process, not that it be unpredictable.**
That property is now asserted — see below.

---

## The defect that came out of looking

`HashCodeTests.Seed_DiffersAcrossProcessesButConsistentWithinOne` asserted **only the second half of
its own name**. Its body compared two accumulators *inside one process*, which a constant seed
satisfies perfectly. **The first clause was untested** — and it is the clause the property exists
for, stated in `HashCode.hpp`'s own doc-comment (*"consistent within a run but differ across runs
(by design, to discourage persisting hash codes and to resist hash-flooding)"*) and in .NET's
`HashCode.cs:58`.

**This is the third assertion in this sweep that could not fail for the property it claimed**, after
the two `EXPECT_EQ(buffer.size(), N)` cases #2398 and #2399 replaced. That pattern is now recorded
in `NEXT.md` §4b as a search worth running on its own.

**A plain `fork()` cannot test it**, and finding that out is the useful part: the seed is a
function-local `static` initialised on first use, so a forked child **inherits** it and reports the
same hash whatever the source. The child must **re-exec** — the idiom #1979 established for
`PosixSignalTests`, for the same class of reason. The case is now split in two:
`Seed_IsSharedByEveryInstanceWithinOneProcess` (the old body, renamed to what it actually checks) and
`Seed_DiffersAcrossProcesses` (two re-exec'd children, reporting through a pipe on fd 3 so gtest's
own output cannot be mistaken for the payload).

The child driver uses `SUCCEED()` rather than `GTEST_SKIP()` when it is not the child. That is
deliberate: it runs in **every** ordinary run, and a skip would move this repository's gate off
**"0 skipped"** permanently — a documented property of the floor in `CLAUDE.md` rule 2, not an
incidental one.

Three mutations, all caught. A constant seed and a seed fixed at build time are each caught **only**
by the new case, which is the evidence the repair was needed; a per-*instance* seed — the opposite
mistake — is caught by 22 pre-existing cases. One mutation was **invalid as first written and was
reformulated rather than counted**: deriving the seed from a stack address is not the defect it was
meant to be, because ASLR varies it per process.
