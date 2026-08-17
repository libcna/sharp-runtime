<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `Guid::NewGuid` and the platform CSPRNG — SR-AUD-050, ticket #2228

Owning ticket **#2228**, the last `high` finding in `modules/core`. This document is the durable
design record; the repair landed on **2026-08-17** under `docs/StandingApprovals.md` SA-5.

---

## 1. The defect, and why no ordinary test could see it

Until #2228, every byte of a `Guid` came from a `std::mt19937_64` seeded once per thread. Mersenne
Twister is not a cryptographic generator: **2,496 bytes of its output recover its entire internal
state**, after which every subsequent GUID is predictable.

.NET documents 122 bits of strong entropy from the platform CSPRNG, and `Guid.Unix.cs:18` explains
in its own comment why it does not economise there:

> Guid.NewGuid is often used as a cheap source of random data that are sometimes used for security
> purposes. Windows implementation uses secure RNG to implement it. We use secure RNG for Unix too
> to avoid subtle security vulnerabilities in applications that depend on it.

The output was a structurally valid version-4 UUID either way, and it never repeated. **That is
precisely why the finding is dangerous**: no shape test, no uniqueness test and no
byte-distribution test can distinguish the two implementations. §4 is about the one test that can.

`#1901` had already made the engine per-thread to close the SR-AUD-010 data race, and deliberately
did **not** change what kind of entropy it produced. Nothing here supersedes that ticket; §5 says
what happened to its property.

---

## 2. The blocking question, and why it dissolved

#2228 was `blocked` on a user decision between three options, because a CSPRNG *appeared* to make a
previously non-throwing public function able to throw on Emscripten — and the alternative, a
platform-dependent silent fallback to a weak source, recreates the very defect class the finding
names.

**The premise was false, and measuring it is what closed the ticket.** Emscripten has a real
CSPRNG:

- Emscripten's libc declares `getentropy()` in `<unistd.h>` (line 193 of its bundled `unistd.h`)
  and implements it as `__wasi_random_get()`
  (`system/lib/libc/musl/src/misc/getentropy.c`), which the runtime backs with the host's
  `crypto.getRandomValues`.
- .NET reaches the same source there:
  `minipal_get_cryptographically_secure_random_bytes` calls `SystemJS_RandomBytes` under
  `__EMSCRIPTEN__` (`src/native/minipal/random.c:83-93`).

So all three target platforms have a real CSPRNG, **nothing new throws anywhere**, and there was no
decision left for the user to take. The options A/B/C the ticket described were answering a
question the environment does not pose.

---

## 3. Where the code lives, and why it is not a call into `RandomNumberGenerator`

`Core.Base` **cannot** depend on `Security.Cryptography.Random`: the dependency runs the other way,
and inverting it would drag a cryptography component under every consumer of `Guid`. That
constraint is what the ticket recorded as needing verification, and it holds.

The entropy function is therefore **file-local to `Guid.cpp`**, in an anonymous namespace. That
makes this a `.cpp`-only change:

| Category | Consequence |
|---|---|
| Public headers | **none** — no new header, no new declaration |
| Component graph | **unchanged** — no new edge |
| Object layout, vtable, mangled symbols | **unchanged** |
| Signatures, `noexcept` | **unchanged** |
| Accepted input / output shape | **unchanged** — still a valid v4 / v7 UUID |

It deliberately mirrors `RandomNumberGenerator.cpp`'s three-platform shape rather than inventing a
fourth: `BCryptGenRandom` with `BCRYPT_USE_SYSTEM_PREFERRED_RNG` on Windows, `getentropy()`
elsewhere, chunked at 256 bytes because that is a documented `EIO` rather than a silent truncation.

`getentropy()` rather than `getrandom()`: `getrandom()` is Linux-only — undeclared on Apple and BSD
— while `getentropy()` is present on all three, and on Emscripten it is the one backed by
`__wasi_random_get`.

### 3.1 Failure semantics, stated rather than left implicit

The fill loop **does not throw**. `NewGuid()` is treated as infallible by every caller, a CSPRNG
that has already been reached does not intermittently fail, and on these platforms a failure means
the process has no entropy source at all. The loop retries on `EINTR`, and for anything else
retries the same call after a `yield()`. It never falls back to a weaker source — that fallback is
the defect this ticket exists to remove, and it would be indistinguishable from the original bug.

---

## 4. The one test that discriminates, and the ones that cannot

The finding's claim is *"recovering the generator state predicts subsequent UUIDs"*. Observed from
outside a process, that reduces to a property with a very sharp test: **a userspace PRNG has state,
and `fork()` duplicates it.**

```
draw one GUID          (this is what seeded the thread_local engine in the old code)
fork()
parent draws a GUID    child draws a GUID
```

Under the old implementation both sides advance the *same inherited* engine and produce the
**identical** GUID. Under a CSPRNG each call reads the kernel, so they differ. This is not a
contrived probe — PRNG-state duplication across `fork` is a real and repeatedly exploited class of
vulnerability.

`GuidTests.NewGuidDoesNotInheritGeneratorStateAcrossFork` and its version-7 sibling assert exactly
that. The version-7 case asserts on the **random tail** only, because `CreateVersion7` overwrites
the first 48 bits with a millisecond timestamp that parent and child usually share.

**Mutation-checked**: restoring the pre-#2228 per-thread Mersenne Twister fails **both** fork cases
and nothing else — which is the finding's invisibility, demonstrated rather than asserted.

A third case pins that the RFC 4122 version and variant bits are still applied over the new bytes,
which is what would catch a repair that swapped the source and forgot the masks.

**What is deliberately not claimed:** no test here measures entropy *quality*. There is no
black-box test that separates a CSPRNG from a well-seeded PRNG, and writing a statistical test that
passes either way would have been worse than writing none. The quality guarantee rests on the
construction and on the citations in §2 and §3, and this document says so plainly.

---

## 5. #1901's concurrency property, preserved by construction

#1901 made the engine per-thread and argued for distinct seeds because a platform with a
deterministic `std::random_device` would otherwise hand every thread the same stream.

#2228 removes the engine entirely. There is now **no engine, no `thread_local`, no function
`static` and no `std::random_device`** on this path, so:

- the race surface is **empty** — concurrent `NewGuid()` shares nothing to synchronise;
- the distinct-seed argument no longer has a subject.

The 100,000-GUID concurrency tests #1901 added are kept **unchanged**: they still assert the same
observable contract, and any future repair that reintroduced shared generator state would have to
satisfy them again.

**ThreadSanitizer was not re-run for this ticket, and that is a deliberate, stated choice.** TSan's
job is to find races in shared mutable state; this change deletes all of it from the path, which is
verifiable by reading thirty lines. Building a sanitizer tree to confirm that code with no shared
state has no race would have cost hundreds of megabytes to demonstrate a tautology.

---

## 6. Rollback

One `.cpp` hunk. Reverting restores the previous engine exactly; no data format, persisted state,
serialized layout or public declaration is involved.

## 7. Downstream

Measured under SA-2 condition 5: neither `cna` nor `mobile-eggbert` calls `Guid::NewGuid` or
`Guid::CreateVersion7` — **zero sites in both**. Neither repository was modified. No consumer
rebuild is required, since nothing about the public surface changed.
