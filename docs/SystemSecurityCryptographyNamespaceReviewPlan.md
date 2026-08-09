<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `System::Security::Cryptography` namespace review and remediation plan

*Ticket #2158. Opened 2026-08-09 on branch `claude/remediation-batch-1804-namespace-b1yjh5`.
Audit numbering is **frozen at 364** — this review creates no `SR-AUD-*` identifier. Every number
below was measured in this repository on this branch; nothing is inherited from the previous
handoff without being re-measured, and where the two disagree the measurement wins and the
disagreement is stated rather than quietly corrected.*

---

## 0. The one-line result

`modules/security-cryptography` has two open findings, both `high`, both about key material that
outlives the operation that needed it. **Both are compatible.** The previous handoff's claim that
SR-AUD-331 requires an object-layout change is **wrong, and measurably so**: the repair fits in an
existing four-byte padding hole and overrides an already-virtual method, so `sizeof`, `alignof`,
every pre-existing member offset and the vtable shape are all unchanged. This namespace can
therefore be closed in one pass, with no approval gate.

---

## 1. Why `modules/security-cryptography` — the selection, re-derived

`audit/AUDIT_FINDINGS_INDEX.md` was re-parsed from scratch (364 rows: 156 `remediated`, 158
`confirmed`, 50 `confirmed (design-complete)`). Open findings per **unreviewed** module:

| Module | Open | high | high % | `/rv`-bound | Verdict |
|---|---|---|---|---|---|
| `core` | 72 | 9 | 12 % | mixed | not a namespace; already carved by seven `CCF-*` plans |
| `globalization` | 7 | 1 | 14 % | **heavily** | needs `/rv` *and* ICU data — both absent |
| `time-zone` | 7 | 0 | **0 %** | **mostly** | zero high; needs a real tz database plus .NET parity |
| `numerics` | 4 | 0 | 0 % | partly | zero high |
| `xml-linq` | 4 | 1 | 25 % | no | its **only** high is CCF-019 — **blocked** (#1899) |
| `net-network-information` | 3 | 0 | 0 % | no | zero high; its executable cannot pass here (#1962) |
| **`security-cryptography`** | **2** | **2** | **100 %** | **none** | **winner** |
| `console` | 2 | 0 | 0 % | no | zero high — the planned second unit |

### 1.1 Why a two-finding unit is defensible below the programme's earlier ≥ 6-finding threshold

Namespace size is a proxy for expected value, and here the proxy is wrong in four measurable ways.

1. **Severity is uniform, not averaged.** This is the only unreviewed unit whose open findings are
   **100 % high**. Every larger candidate's high is either blocked (`xml-linq` → CCF-019),
   approval-gated (`globalization`), or absent entirely (`time-zone`, `numerics`,
   `net-network-information`).
2. **The consequence class is disclosure of secrets, not a wrong answer.** SR-AUD-332's probe
   recovers **32 of 32 key bytes** from a *disposed* HMAC by XOR-ing one retained pad with a
   constant. SR-AUD-331 leaves a PBKDF2 password resident and lets the derivation keep running.
   Neither is a parity nicety.
3. **Attacker influence and lifetime are both real.** The retained material is a caller's long-term
   key or password, and it survives for the lifetime of the *process*, not the operation: the
   measurement in §4.3 shows key-derived bytes reaching **freed heap storage**, where any later
   allocation in the same process can read them without any memory-safety bug at all.
4. **Zero `/rv` dependence.** Neither finding turns on a .NET behaviour that needs the reference
   tree — both are "storage the object owns still holds a secret", answerable entirely by direct
   measurement here. The one place .NET's own behaviour matters (what `Rfc2898DeriveBytes.Dispose`
   does) is quoted in the audit record itself, so the target is measured rather than recalled.

Bounded size is a *reason to select it*, not an argument against: 39 headers, 7 bodies, 2,401
production lines, one test executable at **80 tests**, and the whole of the module's encryption
surface already excluded by `CLAUDE.md`'s explicit 2026-07-07 out-of-scope decision — so the
review's surface is genuinely the hash/HMAC/KDF core and nothing else.

---

## 2. Scope and file inventory

**In scope:** `modules/security-cryptography` — 39 public headers, 2 internal `detail/` headers,
7 `.cpp` bodies, 4 test files (`SharpRuntimeTests_Security_Cryptography`, **80 tests** at the start
of this review). Component `Security.Cryptography`, single public dependency `Core.Base`.

**Adjacent but NOT in scope:** `modules/security-cryptography-random`
(`RandomNumberGenerator`, SR-AUD-012 — a separate component with its own open finding) and
`modules/security` (`SecurityException` and principals, SR-AUD unrelated).

### 2.1 Public surface inventory

| Group | Types | Owns key material? |
|---|---|---|
| Hash bases | `HashAlgorithm`, `KeyedHashAlgorithm` | `KeyedHashAlgorithm::keyValue_` — **yes** |
| Fixed-output digests | `MD5`, `SHA1`, `SHA256`, `SHA384`, `SHA512`, `SHA3_256/384/512` + the five `*Managed`/`CryptoServiceProvider` aliases | no (message state only) |
| Extendable output | `Shake128`, `Shake256` | no |
| Keyed hash | `HMAC` + `HMACMD5/SHA1/SHA256/SHA384/SHA512/SHA3_256/SHA3_384/SHA3_512` | **yes** — `keyValue_`, `innerPad_`, `outerPad_` |
| Key derivation | `DeriveBytes`, `Rfc2898DeriveBytes`, `HKDF` | **yes** — `password_`, `salt_`, `buffer_`; `HKDF` is static-only and holds nothing |
| Metadata / value types | `HashAlgorithmName`, `KeySizes`, `Oid`, `OidCollection` | no |
| Exceptions | `CryptographicUnexpectedOperationException`, `AuthenticationTagMismatchException` | no |
| Internal | `SharpRuntimeDetail::Keccak`, `SharpRuntimeDetail::Sha512` | no |

**Deliberately absent, and correctly so:** symmetric/asymmetric ciphers, `ICryptoTransform`,
`CryptoStream`, X.509 and TLS — all excluded by `CLAUDE.md`'s permanent-deviation list. No
byte-array *encryption* key import/export surface exists, so the prompt's "symmetric algorithms",
"encryption/decryption" and "transforms" inventory rows are **empty by design**, not overlooked.
The only key-import doors in the whole component are `HMAC`'s constructor and
`KeyedHashAlgorithm::setKeyProperty`, plus `Rfc2898DeriveBytes`' two constructors.

### 2.2 Ownership, storage class and lifetime

Every secret in this component lives in a `std::vector<bytecs>` member — heap storage, owned by the
object, released by the implicit destructor. There is no `std::array` secret, no string-typed
secret, no stack-resident long-lived secret, and no caller-visible span or pointer into any of it:
`getKeyProperty()` returns a **copy**, `getSaltProperty()` returns a **copy**. That matters for the
repair, because it means every retained byte is in storage this component can reach and erase —
there is no borrowed-view lifetime problem here of the kind CCF-019 covers elsewhere.

---

## 3. Confirmed finding inventory — both, with measured current behaviour

Measured by `build-probe/2158_probe2_residue.cpp`, compiled `-O2 -fno-access-control` against
`libsharp_runtime_security_cryptography.a`
(log: `build-probe/2158_probe2_residue_before.log`).

| ID | Sev | Type | Audit claim | Measured here |
|---|---|---|---|---|
| SR-AUD-332 | high | `HMAC` | Dispose clears `keyValue_` but leaves both pads resident and invertible | **Confirmed exactly.** After `Dispose()`: `keyValue_.size() = 0`, `innerPad_.size() = 64`, `outerPad_.size() = 64`, longest run of `key ^ 0x36` = **32**, of `key ^ 0x5C` = **32**, and **32 of 32** key bytes recovered by one XOR |
| SR-AUD-331 | high | `Rfc2898DeriveBytes` | Inherits an empty `Dispose`, retains password/salt/buffer, and keeps deriving | **Confirmed exactly.** After `Dispose()`: `password_` = 16 bytes, all still the password; `salt_` = 12; `buffer_` = 32; `GetBytes(4)` returns 4 bytes, and those bytes are **byte-identical to a fresh, never-disposed instance's** continuation — so the post-disposal path is the real PBKDF2 derivation, not residue |

---

## 4. Corrections and extensions to the audit record — measured, not inferred

### 4.1 SR-AUD-331 does **not** need an object-layout change — the inherited premise is wrong

`NEXT.md`'s handoff states: *"SR-AUD-331 needs a disposed-state flag on `Rfc2898DeriveBytes`, i.e.
an object-layout change, so plan for one compatible ticket plus one blocked design."* That was a
prediction, not a measurement. `build-probe/2158_probe1_layout.cpp`
(log: `build-probe/2158_probe1_layout.log`) measures it:

```
Rfc2898DeriveBytes                 sizeof=160 alignof= 8

Current (control restatement)      sizeof=160 alignof= 8
A: trailing bool                   sizeof=168 alignof= 8      <- the naive repair, +8
B: bool in blockSize_ padding      sizeof=160 alignof= 8      <- SELECTED
C: iterations_==0 sentinel         sizeof=160 alignof= 8

Current            blockSize_     104        PaddingHoleFlag    blockSize_     104
                                             PaddingHoleFlag    disposed_      108
Current            buffer_        112        PaddingHoleFlag    buffer_        112
Current            block_         136        PaddingHoleFlag    block_         136
Current            startIndex_    144        PaddingHoleFlag    startIndex_    144
Current            endIndex_      152        PaddingHoleFlag    endIndex_      152
```

`blockSize_` is an `intcs` at offset 104 followed by **four bytes of alignment padding** that no
member uses, because `buffer_` needs 8-byte alignment. A `bool` declared immediately after
`blockSize_` lands at offset 108, inside that hole. Every pre-existing member keeps its offset
(8 / 32 / 56 / 64 / 104 / 112 / 136 / 144 / 152), `sizeof` stays **160**, `alignof` stays **8**.

The vtable is equally unaffected: `DeriveBytes` already declares `virtual void Dispose()`, so
`Rfc2898DeriveBytes::Dispose()` **fills an existing slot** rather than adding one. `DeriveBytes` is
polymorphic and abstract both before and after; the derived class gains one new *definition*
(`_ZN6System8Security12Cryptography18Rfc2898DeriveBytes7DisposeEv`), which is purely additive.

Two consequences of the hole being **interior** rather than trailing, both checked: `sizeof` is
160 = 8 × 20 with `endIndex_` ending exactly at 160, so the type has **no tail padding** for a
derived class to reuse, and an interior hole is never reusable by a derived class in the first
place. Derived-class layout is therefore also unchanged. Probe
`build-probe/2158_probe4_derived.cpp` measures this directly rather than arguing it.

**This finding is compatible and is implemented in this batch (#2160). Nothing about it is
approval-gated.**

### 4.2 The retention is wider than either finding records — five further sites, all measured

The audit names two sites (`HMAC`'s pads, `Rfc2898DeriveBytes`' three buffers). Direct measurement
of **freed** storage — a replacement global `operator delete` that snapshots each block at the
moment it is released — finds five more, none of which either finding mentions:

| # | Site | Measured residue | Reachable without calling anything wrong? |
|---|---|---|---|
| 1 | `HMAC::derivePads()`'s `keyPrime` local | 32-byte run of the raw key in freed storage, **even after `Dispose()`** | yes — every construction |
| 2 | `HMAC::HashFinal()`'s `innerInput`/`outerInput` | 32-byte runs of both pads in freed storage | yes — **every `ComputeHash` call** |
| 3 | `setKeyProperty` replacing a key | 32-byte run of the **old** key in freed storage | yes |
| 4 | Destruction without `Dispose()` | key **and** both pads in freed storage | yes — the *normal* C++ RAII path |
| 5 | Failed construction (`iterations = 0`) | 16-byte run of the password in freed storage | yes — a rejected argument |

Site 4 is the important one for a C++ port. .NET has no deterministic destructor, so .NET's answer
is necessarily "call `Dispose`"; a C++ caller who writes `HMACSHA256 h(key);` and lets it go out of
scope is writing *idiomatic* code, and today that path erases nothing at all. Site 3 is notable for
the opposite reason: the old **pads** leave no residue (they are overwritten in place by `assign`
at the same size), only the old **key** does — so a test that checked pads alone would have passed.

### 4.3 `std::fill` on a dying buffer is deleted by the optimiser — measured, not assumed

`KeyedHashAlgorithm::Dispose` clears `keyValue_` with `std::fill`. The prompt requires that a
zeroisation repair not rely on that, and the requirement is justified.
`build-probe/2158_probe3_zeroise.cpp`, compiled `-O2` with GCC 13.3 and disassembled
(`build-probe/2158_probe3_zeroise.log`):

- `naiveClearStack()` — a 64-byte stack secret cleared with `std::fill` before scope exit —
  compiles to **the `printf` tail call and nothing else**. Buffer and clear are both gone.
- `naiveClearHeap()` — a 64-byte heap secret cleared with `std::fill` immediately before the block
  is released — goes straight from `printf` to `operator delete`. **No zero-store survives.**
- `secureClearStack()` / `secureClearHeap()` — the same, through
  `SharpRuntimeDetail::SecureMemory` — both keep a real store loop
  (`movb $0x0,(%rdx)` / `movb $0x0,(%rax)`) immediately before scope exit or deallocation.

The primitive is a `volatile unsigned char` write loop. Every access to a `volatile` lvalue is an
observable side effect of the abstract machine, so removing it would be a miscompile rather than an
optimisation — no `#pragma`, no inline asm, no platform `#ifdef`, and nothing that would break the
MinGW or Emscripten builds `CLAUDE.md`'s platform policy requires to keep compiling.

### 4.4 The audit's own "direct key cleared" reading is true but not sufficient

`hmac-direct-key-cleared=1` is reproduced here. It is also, on its own, misleading: the key is
cleared in the *live object* and simultaneously present in **freed** storage via site 1, on the same
`Dispose()` call the reading describes. The repair therefore cannot be "clear one more member"; it
has to be "no owned buffer is released while it still holds key-derived bytes".

---

## 5. Root causes

### SC-A — key-derived state is erased by member, not by policy (SR-AUD-332, and §4.2 sites 1–4)

`KeyedHashAlgorithm::Dispose` erases the one member it can see. `HMAC` adds three more buffers —
two of which are a fixed XOR away from the key — and overrides nothing. Nothing in the component
states, or enforces, that a buffer holding key-derived bytes must be erased before it is released,
so each new buffer silently opts out. This is why the defect is five sites and not one.

### SC-B — a disposal that records nothing cannot be checked (SR-AUD-331)

`DeriveBytes::Dispose()` is an empty virtual, and `Rfc2898DeriveBytes` neither overrides it nor has
any state that could represent "disposed". Every public door therefore keeps working, and the
sibling base in the very same component (`HashAlgorithm`) already shows the idiom this type is
missing: a `disposed_` flag plus `ObjectDisposedException::ThrowIf` at each door.

### SC-C — a rejected argument still consumed the secret (§4.2 site 5)

`Rfc2898DeriveBytes`' constructor moves the password into `password_` in the member-initialiser
list, then validates `iterations` and the hash algorithm in the body. A rejected call therefore
destroys a fully populated `password_` without erasing it.

---

## 6. What the repair can and cannot guarantee — stated before it is built

The prompt requires that no claim be made about OS-wide erasure the implementation cannot back.
The boundary drawn here, and pinned by the tests:

**In scope — this component erases it.** Bytes in storage the object owns: `keyValue_`,
`innerPad_`, `outerPad_`, `pendingMessage_`, `password_`, `salt_`, `buffer_`, and the named
temporaries in `derivePads`, `HashFinal` and `func`. Erased on `Dispose()`, on destruction, on key
replacement, and on the failed-construction path — in each case **before** the storage is released.

**Out of scope — and not claimed.**

- The caller's own copy of the key. `HMAC`'s constructor takes `std::vector<bytecs>` by value; if
  the caller passes an lvalue, the caller's vector is untouched and is the caller's to erase.
- `getKeyProperty()` / `getSaltProperty()` return copies. Once handed out, they are the caller's.
- Reallocation growth inside `pendingMessage_`: `std::vector::insert` may release the old block
  itself, before any component code runs. Erasing that would need a custom allocator, which is a
  different and much larger change. **Measured, recorded in §9 as a deliberate exclusion, pinned.**
- Register and stack spills. A compiler may keep a byte of key material in a register or spill it;
  nothing at C++ level can reach that.
- Anything outside the process image: swap, hibernation, core dumps, `/proc/<pid>/mem` read by a
  privileged observer, or a page the kernel has already recycled.
- Cryptographic strength. Nothing in this batch changes an algorithm, a parameter or an output
  byte, and no functional test here is evidence about strength.

---

## 7. Compatible / blocked / deferred matrix

| Cause | Finding | Ticket | Class | Layout / ABI |
|---|---|---|---|---|
| SC-A | SR-AUD-332 | **#2159** | **compatible** | none — `Dispose` override fills an existing vtable slot; no new member |
| SC-B | SR-AUD-331 | **#2160** | **compatible** | none — `bool` in an existing padding hole; `sizeof` 160 → 160 |
| SC-C | none (post-audit) | **#2160** | **compatible** | none — body reordering only |
| — | contract + pins + reconciliation | **#2161** | **compatible** | none |

**Nothing in this namespace is approval-gated.** No ticket here is blocked, `needs_user`, or
design-only.

---

## 8. Source / ABI / layout / `noexcept` consequences

- `sizeof`/`alignof` of every public type: **unchanged** (§4.1, and re-measured after
  implementation in §14/§15).
- Vtable shape: **unchanged**. `Dispose` is already virtual on both `HashAlgorithm` and
  `DeriveBytes`; the two new overrides fill existing slots.
- Public signatures: **unchanged**. No parameter, return type, default argument or `const`
  qualification changes.
- New symbols: two virtual-override definitions and the header-inline
  `SharpRuntimeDetail::SecureMemory` entry points. Purely additive.
- Special members: `HMAC`, `KeyedHashAlgorithm` and `Rfc2898DeriveBytes` gain a user-declared
  destructor, which suppresses the implicit move operations. All four of copy-construct,
  copy-assign, move-construct and move-assign are therefore **explicitly `= default`ed** so that
  every one of them keeps exactly today's semantics. This is not cosmetic: leaving the moves
  implicitly suppressed would silently turn a move into a copy and leave the *source* object
  holding a live key — a security regression introduced by the security fix. Pinned by test.
- `noexcept`: the primitive is `noexcept`; nothing else changes.
- Behavioural break, deliberate and documented: `Rfc2898DeriveBytes::GetBytes`, `Reset`,
  `setIterationCountProperty`, `setSaltProperty` and `getSaltProperty` now throw
  `ObjectDisposedException` after `Dispose()` instead of working. That is the finding.

---

## 9. Test matrix

Every row becomes a permanent GoogleTest in
`modules/security-cryptography/tests/System/Security/Cryptography/`.

**Key-material lifecycle (#2159, HMAC):** empty key; every supported size (block-size boundary at
20/32/48/64/128/144, and a > block-size key that goes through the hash-compression path); all-zero
key; repeated-byte key; high-entropy key; key with embedded zeros; minimum (1 byte) and a large
key; key replacement; `Dispose`; **double `Dispose`**; `Dispose` then `ComputeHash` (must still
throw); destruction without `Dispose`; move construction; move assignment; copy construction; copy
assignment; a failed operation and a thrown exception mid-use; repeated compute cycles.

**Lifecycle (#2160, `Rfc2898DeriveBytes`):** `GetBytes`/`Reset`/both setters/`getSaltProperty`
after `Dispose` → `ObjectDisposedException` with the exact object name; double `Dispose`;
`Dispose` after partial consumption; failed construction for each of invalid `iterations`, unknown
hash algorithm; split-call continuation still byte-identical to a single call; every supported
`HashAlgorithmName`.

**Cryptographic invariance — the non-negotiable row.** Published vectors for MD5, SHA-1, SHA-256,
SHA-384, SHA-512, SHA3-256/384/512, HMAC-SHA* (RFC 4231) and PBKDF2 must be **byte-identical
before and after**. The repair may not move a single output byte. Asserted by the existing 80 tests
plus explicit before/after digests recorded in the implementation sections.

**Excluded, deliberately:** threading. Nothing in this component documents or implements thread
safety for a shared instance, and this batch does not add any; TSan is therefore not a
discriminating tool here and is not claimed as evidence.

---

## 10. Sanitizer and memory-inspection matrix

| Tool | Used for | Discriminating? |
|---|---|---|
| ASan | bounds and lifetime across the new clear paths | yes |
| UBSan | the pointer arithmetic in the volatile clear loop | yes |
| LSan | the new destructors must not change ownership | yes |
| TSan | — | **no** — excluded above, with the reason |
| Replacement `operator delete` snapshot | freed storage still holding key-derived bytes | **the primary instrument** |
| `-fno-access-control` direct read | live-object retention after `Dispose` | yes |
| `objdump -d` at `-O2` | that the clear is not elided | yes |

**ASan/LSan cleanliness is explicitly not treated as erasure evidence.** A retained key is not a
leak and not a bounds error; the only instruments that can see it are the last three rows.

---

## 11. Execution order

1. **#2158** — this plan.
2. **#2159** — SC-A. The primitive plus every `HMAC`/`KeyedHashAlgorithm` erasure site.
3. **#2160** — SC-B and SC-C. `Rfc2898DeriveBytes` disposal, lifecycle rejection, and the
   failed-construction path.
4. **#2161** — state the contract in the headers, pin what is deliberately excluded, reconcile the
   namespace and the audit index.

#2160 depends on #2159 because `Rfc2898DeriveBytes` derives its blocks through an `HMAC` it
constructs per block; the password's pads are erased by #2159's destructor, not by #2160.

---

## 12. Exclusions

Each of these is a decision, with its reason, and each is pinned by a test or a comment rather than
left to be rediscovered:

1. **Vector reallocation growth** (§6) — needs a custom allocator; out of proportion.
2. **Threading** (§9) — no thread-safety contract exists to repair.
3. **`RandomNumberGenerator` / SR-AUD-012** — a different component
   (`security-cryptography-random`), not this review's scope.
4. **Encryption, `ICryptoTransform`, X.509, TLS** — `CLAUDE.md` permanent deviation, 2026-07-07.
5. **`HKDF`** — static-only; it holds no state to erase. Its per-call `HMAC` objects are covered by
   #2159's destructor, which is measured rather than assumed.
6. **`getKeyProperty`/`getSaltProperty` returning copies** — this is the correct .NET shape; the
   copy's lifetime is the caller's.
7. **The stale `Rfc2898DeriveBytes` doc-comment** that says SHA-3 "hasn't been ported" — corrected
   as documentation by #2161, not treated as a behaviour change; PBKDF2's SHA-3 exclusion matches
   .NET's own `OpenHmac()` restriction and stays.

---

## 13. Completion criteria

- SR-AUD-331 and SR-AUD-332 both `remediated` in `audit/AUDIT_FINDINGS_INDEX.md`, with the audit
  records appended rather than rewritten.
- Every §9 row is a permanent test; namespace suite grows from **80**.
- Live-object retention after `Dispose` is **0 bytes** at every site named in §6, and freed-storage
  residue is **0** at all five §4.2 sites, measured by the same probe that found them, against the
  same deliberately-uncleared control.
- Published vectors byte-identical before and after.
- Zero warnings, zero errors, two jobs maximum, no test-count regression, graph/seams/fixtures
  unchanged.

---

## 14. Implementation record — #2159 (cause SC-A, SR-AUD-332)

**Done 2026-08-09.** `SharpRuntimeTests_Security_Cryptography` **80 → 104** tests, all passing.

### 14.1 The repair

The finding is "clear one more member"; the repair is a **policy**, because §4.2 measured seven sites
and the audit names two.

1. **A primitive the optimiser may not elide** —
   `System/Security/Cryptography/detail/SecureMemory.hpp`, a new internal header:
   `Clear(void*, size_t)` (a `volatile unsigned char` write loop), `ClearContainer` (erase then
   `clear()`, keeping the block so the *erased* bytes are what the allocator eventually gets back)
   and `Scrub` (erase, keep the size).
2. **`KeyedHashAlgorithm`** — `Dispose` now goes through the primitive instead of `std::fill`;
   `setKeyProperty` erases the previous key **before** the move-assignment releases its block; a new
   destructor erases the key.
3. **`HMAC`** — a `Dispose()` override and a destructor, both erasing `innerPad_`, `outerPad_` and
   `pendingMessage_` through one shared `clearDerivedState()`; `setKeyProperty` scrubs both pads
   before re-deriving; `Initialize()` erases the buffered message rather than merely dropping it.
4. **`HMAC::derivePads`** — restructured so the block is **reserved once** up front. That is what
   makes the erasure sufficient rather than decorative: previously the copy and the `resize` could
   each reallocate, handing the allocator a block still holding the key *before* any code could
   reach it. `keyPrime` and the hashed-key temporary are both erased.
5. **`HMAC::HashFinal`** — the same treatment for `innerInput` and `outerInput`, which both begin
   with a key-derived pad, plus `innerDigest`.
6. **The five `.cpp`-backed digests and the Keccak sponge** — a measured extension, not part of the
   finding's text. `Initialize()` now erases the block buffer, and `Keccak::Sponge::Reset()` uses
   the primitive instead of `fill(0)`. This closes §14.2's long-key row: HMAC hashes an
   over-long key with a real digest object, and that object's `std::array` block buffer kept the
   key's tail.

### 14.2 Measured before / after — `build-probe/2158_probe2_residue.cpp`

Same probe, same control, same build flags (`-O2 -fno-access-control`); logs
`2158_probe2_residue_before.log` and `2158_probe2_residue_after_2159.log`.

| Observation | Before | After |
|---|---|---|
| **Control** — a deliberately uncleared 96-byte block, freed in the window | **96** | **96** |
| live `innerPad_` size / longest `key ^ 0x36` run after `Dispose` | 64 / **32** | 0 / **0** |
| live `outerPad_` size / longest `key ^ 0x5C` run after `Dispose` | 64 / **32** | 0 / **0** |
| **key bytes recovered from a pad after `Dispose`** | **32 of 32** | **0** |
| live `pendingMessage_` after `Dispose` | 40 | **0** |
| freed storage after destruction *without* `Dispose` — key / ipad / opad | 32 / 32 / 32 | 0 / 1 / 0 |
| freed storage after `Dispose` then destruction | 32 / 32 / 32 | 0 / 1 / 0 |
| freed storage after key replacement — old key | **32** | **0** |
| freed storage after one `ComputeHash` — ipad / opad | 32 / 32 | 1 / 0 |
| freed storage after a **131-byte** key (hashed first) — key | **36** | **0** |

The residual **1**s are a single coincidental byte, not a retained pad: a pad is 64 contiguous
bytes and the constant `0x91` occurs by chance in a digest. The control is what makes that reading
safe — 96 in both columns means the instrument still sees material when material is there.

### 14.3 Zeroisation evidence — `build-probe/2158_probe3_zeroise.log`

GCC 13.3, `-O2`, disassembled. `naiveClearStack()` compiles to its `printf` tail call and nothing
else — buffer and `std::fill` both deleted. `naiveClearHeap()` goes from `printf` straight to
`operator delete` with no zero-store. Both `SecureMemory` variants keep a real store loop
(`movb $0x0,(%rdx)` / `movb $0x0,(%rax)`) immediately before scope exit or deallocation. This is
why the repair does not use `std::fill`, and it is measured rather than assumed.

### 14.4 Permanent tests — +24, and what they can and cannot pin

`modules/security-cryptography/tests/System/Security/Cryptography/KeyMaterialErasureTests.cpp`.
Live-object assertions go through a **new test-only access seam**,
`SharpRuntime::Testing::KeyMaterialAccess<T>` — declared by `detail/SecureMemory.hpp`, befriended
by `KeyedHashAlgorithm` and `HMAC`, defined in exactly one file
(`modules/security-cryptography/tests/support/KeyMaterialSeam.hpp`), per `CLAUDE.md`'s seam rules.
Seams **2 → 3**, seam definitions **18 → 19** as `scripts/check_version_seam_odr.py` counts them
(the eight HMAC specialisations share one macro body, which is the point of the macro). A seam is
needed because erasure is not observable
through any public API *by construction*, and because the freed-storage recorder cannot separate
"`Dispose()` erased it" from "`~HMAC()` erased it" — they leave the same zeroed block at the same
moment, and the first is what SR-AUD-332 is about.

`test/consumer/security_cryptography_key_material_negative.cpp` is the consumer-side half:
**9 sites**, every one rejected, baseline clean. Negative fixtures **11 → 12 files**,
**94 → 103 sites**.

Coverage: the control (pads present and invertible **before** `Dispose`); `Dispose`; double and
triple `Dispose`; post-`Dispose` `ComputeHash` still throwing; key replacement, including replacing
a block-length key with a 4-byte one so the zero-padded tail is checked; `Initialize`; **all eight**
HMAC variants; **ten** key shapes (empty, 1, all-zero, block−1, block, block+1, 131, embedded
zeros, 4 KiB, and a 200-byte key against a 128-byte block); all four copy/move operations; three
rejected-argument paths followed by a still-correct digest; and the primitive itself with a
deliberately uncleared control alongside it.

**Honest limitation.** These tests do **not** assert the state of storage already returned to the
allocator. Doing that needs a replacement global `operator new`/`operator delete`, and installing
one in this executable would displace AddressSanitizer's own replacement and silently weaken every
other suite in the binary. That half is probe evidence (§14.2), and the consequence is that the
**destructor's** erasure is pinned by the probe rather than by the permanent suite. Stated here
rather than left for a future reader to discover.

### 14.5 Mutations

| Mutation | Result |
|---|---|
| Delete `HMAC::Dispose()`, leaving only the destructor | **6 clean failures** / 98 pass, exactly the disposal tests. **Counts** — and it is the mutation the seam exists for; the freed-storage probe cannot see it |
| Drop the explicitly defaulted move operations, so the new destructor suppresses them | **2 clean failures**, exactly `MoveConstructionTransfersTheKeyAndLeavesTheSourceEmpty` and `MoveAssignmentTransfersTheKeyAndLeavesTheSourceEmpty`. **Counts** — this is the security regression the fix could have introduced, and it is pinned |
| Revert `derivePads`/`HashFinal` to the unreserved shape | **honest non-result** for the permanent suite: it is invisible to the live-object seam and is caught only by the freed-storage probe (§14.2, rows 6–10). Recorded rather than claimed |
| Replace `SecureMemory::Clear` with `std::fill` | **honest non-result**: the live-object tests read the buffer afterwards, so the store is not dead and survives. The discriminating instrument for this one is the disassembly in §14.3, not a test |

### 14.6 Sanitizers

`build-probe/2159_probe_san.cpp`, production sources compiled from source under
`-fsanitize=address,undefined` with `detect_leaks=1`: 8 HMAC variants × 8 key lengths (0, 1, 20,
63, 64, 65, 131, 4096), each exercising construct → compute → key replacement → compute → copy →
move → copy-assign → move-assign → double `Dispose` → `Initialize` → `Dispose`, plus a 200-byte
PBKDF2-SHA512 derivation. **0 reports, exit 0.** TSan was not run and is not claimed: this
component documents no thread-safety contract for a shared instance, so TSan is not a
discriminating tool here (§9).

### 14.7 Not changed

`getKeyProperty()` still returns a copy — that is the .NET shape, and the copy's lifetime is the
caller's. No algorithm, parameter or output byte moved: all 80 pre-existing tests pass unchanged,
and RFC 4231 case 6 (the 131-byte key that exercises the restructured `derivePads`) plus repeated
double-digest cycles for MD5/SHA-256/SHA3-256 are pinned explicitly, because the digest
`Initialize()` change would corrupt an instance's *second* digest rather than its first.

## 15. Implementation record — #2160 (causes SC-B and SC-C, SR-AUD-331)

**Done 2026-08-09.** `SharpRuntimeTests_Security_Cryptography` **104 → 125** tests, all passing
(**80 → 125** across the two implementation tickets).

### 15.1 The layout claim, re-measured on the real type after the repair

§4.1 measured the *prediction* on restatement structs. `build-probe/2160_probe_layout_real.cpp`
measures the **real** `Rfc2898DeriveBytes` with `-fno-access-control` and compares every offset
against the pre-repair figures (`build-probe/2160_layout_real.log`):

```
sizeof=160 alignof=8
password_          8      blockSize_       104
salt_             32      disposed_        108   <- the previously unused padding
iterations_       56      buffer_          112
hashAlgorithm_    64      block_           136
                          startIndex_      144
pre-existing members moved = 0                endIndex_        152
sizeof unchanged (160)     = 1
```

**Zero pre-existing members moved. `sizeof` 160 → 160, `alignof` 8 → 8.** `Dispose` filled an
existing vtable slot. No public signature changed. The inherited handoff's "object-layout change,
so plan for one blocked design" is therefore **withdrawn on measurement**, and this is stated
rather than quietly dropped.

The claim is pinned in the build, not only in this document: `Rfc2898LayoutTests.TheDisposedFlagCostNothing`
asserts `sizeof == 160` and `alignof == 8`, so a future change that appends a member instead of
using the hole fails a test rather than passing unnoticed.

### 15.2 The repair

**SC-B — disposal.** A private `bool disposed_` in the padding hole; a `Dispose()` override that
erases `password_`, `salt_` and `buffer_` through the primitive, resets the derivation indices and
sets the flag; a `throwIfDisposed()` at `GetBytes`, `Reset`, `setIterationCountProperty`,
`setSaltProperty` **and `getSaltProperty`**. The last of those is not a lifecycle nicety: the getter
computes `salt_.end() - 4`, which on the emptied vector `Dispose` leaves behind is **undefined
behaviour**, not an empty result — so erasing without guarding would have replaced a disclosure
defect with a memory-safety one. A destructor covers the ordinary C++ path, with all four
copy/move operations explicitly defaulted for the reason §8 gives.

**SC-C — the rejected argument.** Validation moved out of the constructor body into
`acceptPassword()`, called from the member-initialiser list, which erases the password before
rethrowing. The rejection itself is unchanged — same exception types, same messages — and that is
pinned by test for invalid `iterations`, for MD5, for SHA3-256, for a nameless
`HashAlgorithmName`, and for the `std::string` overload.

**Working buffers.** `initialize()` scrubs the previous block before reassigning; `func()` erases
each PBKDF2 intermediate `U_i` as it is consumed and the last one when the loop ends;
`setSaltProperty` erases the old salt before the move-assignment releases it.

### 15.3 Measured before / after — the same probe, control, and flags as §14.2

| Observation | Before | After |
|---|---|---|
| **Control** — deliberately uncleared 96-byte block | **96** | **96** |
| live `password_` after `Dispose` (bytes / run of the password byte) | 16 / **16** | 0 / **0** |
| live `salt_` after `Dispose` | 12 | **0** |
| live `buffer_` after `Dispose` | 32 | **0** |
| `GetBytes(4)` after `Dispose` | **4 bytes, byte-identical to a fresh instance's continuation** | **throws** `ObjectDisposedException`, naming `Rfc2898DeriveBytes` |
| freed storage after destruction — password | **16** | **0** |
| freed storage after destruction — the HMAC ipad derived from the password | 16 | 1 |
| freed storage after a **rejected** `iterations = 0` — password | **16** | **0** |

### 15.4 Permanent tests — +21

`Rfc2898DisposalTests.cpp`: the control (secrets resident *before* `Dispose`); erasure of all three
buffers; **every** door rejecting afterwards; the exact diagnostic naming the type; double and
triple `Dispose`; `Dispose` after partial consumption and before any derivation at all; rejected
`iterations` (0, −1) and rejected algorithms (MD5, SHA3-256, nameless) including through the
`std::string` overload; all four copy/move operations, with a moved-from source proved empty and a
disposed copy proved not to dispose its original; RFC 6070 PBKDF2-HMAC-SHA1 vectors at 1, 2 and
4096 iterations; the SHA-256 vector **the audit's own probe recorded for this exact type**
(`120fb6cf…be17b`); five-way split-call continuation equal to one call; `Reset` and both setters
restarting the sequence; all four supported algorithms; a rejected `GetBytes` argument leaving the
instance usable; and the layout assertion.

The `KeyMaterialAccess` seam gained its `Rfc2898DeriveBytes` specialisation — written out literally
rather than macro-generated, because `scripts/check_version_seam_odr.py` correctly rejects two
macros that both define `KeyMaterialAccess<OwnerType>` with different bodies (that is precisely the
divergence ticket #1800 measured). Seam definitions **19 → 20**. The negative fixture gained a
tenth site for `disposed_`; negative fixtures **12 files / 104 sites**.

### 15.5 Mutations

| Mutation | Result |
|---|---|
| `Dispose()` invalidates but does not erase | **4 clean failures** / 121 pass, exactly the erasure tests. **Counts** |
| Append `disposed_` after `endIndex_` instead of using the hole | `Rfc2898LayoutTests.TheDisposedFlagCostNothing` fails on `sizeof` 168 ≠ 160. **Counts**, and it is the mutation that would silently undo this ticket's central claim |
| Remove `throwIfDisposed()` from `GetBytes` | One clean assertion failure, then the process **aborts** with "double free or corruption". Recorded, but **excluded** by this batch's abort-only rule. It is worth stating why it aborts: with the erasure in place but the guard gone, `func()` writes `salt_[salt_.size() - 4]` on an emptied vector. The original code had no such window — it never erased — so this is a property of the *mutant*, and it is the reason the guard and the erasure had to land together |
| Revert `acceptPassword` to body validation | **honest non-result** for the permanent suite: the rejection behaviour is identical and only the freed-storage probe sees the difference (§15.3, last row) |

### 15.6 Sanitizers

`build-probe/2160_probe_san.cpp` under `-fsanitize=address,undefined` with `detect_leaks=1`:
4 algorithms × 4 iteration counts, each doing five split `GetBytes` calls, `Reset`, both setters,
copy, move, copy-assign, move-assign, double `Dispose`, then five post-disposal doors, plus four
rejected constructions — 4,544 bytes derived, 84 rejections, **0 reports, exit 0**. TSan not run
and not claimed (§9).

### 15.7 A documentation correction that came with it

The type's doc-comment said SHA-3 was excluded because "this runtime hasn't ported SHA-3". It has —
`SHA3_256`/`SHA3_384`/`SHA3_512` and all three `HMACSHA3_*` live in this component, and `HKDF`
accepts them. The comment was true when written and stopped being true without noticing. The
**exclusion itself is correct and unchanged**: .NET's own `OpenHmac()` does not accept SHA-3 either,
and that is now the reason the comment gives. Pinned by
`Rfc2898ConstructionTests.AnUnknownHashAlgorithmIsRejectedBeforeThePasswordIsStored`.

## 16. Implementation record — #2161, and the namespace reconciliation

**Done 2026-08-09.** `SharpRuntimeTests_Security_Cryptography` **125 → 134** tests, all passing.
**80 → 134 (+54)** across the whole review.

### 16.1 The contract is now stated where a caller will see it

§6's boundary was prose in this document. It is now a `@note` in every public header that owns a
piece of it: `KeyedHashAlgorithm` (the key), `HMAC` (the pads and the working buffers),
`Rfc2898DeriveBytes` (the password, salt and derived-byte buffer, and the rejected-argument path),
`HashAlgorithm` (an unkeyed digest holds no key, but its block buffer becomes key material in the
one place `HMAC` hashes an over-long key), and `HKDF` (static-only, holds nothing, and what it
*returns* is the caller's).

### 16.2 The exclusions are pinned, not merely listed

`KeyMaterialContractTests.cpp`, +9 tests, all of them about the boundary rather than the repair:

- `getKeyProperty()` and `getSaltProperty()` hand out **copies** that disposal does not reach, and
  a caller's own key vector is untouched. These pin the *absence* of a guarantee: a future change
  that tried to erase a caller's storage would be reaching into memory it does not own, and these
  tests would fail.
- `HKDF` is not default-constructible and holds no state between calls; RFC 5869 test case 1 is
  byte-identical after the HMAC erasure work.
- A disposed unkeyed digest rejects further use; the block-buffer erasure does not disturb a
  **reused** digest (a misplaced erasure would corrupt an instance's second digest, not its first,
  so a single-shot vector would not catch it); `Keccak::Sponge::Reset` still leaves a fresh sponge
  for both SHA3 and SHAKE.
- Two deliberate `SUCCEED()` tests record the two things this component does **not** claim —
  erasure of storage the allocator has already reused, and any thread-safety contract for a shared
  instance. They assert nothing, and that is their point: a future change that started claiming
  either would have to delete a test to do it.

### 16.3 The stale doc-comment

Corrected in §15.7 rather than here, because it travelled with the type it describes.

### 16.4 Namespace reconciliation — `System::Security::Cryptography`

| Finding | Sev | Ticket | Disposition |
|---|---|---|---|
| **SR-AUD-331** | high | **#2160** | **remediated** |
| **SR-AUD-332** | high | **#2159** | **remediated** |

**Both open findings are closed. `modules/security-cryptography` has no blocked, deferred,
`needs_user` or design-only remainder — the namespace is fully reconciled.** That is a stronger
outcome than this batch's brief anticipated, and it rests on one measurement: §4.1/§15.1 showed
SR-AUD-331's repair to be layout-neutral, so the "one compatible ticket plus one blocked design"
the handoff predicted became two compatible tickets.

Three post-audit defects were found and fixed **without** creating an `SR-AUD-*` identifier
(numbering stays frozen at **364**): the rejected-constructor-argument password retention (cause
SC-C, #2160), the five further HMAC retention sites and the digest block-buffer retention (both
folded into cause SC-A, #2159, because they are the same defect the finding names, measured wider).

Deliberately excluded, each with a named pin or a stated reason: vector reallocation growth
(§6, §12.1); caller-held copies (§12.6, pinned by test); threading (§9, §12.2, pinned by test);
`RandomNumberGenerator`/SR-AUD-012, which belongs to the separate `security-cryptography-random`
component (§12.3); the encryption surface `CLAUDE.md` excluded on 2026-07-07 (§12.4); a digest
destroyed **mid-message**, which still holds the message tail (§12, stated in `HashAlgorithm`'s
header).

**Totals for this namespace:** 2 findings, 2 remediated, 0 open. Tests **80 → 134**. Seams
**2 → 3** (definitions 18 → 20). Negative fixtures **11 → 12 files**, **94 → 104 sites**. Module
graph unchanged at **41 / 92** — the new `detail/SecureMemory.hpp` is internal to an existing
component and declares no new edge. No `sizeof`, `alignof`, member offset, public signature or
vtable-shape change anywhere in the namespace.
