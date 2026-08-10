# Sharp Runtime plan

*Last verified: 2026-08-10 — branch `claude/remediation-batch-1804-namespace-b1yjh5`, the
harness-designated branch. **Pushed after every commit**, per `CLAUDE.md` rule 13 — five commits,
five pushes, all normal fast-forwards. No merge, rebase, tag, force-push, PR, publication, amend or
history rewrite; all commits unsigned. This batch **reviewed `modules/net-network-information`**
(#2187) and landed **its whole compatible queue**: #2189 (SR-AUD-254 — the exception wrapper caught
by base reference and stored `make_exception_ptr(e)`, so every Ping failure arrived as a bare
`std::exception` with its type, message **and native error code** destroyed), #2188 (SR-AUD-253 —
**all eight** `SendPingAsync` overloads validated on the worker, so an already-invalid argument got a
task and an OS thread instead of a throw), #2190 (SR-AUD-255 — three doors invented a default
`PingOptions` where the fourth already forwarded `nullptr`), #2191 (both async lambdas captured a raw
`this`; `sizeof(Ping) == 1`, so the capture was removable outright — this does **not** touch the
blocked stateful family #2066/#2088/#2134) and #2193 (the send core held a raw descriptor across five
allocating operations, and closed it **before** reading the `errno` it reports).
**`modules/net-network-information` is closed except for two remainders**, neither of them an audit
finding: **#2192** (deferred verification — a DNS failure escapes the wrapper as `SocketException`
while the module's own unresolvable-host `PingException` is practically unreachable; needs `/rv` or a
managed runtime, current behaviour pinned) and **#2194** (blocked — the receive path matches no
source/identifier/sequence and discards every `setsockopt` result, untestable while every send fails
at socket creation). **No `SR-AUD-*` identifier created — numbering frozen at 364**; audit
**170 remediated / 194 confirmed / 364 total**, of which **54** carry `confirmed (design-complete)`,
unchanged. Gate **16,406 across 37 executables, 16,398 passing, 2 skipped, 6 failing** for the same
two measured causes (+23, exactly this batch's additions, so no regression anywhere). Graph
**41 / 92**, seams **3 / 20**, negative fixtures **13 / 116** — all unchanged. ASan+UBSan+LSan,
non-recovering UBSan and **TSan** over the production module bodies: exit 0, **zero reports** — and
what they cannot reach is stated too. **Doxygen, `ccache` and `/rv` all absent.** CCF-019 open;
CCF-021/#2131 and CCF-022/#2109 unminted; #1773, #1962, #2150, #2152, #2155, #2166, #2170, #2172,
#2175, #2185 and #2186 unchanged. **Next unit, measured: `modules/xml-linq`** — 4 open, of which 3
are compatible-actionable and the one high **is** CCF-019, already design-complete and blocked.
Maximum aggregate compiler parallelism **2 jobs**.*

## 2026-08-10 — the `modules/net-network-information` review (#2187) and its whole compatible queue (#2188–#2193)

**Selected by recount, not inheritance.** The audit index was re-parsed from scratch: **167
remediated / 143 confirmed / 54 design-complete = 364**. Unlike the previous batch, the inherited
figure was **correct** and needed no correction. Among unreviewed units `net-network-information` was
the only one with **zero blocked and zero approval-gated** findings, all three actionable here.
Deliverable: `docs/SystemNetNetworkInformationNamespaceReviewPlan.md`.

**The load-bearing claim was proved, not assumed.** All three findings are independent of blocked
**#1962**: SR-AUD-253 reproduces with no socket involved; SR-AUD-254 is *reached* because the socket
fails but its defect is unconditional; SR-AUD-255 is structural for the same reason it always was.
The capability was re-measured — `SOCK_DGRAM/IPPROTO_ICMP` denied (EACCES), `SOCK_RAW/IPPROTO_ICMP`
succeeding, `ping_group_range = "1 0"`. **#1962 is untouched and still blocked**, and the five
`PingTests` that fail for want of an unprivileged ICMP socket still fail, unweakened.

**Five premise corrections, each measured before any code changed:**

| Correction | Evidence |
|---|---|
| SR-AUD-253 reaches **all eight** `SendPingAsync` overloads, not the one the audit's probe names | probe 1, ten doors |
| It starts a **real OS thread** per already-invalid argument — `TaskT` is `std::async(std::launch::async, …)` | probe 2 |
| SR-AUD-254 destroys the **native error code**, not merely the type: `NetworkInformationException("Win32 error 13", code 13)` became `St9exception`/`"std::exception"` | probe 1 |
| SR-AUD-254 has a **second, opposite door**: `Dns` runs *outside* the wrapper, so a resolver failure escapes as `SocketException` while an unresolvable-host `PingException` sits three lines later on a practically unreachable branch | probe 1 |
| SR-AUD-255 is **three** sites and its fourth sibling was already correct, so the repair rests on an inconsistency **inside one file** | source, probe 1 |

**Three post-audit defects, ordinary ticket numbers only** — #2191 (raw `this`), #2193 (raw
descriptor), #2194 (receive-path matching, blocked) — plus #2192 (the DNS wrapping question,
deferred). **No CCF minted or closed**: CCF-019 stays open, CCF-021/#2131 and CCF-022/#2109 stay
unminted, CCF-004 gains no member.

**Two limitations recorded rather than hidden.** A mutation restoring SR-AUD-255's fabricated
`PingOptions()` leaves the suite **green** here, because the only test that can observe it needs a
reply and skips — so **this batch has no discriminating test for #2190 in this container**, and
raising `ping_group_range` to get one is outside the boundary. #2191 and #2193 likewise have no
runtime discriminator: nothing was ever dereferenced through the stale `this`, and no descriptor is
ever successfully opened here.

**Gate:** module suite **39 → 62** tests; repository **16,406 across 37 executables**, 16,398
passing, 2 skipped, 6 failing — the same five `PingTests` (#1962) and one `SocketTests`
(`/proc/net/if_inet6` absent), re-verified this run. Four mutations run, three discriminating
exactly their own pins with controls stable and the fourth recorded as non-discriminating.
`Ping.hpp` untouched; no public signature, member, virtual, vtable, object layout, `noexcept` or
mangled symbol changed.

## 2026-08-10 — the `modules/time-zone` review (#2176) and its whole compatible queue (#2177–#2184)

**Selected by recount, not inheritance.** The audit index was re-parsed from scratch: **161
remediated / 150 confirmed / 53 design-complete = 364**. The batch prompt's *160/154/50* was stale;
the index and `NEXT.md` agree with each other and with the recount. Among unreviewed units
`time-zone` was the only one with 7 open findings, **zero high, zero blocked, zero approval-gated**
and full decidability here — `/usr/share/zoneinfo` holds **499 TZif zones**. Deliverable:
`docs/SystemTimeZoneNamespaceReviewPlan.md`.

**All seven findings reproduced in one process before any code changed.** Six premise corrections,
each measured:

| Correction | Evidence |
|---|---|
| SR-AUD-229 is **three** properties, not one — the same snapshot fed `BaseUtcOffset`, `StandardName` and `DaylightName`, so both names carried the daylight abbreviation all summer | probe 1 |
| Its reach is **158 of 499** zones; **141 wrong on 2026-08-10**, the other 17 wrong in January | probe 4, whole database |
| It contradicts the port's **own doc-comment** and the Windows branch, so the repair changes no contract | source |
| SR-AUD-223 is **two** independent defects — frozen offset *and* hard-coded `false` | probe 1 |
| SR-AUD-225 left a public door onto `TimeSpan`'s negation guard — a **validation** defect, so **CCF-004 gains no member**, confirmed by a clean UBSan run | probe 1, UBSan |
| SR-AUD-226 affects **both** overloads; SR-AUD-227's hash was **locale-dependent** | probe 1, source |

**Three post-audit defects, ordinary ticket numbers only.** #2183: glibc parses a `TZ` value that is
not a path as a POSIX rule string, so all seven non-TZif files in `/usr/share/zoneinfo` resolved as
zones — and a `':'` prefix to force file interpretation was **measured and does not help**, so the
door needs a `TZif` magic check. #2184: the `TZ` restore was straight-line and branched on
*emptiness* rather than *presence*. And a third, found while writing #2182's pins: raw
`mktime(tm_isdst = -1)` answers a **repeated local hour** differently depending on the preceding
conversion in the same process (−240 / −300 / −240 for one argument), which is a defect whatever
.NET does; it is repaired inside #2182 because it is inside SR-AUD-223's own cause.

**Ticket outcomes**

| Ticket | Outcome | Commit |
|---|---|---|
| **#2176** review | **done** | `886c066` |
| **#2177** SR-AUD-224 | **done** — remediated | `a431bc8` |
| **#2178/#2179/#2180** SR-AUD-225/226/227 | **done** — all remediated | `7888a29` |
| **#2181/#2182/#2183/#2184** SR-AUD-229/223 + two post-audit | **done** — both remediated | `9210d8f` |
| **#2185** SR-AUD-228 | **needs_user** — approval for `sizeof` 160 → 184 | `11b4fc8` |
| **#2186** five parity questions | **todo, inactive** — needs `/rv` or a managed runtime | `11b4fc8` |

**The one approval sentence, stated exactly.** *"`System::TimeZoneInfo` may grow from 160 to 184
bytes (one `std::vector<std::shared_ptr<AdjustmentRule>>`) so that `HasSameRules` and
`GetAdjustmentRules` can distinguish two zones that share a base offset and a daylight flag but not
their transition rules, requiring every consumer to be rebuilt."* Three alternatives were considered
and rejected (comparing `id_` is wrong for differently-named zones with identical rules; comparing a
sampled offset function invents an algorithm .NET does not use; deriving rules per call means twelve
`setenv`/`tzset` cycles under the global lock). The failure is **one-directional** — this port can
only be too permissive, never too strict — which is why it is medium, not high. Four `PIN_` tests
and a `static_assert` on `sizeof` hold it so the question cannot be answered silently.

**A correction of record.** An earlier draft of the review plan carried an *estimate* of
`sizeof(TimeZoneInfo)` = 144 and a gate of 144 → 168, and that estimate reached the migration note,
`README.md` and commit `9210d8f`'s message before the shape was compiled. Measured, it is
**160 → 184** — `TimeSpan` is 24 bytes here, not 8. The substantive claim is unaffected (the batch
adds no member; `sizeof` is 160 before and after) and `9210d8f` is not rewritten.

**Evidence.** +73 tests (114 → 187 in the component). **Eight mutations proven**, each failing
exactly its own pins with controls stable, covering under-repair *and* over-rejection in both
directions; one mutation whose build failed was discarded and re-run. **UBSan (non-recovering),
ASan+LSan+UBSan and TSan** over the production bodies: exit 0, zero reports, TSan driving 8 threads
through concurrent `Local()`, `FindSystemTimeZoneById()` and `CurrentTimeZone()`. Behaviour changes
are documented in `docs/Migration-TimeZoneStandardOffset.md` and `README.md`.

**Next unit, measured: `modules/net-network-information`** — see `NEXT.md` for the scoring and the
reproduction that shows its three findings are independent of blocked #1962.

## 2026-08-10 — the `modules/numerics` review (#2167) and its whole compatible queue (#2168, #2169, #2171, #2173)

**Selection, and the claim this batch disproved.** `numerics` (4 open, 0 high) was chosen over
`time-zone` (7 open, 0 high) and `globalization` (7 open, 1 high) as the largest unreviewed real
namespace with zero blocked findings that is decidable without `/rv`. That ranking rested partly on
an inherited claim — that `time-zone` needs a tz database this container lacks — which is **false**:
`/usr/share/zoneinfo/America/New_York` is present, `FindSystemTimeZoneById` resolves it, and three
of `time-zone`'s seven findings reproduced in a single probe. Recorded in
`docs/SystemNumericsNamespaceReviewPlan.md` §1 and §17 rather than quietly fixed.

**The four findings, and what each became.** SR-AUD-278 → **remediated** (#2168). SR-AUD-042 →
#2169 done + **#2170 needs_user**, split on a measurement: the layout-neutral subpart is 8/8, the
complete repair is 8 → 16 with a second vptr, the class this repository has gated since #1788/#1789.
SR-AUD-277 → #2171 done + **#2172 needs_user**, split because a public return type has no conversion
path. SR-AUD-276 → #2173 done + **#2175 blocked on evidence**, because the finding carries no managed
probe and `Plane::Normalize` — measured to behave *oppositely* to the vectors — would need its own
answer. Every finding maps to exactly one disposition; none disappeared.

**Five mutations, five counted.** Undeleting `IMinMaxValue::MinValue` made negative site 1 compile
again while the other 11 stayed rejected; `Equals` → ordinary floating equality produced 4 clean
failures; a truncating binary64 hash produced 2; `Complex::ToString` → `std::to_string` produced 5;
and applying the **#2175 shape** fired **10 of #2173's 16 pins**, which is the right mutation for a
pins ticket — it proves the pins detect the change they exist to make visible. All reverted.

**Namespace 299 → 335 tests.** No layout, signature, vtable, seam or module-graph change; negative
fixtures gained one file and twelve sites. Full record in
`docs/SystemNumericsNamespaceReviewPlan.md` §15–§17 and in `NEXT.md`.

## 2026-08-09 (later still) — `modules/security-cryptography` fully closed (#2158–#2161) and `modules/console` fully closed (#2162–#2165)

**security-cryptography.** Selected over larger units because it was the only unreviewed unit whose
open findings were **100 % high**, its consequence class is disclosure of a caller's key rather than
a wrong answer, and it has zero `/rv` dependence. Both findings were narrower than the defect. A
replacement global `operator delete` snapshotting each block as it is released — against a
deliberately uncleared 96-byte control that read 96 in **both** columns — found five sites beyond
the two the audit names: `derivePads`' `keyPrime` local held the raw key *even after `Dispose`*,
`HashFinal`'s two working buffers leaked both pads on **every** `ComputeHash`, key replacement
released the old key unerased, plain destruction without `Dispose` erased nothing at all, and a
131-byte key left a 36-byte tail of itself inside the digest object that hashes it. All read 0
afterwards. **`std::fill` was not an option and that is measured, not assumed**: at `-O2` GCC 13.3
deletes it outright, both on a dying stack buffer and immediately before `operator delete`, so the
primitive is a `volatile` write loop whose surviving store loop is visible in the disassembly. The
fix could itself have introduced a regression — the new erasing destructors suppress the implicit
moves unless explicitly defaulted, and a suppressed move becomes a **copy** that leaves the source
holding a live key — so all four operations are `= default`ed and pinned, and the mutation that
removes them fails exactly the two move tests. Repairing SR-AUD-331 exposed that `getSaltProperty`
computes `salt_.end() - 4`, so erasing the salt without guarding that door would have swapped a
disclosure defect for undefined behaviour; guard and erasure had to land together. **Namespace
80 → 134 tests; 2 findings, 2 remediated, 0 open, nothing gated.**

**console.** Both findings carry a managed probe result inside the audit itself, which is what made
them actionable with `/rv` absent. SR-AUD-243 is worse than recorded: `ansiColor` formats into
`char buf[12]`, sized for the 0–15 domain, so `INT_MIN` emitted `ESC[-21474836` — truncated
mid-number with **no terminator** — and a terminal that receives an unterminated escape consumes the
output that follows it. A third defect was found by **pinning** rather than by repairing: because
the review deliberately enforces no upper cursor bound, `INTCS_MAX` is reachable and `left + 1` was
signed integer overflow there (UBSan 2 → 0 after widening). Six adjacent doors accept the same shape
of invalid input and were **deliberately left alone** — no managed probe measured any of them, so
implementing them would be recollection — and are pinned by three `PIN_` tests under #2166.
**Namespace 127 → 142 tests; 2 findings, 2 remediated, 0 open.**

**A third test-only access seam.** `SharpRuntime::Testing::KeyMaterialAccess<T>` was **required**,
not convenient: erasure is unobservable through any public API by construction, and the
freed-storage recorder cannot separate "`Dispose()` erased it" from "`~HMAC()` erased it" — both
leave the same zeroed block at the same moment, and the first is what SR-AUD-332 is about. Both
halves of `CLAUDE.md`'s seam rule are satisfied. `scripts/check_version_seam_odr.py` **correctly
rejected** two macros that each defined `KeyMaterialAccess<OwnerType>` with a different body —
exactly the divergence #1800 measured — so the `Rfc2898DeriveBytes` specialisation is written out
literally.

**Honest limitations, recorded rather than left to be discovered.** The permanent crypto suite does
not assert freed-storage state, because installing a replacement global `operator new`/`delete` in
that executable would displace AddressSanitizer's own and weaken every other suite in the binary; so
the **destructor's** erasure is pinned by the probe, not by the suite. Nothing claims OS-wide
erasure. TSan was not run and is not claimed for either namespace — neither documents a
thread-safety contract for a shared instance. Two mutations are recorded as **honest non-results**:
reverting the reserve-once restructure is invisible to the live-object seam, and swapping the
primitive for `std::fill` survives the tests because they read the buffer afterwards; the
discriminating instruments there are the freed-storage probe and the disassembly.

*Last verified: 2026-08-09 — branch `claude/remediation-batch-1804-namespace-b1yjh5`, the
harness-designated branch, continued from its own tip `6676a08`. **Pushed after every commit**, per
`CLAUDE.md` rule 13 and the harness branch policy; the tension with the batch brief's "do not push
unless requested" is recorded rather than resolved by silence. No merge, rebase, tag, force-push,
PR, publication, amend or history rewrite; all commits unsigned. This batch **finished
`modules/io-compression`** — #2148 (SR-AUD-258: an out-of-domain `CompressionMode` was a
LeakSanitizer-confirmed leak of the whole zlib state, and three doors, not one, kept answering after
`Close()`), #2149 (SR-AUD-259's strategy half: 45 of 45 outputs were byte-identical to `Default`
before, 0 of 45 diverge from zlib after) and #2151 (the contract in the headers plus a compile-time
pin on #2150's absence) — and then **reviewed and closed `modules/timers`**: #2153 (the review),
#2154 (SR-AUD-238: a throwing `Elapsed` handler aborted the process, 7 of 7, taking unrelated timers
with it), #2156 (a post-audit defect — the interval setter's domain was narrower than the
constructor's and the gap was undefined behaviour) and #2157 (the lifecycle matrix made permanent
plus the reconciliation). Audit **156 remediated / 208 confirmed / 364 total**,
`confirmed (design-complete)` **50**; **no `SR-AUD-*` identifier created — numbering frozen at
364.** Gate **16,205 across 37 executables, 16,198 passing, 1 skipped, 6 failing** for the same two
measured causes (+53). Graph **41 / 92**, seams **2 / 18**, negative fixtures **11 / 94**.
**Doxygen, `ccache` and `/rv` all absent.** CCF-019 open; CCF-021/#2131 and CCF-022/#2109 unminted;
#1773, #1962, #2150 and #2155 blocked. Maximum aggregate compiler parallelism **2 jobs**.*

## 2026-08-09 (later) — `modules/io-compression` finished (#2148, #2149, #2151) and `modules/timers` reviewed and closed (#2153, #2154, #2156, #2157)

**io-compression.** All three inherited `todo` tickets classified from their exact rows and the
durable plan, and all three **compatible implementation-ready**, so all three were implemented.
#2148's two premise corrections were measured before anything was edited: an out-of-domain
`CompressionMode` leaks the whole zlib state (the constructor splits on `Decompress`, `Close()` on
`Compress`, so `inflateEnd` was handed a `deflateInit2` stream — LSan: 5,952 direct + 65,536
indirect bytes per object, clean after), and the closed-state defect was on `Read` and `Flush` as
well as the `Write` the finding names. Its acceptance criterion named
`ArgumentOutOfRangeException`; the base `ArgumentException` was implemented instead, because the
audit's own managed probe recorded that category for .NET — corrected in the plan, the audit record
and the ticket rather than swapped silently. #2149 widened the evidence from one pair to 45 cases
and left the default-option output byte-identical. #2151 documented the contract and pinned #2150's
absence at compile time. **Namespace 40 → 101 tests; closed except for #2150 (approval) and #2152
(a new inactive post-audit ticket, pinned).**

**timers.** The next unit was re-derived rather than inherited, and the comparison against
`security-cryptography` is recorded: timers wins because its high **terminates the process** (7 of
7 SIGABRT, killing unrelated timers with it) while the rival's highs need a separate disclosure
primitive, and because the rival could not be closed in one pass either. Four premises corrected,
including that the exception escapes a raw `std::thread` entry point in **modules/threading** (left
unchanged, because .NET's `System.Threading.Timer` also crashes) and that a third, unrecorded defect
existed: the interval setter accepted four values the constructor rejects, all of which reached an
undefined float-to-integer conversion. **GCC's `-fsanitize=undefined` does not include
`float-cast-overflow`**, which is why it survived every previous UBSan pass. SR-AUD-239 is
`confirmed (design-complete)` and blocked: its only repair takes `sizeof(Timer)` 104 → 112 and adds
a vtable. **Namespace 9 → 36 tests; closed except for #2155.**

**Mutation testing earned its keep twice.** Mis-mapping `RunLengthEncoding` to `Z_FIXED` survived
#2149's suite until a pairwise-distinctness assertion was added, and narrowing #2154's catch to
`std::exception` survived until every throwing test was given a sentinel timer to wait on after the
throw. One surviving mutation is recorded as an **honest non-result**: always resolving memLevel 8
is observationally equivalent, because the port picks memLevel 7 only at quality 0, where zlib emits
stored blocks.

## 2026-08-09 — io-hashing verified closed, the `modules/io-compression` review (#2147) and #2146

**The brief's first two work units were already done.** `c8d8b71`/`fab0c99`/`03f6eb0`/`3827ded`/
`3d022d8` and `f38b50c` landed #2142, #2141, #2143, #2144, the reconciliation and #2145 in the
previous context. Verified independently — all three findings `remediated`, `IO_Hashing` 131/131,
no ticket `doing` — and **not redone**. No commit was created to restate existing work.

### The next unit, re-derived rather than inherited

Every module with ≥ 2 open findings was re-scored from the index. `io-compression` won on a
combination no larger candidate matched: an **actionable** memory-safety high (every larger
candidate's high is blocked — `xml-linq`'s is CCF-019 — or architecture-gated — `globalization`'s
is a process-global race), **zero `/rv` dependence** where `globalization` and `time-zone` are
predominantly parity-bound, **hostile-input exposure** (its job is decompressing attacker bytes),
and an idiom the sibling `IO::Hashing` batch had just settled.

### #2146 — SR-AUD-256

Every raw-pointer door handed its `intcs` length to zlib as unsigned `uInt`:
`zs.avail_in = static_cast<uInt>(sourceLength)`. A negative length became an enormous count and
zlib ran off the caller's allocation — ASan reporting a 65,536-byte READ past a one-byte source.
Repaired with one module-local `Detail::ValidateSource`/`ValidateDestination` choke point whose
rule, exception types and messages are **identical to `System::IO::Hashing::Detail`**, so the two
sibling components do not answer "what does a negative length mean" two different ways.

| Matrix | Cases | Crashed |
|---|---|---|
| before | 63 | **15** |
| after | 63 | **0** |

**Four corrected premises**, all measured. The load-bearing one: the source-side defect is in
**all three** encoders. Probe 1 appears to clear GZip and ZLib, but that is an artefact of its
1-byte destination — a gzip/zlib header fills it before `deflate()` reads the source, and raw
deflate has no header. With a 4096-byte destination all three crash. Also: the three **decoders**
never crashed at all (21/21 normal) because `inflate()` rejects garbage first — luck, not a guard —
and null handling already differed between encoders (threw 6/6) and decoders (normal 6/6).

**Mutations: three run, two count.** Removing `ValidateSource` gave SIGSEGV — an abort/UB-only
outcome that the batch's own rules exclude, reported rather than counted. The two that count are
an over-rejection mutation (7 clean failures, all zero-length and round-trip pins) and an
output-corruption mutation (**exactly** the 2 output pins, with all 73 bounds tests green).

**+35 tests**, 40 → 75. No signature, `noexcept`, vtable, layout or ABI change.

### Remaining

#2148, #2149 and #2151 are `todo`; #2150 is `blocked` as a public surface addition. The next unit
by the same measurement is **`modules/timers`** (2 open, 1 high — an exception escaping a worker
thread and aborting the process).

*Prior plan snapshot, retained historically: 2026-08-08 — branch `claude/remediation-batch-1804-namespace-b1yjh5`, the
harness-designated branch, continued from its own clean tip `f013fe1`. **Not pushed; no push was
requested during this batch.** No merge, rebase, tag, PR, force-push, amend or history rewrite; all
six new commits intentionally unsigned (`git -c commit.gpgsign=false`), authored and committed as
`Claude <noreply@anthropic.com>`. The batch first **reconciled the inherited three-test gate
discrepancy and found the inherited 16,005 was RIGHT**: the tip enumerates 16,052, the only test
files changed since `a3cfa69` add 43 + 4 = **47** registrations and delete none, so `a3cfa69` was
16,005 and the error was the previous report's **"+50"** — #2135's record said "+7 (84 → 91)" where
measurement says "+4 (88 → 92)", and the `net-sockets` executable has **never** been 84 or 91 at any
commit in this repository's history. It then **closed `modules/net-sockets` for compatible work**.
**#2136** found the finding far wider than filed — a **listening socket**, a **regular file** and a
**pipe** all constructed a `NetworkStream` — and put the validation in the constructor **body**, so
"a rejected construction leaks nothing" is **structural**: a throwing body means the destructor
never runs. **#2137** found **three** bare-`int` port doors rather than one, and the two extra ones
failed *more* misleadingly — `getaddrinfo` blamed **DNS** for the caller's argument while a port
above 65535 was truncated and connected to the wrong port; its endpoint half needed **a state, not a
member** (`fd_ >= 0 && !connected_` was previously unreachable), so `sizeof(TcpClient)` is unchanged.
**#2139** pinned what is gated and corrected a premise that changes **#2138**'s cost: the IPv4-only
limitation is **not silent** — every endpoint path throws `SocketException(OperationNotSupported)`
from `IPAddress::getAddressProperty()` — so option (b) is *"make it say what it means"*, not *"make
it loud"*. **Eleven mutations** were run, and the four that failed **exactly one** test each are the
ones that prove an *ordering* rather than a check. **#2140** then reviewed `modules/io-hashing`,
selected because **three of three** findings are compatible work with **zero** waiting on `/rv`; a
**fork-per-case** probe measured **102 cases, 58 crashing**, and found SR-AUD-260's **destination**
half larger than its source half and living in **one** file. **#2145** fixed a ~7% flaky gate test
the required validation exposed — a `std::vector<bool>` data race in the test's own bookkeeping, not
a PRNG defect, and proved pre-existing because the failing binary predates this batch by four days.
**Two findings moved `confirmed → remediated`** (SR-AUD-265, 267) plus SR-AUD-266's endpoint half:
the audit index reads **150 remediated / 214 confirmed / 364 total**, **49** design-complete,
numbering **frozen at 364**. Gate **16,082 tests across 37 executables: 16,075 passing, 1 skipped, 6
failing** for the same two re-measured causes. Graph **41 / 92** (unchanged), seams **2 / 18**,
negative fixtures **11 / 94**. **No CCF was minted; CCF-012 and CCF-019 were NOT marked closed;
#1962 and #1773 remain blocked.** The prior header is retained below.*

---

## 2026-08-08 (later) — the gate reconciliation, `modules/net-sockets` closed (#2136, #2137, #2139), the `modules/io-hashing` review (#2140) and #2145

### What shipped

| # | Subject | Result |
|---|---|---|
| — | the inherited **16,005 vs 16,002** gate discrepancy | **inherited 16,005 was right**; the "+50" was the error |
| **#2136** | `NetworkStream` took any `int`, and a closed stream silently accepted writes (SR-AUD-265) | **done**, `remediated`, +13 tests |
| **#2137** | two constructors discarded the caller's argument (SR-AUD-267 + SR-AUD-266 endpoint half) | **done**, 267 `remediated`, +11 tests |
| **#2139** | net-sockets gated-behaviour pins and namespace reconciliation | **done**, +6 tests |
| **#2140** | the `modules/io-hashing` namespace review | **done** — plan + 4 tickets, **all compatible** |
| **#2145** | a ~7% flaky gate test (`std::vector<bool>` race in the test) | **done**, 2/30 → 0/60 |

### What remains in `modules/net-sockets`

**#2134** (`blocked`, CCF-019, now pinned by shape) and **#2138** (`needs_user`, IPv6 scope, pinned).
Nothing compatible remains. **CCF-019 is NOT marked closed.**

### What is next

`modules/io-hashing`: **#2142** → **#2141** → **#2143** → **#2144**, in that order — #2142 first
because it adopts a guard that already exists in the module's four XXH types, which is where the
shared validation helper comes from; the other order writes the helper twice. **All four are
compatible; the module has no gated remainder.**

---

## 2026-08-08 — `modules/net-http-headers` closed for compatible work (#2124–#2127, #2129, #2132), CCF-021's final answer, the `modules/net-sockets` review (#2133) and #2135

### What shipped

| # | Subject | Finding | Result |
|---|---|---|---|
| #2124 | one field-terminator predicate for the whole module; **+1 component edge** | SR-AUD-319 | done, `remediated` |
| #2125 | one HTTP-date parser that consumes its whole value (7 copies) | SR-AUD-321 | done, `remediated` |
| #2126 | one escape-aware list splitter (7 splitters) — a **widening** | SR-AUD-320 | done, `remediated` |
| #2127 | the RFC 5987 charset label is normative — 3 defects | SR-AUD-323 | done, `remediated` |
| #2129 | an RFC 5987 value decoded to a raw CR/LF for the **caller** | post-audit | done |
| #2132 | gated-behaviour pins + the namespace reconciliation (§19) | — | done |
| #2133 | the `modules/net-sockets` namespace review | 5 open | done — plan + 6 tickets |
| #2135 | a negative `SendPacketsElement` count meant "the whole buffer" | SR-AUD-264 | done, `remediated` |

### Still open, untouched, and correctly classified

`#2128` (needs_user, pinned), `#2130` (deferred verification), `#2131` (CCF-021 mint), `#2109`
(CCF-022 mint), `#2134` (blocked, CCF-019), `#2138` (needs_user, IPv6 scope), `#2136`/`#2137`/`#2139`
(todo, compatible — the next work), Approval IO-1 / `#2098`, `#2115`, `#2117`/`#2118`, the
Diagnostics / `System::Text` / Uri / XML approval packages, `#1962` and `#1773`.

---

*Last verified: 2026-08-08 — branch `claude/remediation-batch-1804-namespace-b1yjh5`, the
harness-designated branch, continued from its own clean tip `5aca799`. **Not pushed; no push was
requested during this batch.** No merge, rebase, tag, PR, force-push, amend or history rewrite; all
seven new commits intentionally unsigned (`git -c commit.gpgsign=false`), authored and committed as
`Claude <noreply@anthropic.com>`. The batch **closed the compatible `System::Text::Json` queue** and
then reviewed **`modules/net-http-headers`**, discharging **CCF-021's evidence obligation** without
minting it. **#2113** found that `JsonEncodedText`'s narrow `Encode` had **no validation at all** —
and that the module was **contradicting itself**, certifying as "validated JSON text" five byte
classes its own parser rejects, while raw `0x7F` is accepted by both, which is what makes it a
finding rather than general leniency; its control-character boundary was **measured** by a
granularity matrix rather than stipulated, landing on exactly RFC 8259's 29 bytes with tab, LF, CR
and DEL deliberately outside. **#2114** found the surviving half of SR-AUD-328 to be **undefined
behaviour**, not truncation — and caught a **would-be false clean**, because GCC's
`-fsanitize=undefined` does **not** include `float-cast-overflow`; with it enabled, three reports
before and zero after. It also **corrected its own acceptance criterion**, which asked for the
wrong exception type against the port's transcribed .NET evidence. **#2116** found SR-AUD-330's real
defect to be **two parsers**, not a discarded argument, and **#2121** — a fifth parse door #2112's
NUL guard never reached, found by #2114's probe — corrected the plan's own claim **additively**
rather than editing it away. **#2120** delivered the behaviour pins the completion criteria
required, closing Text.Json for compatible work. **#2122** then selected `modules/net-http-headers`
by re-measurement at **5 open findings**, below the ≥6 threshold, on the **highest high-severity
ratio in the repository (40%)**, on both `high` findings being protocol-field injection, and on
decidability with `/rv` absent — with CCF-021 listed **fourth**. It measured that the module is
**not on this repository's own wire path**, which forces CCF-021's guarantee for its two members to
be stated one step earlier, and found a new request-smuggling shape (singleton headers joined with a
comma) filed as the design ticket **#2128**. **#2123** then landed, and its interesting mutation was
the one that **over**-repaired: validating the value as well as the name fails exactly one test, the
one pinning that "without validation" governs the value and never the name. **Four findings moved
`confirmed → remediated`** (SR-AUD-328, 329, 330, 322): the audit index reads **143 remediated / 221
confirmed / 364 total**, **49** design-complete, numbering **frozen at 364**. Gate **16,005 tests
across 37 executables: 15,998 passing, 1 skipped, 6 failing** for the same two re-measured causes.
Graph **41 / 91**, seams **2 / 18**, negative fixtures **11 / 94**. **No CCF was minted; CCF-012 and
CCF-019 were NOT marked closed; #1962 and #1773 remain blocked.** The prior header is retained
below.*

---

## 2026-08-08 — `System::Text::Json` closed for compatible work (#2113, #2114, #2116, #2121, #2120), the `modules/net-http-headers` review (#2122), CCF-021's evidence, and #2123

| # | Subject | Findings | Result |
|---|---|---|---|
| #2113 | `JsonEncodedText`'s narrow `Encode` had **no** validation | SR-AUD-329 | done, `remediated` |
| #2114 | a non-exact JSON number conversion — measured to be **UB** | SR-AUD-328 | done, `remediated` |
| #2116 | `Deserialize` discarded its options — really **two parsers** | SR-AUD-330 | done, `remediated` |
| #2121 | a **fifth** parse door #2112's NUL guard never reached | post-audit | done |
| #2120 | Text.Json gated-behaviour pins and the §21 reconciliation | — | done |
| #2122 | the `modules/net-http-headers` namespace review | 5 open | done — plan + 11 tickets |
| #2131 | CCF-021: evidence **discharged**, mint filed as a decision | — | `needs_user` |
| #2123 | `TryAddWithoutValidation` accepted a CR/LF header **name** | SR-AUD-322 | done, `remediated` |

**Durable records:** `docs/SystemTextJsonNamespaceReviewPlan.md` §20.3–§21.5 and
`docs/SystemNetHttpHeadersNamespaceReviewPlan.md` (18 sections), plus the CCF-021 discharge appended
additively to `audit/AUDIT_CROSS_CUTTING_FINDINGS.md`.

**Eleven premises corrected by measurement**, seven of which changed what shipped; **three checks
caught not discriminating and fixed rather than reported**, including a would-be false clean under
UBSan, an ASan control masked by an earlier UBSan abort, and a mutation whose build failed and was
recorded as invalid rather than as a result. Full detail in `NEXT.md`.

**Still open and untouched:** #2115, #2117, #2118, #2119, #1888/#1889/#1894 (Text.Json); #2124–#2132
(net-http-headers); #2098/#2099/#2102/#2104 (io); #2109; Approval IO-1.

---

*Last verified: 2026-08-04 — branch `claude/remediation-batch-1804-namespace-b1yjh5`,
fast-forwarded along existing history to the clean tip `13917c2` and developed from there.
**Not pushed; no push was requested during this batch.** No merge, rebase, tag, PR, force-push,
amend or history rewrite; all six new commits intentionally unsigned
(`git -c commit.gpgsign=false`), authored and committed as `Claude <noreply@anthropic.com>`.
The batch completed **#2100** — which repaired **every** `RandomAccess` argument domain rather
than the two SR-AUD-340's index summary names, because the **owning per-file report is broader
than the summary** and already named null buffers, negative lengths, read-only descriptors,
non-seekable handles and write-zero-progress — and **#2107**, the `HttpClientDescriptorLeakTests`
descriptor instrument, whose defect turned out to run in **both** directions: a lagging server
thread produces **−1** (the reported symptom) from an inflated **baseline** and **+1**, *a false
leak report*, from an inflated final sample, so the ticket's stated root cause named only **half**
the fix. It then **measured #2098's layout gate** instead of accepting it, which **refuted three
of the plan's own premises** — `TextWriter::Close()` **does** exist, so no vtable slot is needed;
option (a) is layout-**neutral** for **four of five** types because the flag lands in existing tail
padding; and `UnmanagedMemoryStream` **already has `isOpen_`** — leaving a decision **one type
wide** (`StringWriter`, 384 → 392) against option (b)'s **six**, with the exact approval sentence
recorded as **Approval IO-1** and the compatible half split out and shipped as **#2108**. It
**reconciled the CCF numbering policy** (#2109): *"the numbering is closed"* is scoped to
namespace-local causes, not a global freeze, proven from the same document that instructs
*"Mint CCF-021 when `net-http-headers` is reviewed"* — so the policy obstacle is gone, **CCF-022
is still not minted**, and the remaining question (who may mint, and whether to mint a family two
of whose six sites are blocked) is a bounded decision with three options and a recommendation.
Finally it **re-derived the next review unit by measurement** and performed the
**`modules/text-json` namespace review (#2110)** —
`docs/SystemTextJsonNamespaceReviewPlan.md`, 20 sections, the twelfth in the #1950 series —
correcting **four** of that module's seven findings and finding **two remotely-triggerable
post-audit defects that no finding names**: a `std::` exception escaping **four** public parse
doors on an eight-character number literal, and an embedded NUL **silently truncating** a
document, proven by a space control that is rejected. Both shipped as **#2111** and **#2112**.
**Two findings moved `confirmed → remediated`**: SR-AUD-340 (#2100) and SR-AUD-344 (#2108). Audit
**139 remediated / 225 confirmed / 364 total**, of which **49** carry `confirmed (design-complete)`;
**no `SR-AUD-*` identifier was created — numbering stays frozen at 364**, and **no CCF was minted**.
Gate **15,967 tests across 37 executables: 15,960 passing, 1 skipped, 6 failing** for the same two
re-measured causes. Graph **41 / 91**, seams **2 / 18**, negative fixtures **11 / 94**. Selective
components passed in **748s** at a verified peak of **2** `cc1plus`. **#1962 and #1773 remain
blocked.** The prior header stack is retained below.*

---


*Last verified: 2026-08-04 — branch `feature/remediation-batch-xml-compatible-websockets-review`,
cut from the clean tip `e1cc8c3` and **not pushed; no push was requested during this batch**. No
merge, rebase, tag, PR, force-push, amend or history rewrite; all eight new commits intentionally
unsigned (`git -c commit.gpgsign=false`), authored and committed as `Claude
<noreply@anthropic.com>`. The batch **closed the compatible `System::Xml` queue** with **#2076**,
**#2078**, **#2079** and **#2081**, **reconciled the namespace** — and that reconciliation found
**three unmet completion criteria** the queue itself had left behind — then **re-derived the next
review unit by measurement** and performed the **`System::Net::WebSockets` namespace review
(#2087)** — `docs/SystemNetWebSocketsNamespaceReviewPlan.md`, 20 sections, the eleventh in the
#1950 series — and implemented the first ticket it created, **#2090**. **Four findings moved
`confirmed → remediated`**: SR-AUD-349 (#2076), SR-AUD-348 (#2078), SR-AUD-352 (#2079) and
SR-AUD-355 (#2081). **Nine premises were corrected by measurement**, four of which changed what
shipped. The largest is that **the WebSocket frame parser is entirely unaudited** — not one of the
module's six findings names `readFrame`, the code that touches remote bytes — and reading it
produced **eleven post-audit protocol-validation defects**, including reserved opcodes delivered
to callers as application data, a masked server frame accepted where RFC 6455 says a client MUST
fail, and a 256 MiB "Ping" read into memory **and echoed straight back**. In `System::Xml` the
through-line is that in every one of the four tickets **the correct pattern was already present in
the module and simply not reached from the defective door**. **Four non-discriminating checks were
caught and fixed rather than reported as passes**, including a **stale-binary false pass** where a
suppressed failing build made a mutation report the previous binary's result. Audit **133
remediated / 231 confirmed / 364 total**, of which **49** carry `confirmed (design-complete)`; **no
`SR-AUD-*` identifier was created — numbering stays frozen at 364**, and **no CCF was minted**.
Gate **15,869 / 15,862 passed / 1 skipped / 6 failed** across all 37 executables run individually,
the same two re-measured causes as before. Graph **41 / 91**, seams **2 / 18**, negative fixtures
**11 / 94**. Doxygen and `ccache` are absent from this container and were not run. The prior header
stack is retained below.*

*Prior header, retained:*

*Last verified: 2026-08-04 — branch `feature/remediation-batch-net-http-2063-2064-xml-review`, cut
from the clean tip `257106a` and **not pushed; no push was requested during this batch**. No merge,
rebase, tag, PR, force-push, amend or history rewrite; all new commits intentionally unsigned
(`git -c commit.gpgsign=false`), authored and committed as `Claude <noreply@anthropic.com>`. The
batch **closed the compatible `System::Net::Http` queue** with **#2063** and **#2064** in that
mandatory order, **reconciled the namespace**, then **re-derived the next review unit by
measurement** and performed the **`System::Xml` namespace review (#2073)** —
`docs/SystemXmlNamespaceReviewPlan.md`, 20 sections, the tenth in the #1950 series — and
implemented **three** of the seven compatible tickets it created: **#2074**, **#2075** and
**#2077**. **Six findings moved `confirmed → remediated`** — SR-AUD-311, SR-AUD-312 (#2064),
SR-AUD-313 (#2063), SR-AUD-350, SR-AUD-351 (#2074/#2075) and SR-AUD-353 (#2077) — and **two became
`confirmed (design-complete)`** (SR-AUD-314, SR-AUD-315). **Fourteen premises were corrected by
measurement**, five of which changed what shipped: SR-AUD-313 has **ten** public doors, not the
four the review scoped its ticket to, and one of the missed ones puts attacker text in the
**request line**; SR-AUD-349's validator **already exists** and one sibling already uses it;
SR-AUD-350's correct pattern was already one call away; SR-AUD-351 is **four** mutators failing in
three ways; and SR-AUD-353 was **pinned by a pre-existing test**, which is why it survived a
passing suite. **Two sanitizer premises were corrected by measurement and reported as
non-results** — UBSan's `enum` check does not see the `HttpStatusCode` cast, and the XML node
`InsertAfter` drops is an orphan rather than a leak. Audit **129 remediated / 235 confirmed / 364
total**, of which **49** carry `confirmed (design-complete)`; **no `SR-AUD-*` identifier was
created — numbering stays frozen at 364.** Gate **15,771 / 15,764 passed / 1 skipped / 6 failed**
across all 37 executables run individually, the same two re-measured causes as before. Graph
**41 / 91**, seams **2 / 18**, negative fixtures **11 / 94**. Doxygen and `ccache` are absent from
this container and were not run. The prior header stack is retained below.*

*Prior header, retained:*

*Last verified: 2026-08-04 — branch `feature/remediation-batch-buffers-2054-next-review`, cut from
`b294738` and **not pushed; no push was requested during this batch**. No merge, rebase, tag, PR,
force-push, amend or history rewrite; the pre-existing local-only commits were left untouched. All
new commits intentionally unsigned. The batch **closed the compatible `System::Buffers` queue**
with **#2054**, **re-derived the next review unit by measurement** instead of inheriting it,
performed the **`System::Net::Http` namespace review (#2062)** —
`docs/SystemNetHttpNamespaceReviewPlan.md`, 20 sections, the ninth in the #1950 series — and
implemented the first ticket that review created, **#2065**. **Four findings moved `confirmed →
remediated`**: SR-AUD-070 and SR-AUD-077 (#2054), and SR-AUD-318's **leak half** (#2065, a split
record; its limits half stays `confirmed` and blocked as #2071). **Ten premises were corrected by
measurement**, three of which changed what shipped — the load-bearing ones being that #2054's
requirements bite at **construction** rather than at the call, because the members are `virtual`
and are instantiated for the vtable; that SR-AUD-313's injection vector includes the **request
URI**, not only header text, so a header-only repair would have left the door open; and that
SR-AUD-318's socket clause, quantified, leaks **one descriptor per failing request** from four
remote-controlled paths, which promoted it from a clause in a medium finding to the batch's P1
repair. Audit **123 remediated / 241 confirmed / 364 total**, of which **47** carry `confirmed
(design-complete)`; **no `SR-AUD-*` identifier was created — numbering stays frozen at 364.** Gate
**15,706 / 15,699 passed / 1 skipped / 6 failed** across all 37 executables run individually, the
same two re-measured causes as before. Negative fixtures **10 / 81 → 11 / 94**. Doxygen and
`ccache` are absent from this container and were not run. The prior header stack is retained
below.*

---

## 2026-08-04 — `System::Net::WebSockets` closed for compatible work (#2089, #2091), the `modules/io` review (#2097), and #2101/#2103

Branch `feature/remediation-batch-websockets-2089-2091-io-review`, cut from the clean tip
`a4698e6`. Five unsigned commits, not pushed. No merge, rebase, tag, force-push, PR, publication,
amend or history rewrite.

**#2089 — SR-AUD-248 and SR-AUD-249, both `remediated`.** The load-bearing result is a **third
door the finding does not name**: the request line is built from `uri.getPathAndQueryProperty()`
and `Host:` from `uri.getHostProperty()`, and `System::Uri` preserves CR/LF/NUL in both, so a URI
alone smuggled `GET /admin HTTP/1.1` into the handshake. **The same door #2063 found missing from
SR-AUD-313's paraphrase, in a second namespace.** SR-AUD-249's premise needed correcting both ways:
a validator *did* exist and already rejected `<= 0x20` and `0x7F`, but **not one** of the
seventeen RFC 7230 separators. Rejection precedes any byte on the wire — 20 rejected connections,
`/proc/self/fd` delta **0**, all 20 throwing.

**One predicate body, and CCF-021 still not minted.** Rather than duplicating #2063's predicate or
making `Net.WebSockets` depend on the whole `Net.Http` component, the body moved to
`System::Net::detail::ContainsProtocolFieldTerminator` in the `Net` component both modules already
depend on, with the `Net.Http` name kept as a forwarder. No `Net.Http` call site, type or message
changed; graph unchanged at **41 / 91**. CCF-021 is **not** minted — `net-http-headers` holds two
of five findings, both `high`, and is unreviewed. Recorded so a future promotion cannot lose them:
**SR-AUD-249 is a token-grammar defect and NOT a member**, and the number has been proposed for
**two different** candidate families.

**#2091 — the frame payload.** Two close parsers existed, not one: `ReceiveAsync`'s and
`CloseAsync`'s, each with its own unvalidated `>= 2` block. §7.7's claim is corrected — close codes
3000–4999 are legal and have no enumerator in this port *or* .NET; what #2091 closes is codes that
can never be valid. A **fragmented** Text message is deliberately not UTF-8 validated and the
limit is **pinned**: a scalar may straddle a fragment boundary, and doing it right needs decoder
state on the object, an object-layout change this compatible ticket does not make.

**Namespace reconciliation found five unmet completion criteria** — §19 requires every blocked
finding to carry a pin, and only three existed. Five pins added. **#2096 is deliberately left
without a behavioural pin**, with the reason recorded: a data race and a null dereference are UB,
not behaviour.

**#2097 — the `modules/io` review**, selected by re-measurement with the objection stated: `io`
has **zero** high findings and loses priority 1 on the raw severity column. It wins on
**decidability** — its findings are self-evident contract violations, while `time-zone`'s and most
of `globalization`'s are "what does .NET do" questions that, with `/rv` absent, would yield only
deferred tickets. Ten bounded tickets; **nothing blocked on a user approval**. Four premise
corrections, including **SR-AUD-186's premise is inverted** (the port copies, .NET wraps — .NET's
is the aliasing behaviour) and **SR-AUD-342 is half already fixed**. **CCF-022's trigger is met and
its membership complete, and it is still NOT minted**: the cross-cutting document says in five
places that the numbering is closed, and resolving that contradiction belongs to the maintainer.

**#2101 / #2103.** #2101's real scope was **one door, not seven** — the neighbours a grep flagged
were already guarded, and are now pinned. #2103 routes `FileInfo::Delete` through `File::Delete`,
which **already had the guard**; it is the only finding in `modules/io` that destroys user data.

**Three checks that did not discriminate were caught and fixed**, including a **mutation that
failed to fail because the mutant itself was UB** and a **stale-binary false pass**. One IO test is
honestly recorded as non-discriminating.

**Audit 137 remediated / 227 confirmed / 364 total**, 49 `confirmed (design-complete)`. **No
`SR-AUD-*` identifier created — numbering frozen at 364.** Gate **15,925 across 37 executables:
15,918 passed, 1 skipped, 6 failed** (the same two re-measured causes). Graph **41 / 91**, seams
**2 / 18**, fixtures **11 / 94**. Selective components passed, ~11 min, peak **2** `cc1plus`.
Doxygen, `ccache` and `/rv` absent. **#1962 and #1773 remain blocked; CCF-012 and CCF-019 were not
marked closed; CCF-021 and CCF-022 were not minted.**

**#2107 raised, not silenced:** `local_ci_check` surfaced a descriptor-instrument failure with a
**negative** delta, hitting different tests under load. Proven **not** a regression from this batch
(zero commits touched those tests; the delta sign is impossible for a leak) and root-caused to
`descriptorDeltaOver` sampling before `serverThread.join()`.

## 2026-08-04 — `System::Xml` closed for compatible work (#2076, #2078, #2079, #2081), the `System::Net::WebSockets` review (#2087), and #2090

**Eight commits**, all local and unsigned, on `feature/remediation-batch-xml-compatible-websockets-review`
cut from `e1cc8c3`.

### `System::Xml` — the compatible queue, closed

| Ticket | Finding | What measurement added |
|---|---|---|
| **#2076** | SR-AUD-349 | **Four** name doors, not two — including `WriteProcessingInstruction`'s **target**, whose `"?>"` closed the instruction early and spilled into document text **while the writer already sanitised the PI *data***. All twelve `Write*` members also stayed callable after `Close()`. All four route through `XmlConvert::VerifyName`, which the module already shipped and one sibling already used. |
| **#2078** | SR-AUD-348 | **Three** members destroyed the closed state. The class **already implemented one terminal read state correctly** (`EndOfFile`), so `Closed` became the same shape: no new field, no invented values, no new exception identity. |
| **#2079** | SR-AUD-352 | **Sixteen** silent doors, not three. `RemoveAllChildren` stays silent **on purpose** — it destroys its children, so a `NodeRemoved` handler's `XmlNode*` would name freed storage. The CCF-019 shape was **refused rather than introduced**, pinned by a test, tracked as #2086. |
| **#2081** | SR-AUD-355 | **Four** coupled symptoms. The repair was **already named in the file** by a 17-line `KNOWN GAP` comment citing .NET's `ValueText`/`CalibrateText`; that comment was **replaced**, not left standing. |

**Reconciliation (§21 of the XML plan).** Seven of eight findings remediated; SR-AUD-354 deferred
**and now pinned**. The reconciliation found the queue had left **three completion criteria
unmet** — SR-AUD-354's pin, the two post-audit acceptance pins, and §10's `sizeof` probe structs,
which §10 assigns to *"the first implementation ticket that lands"*. `XmlContractPinTests` closes
all three: layout by **probe struct, never literal byte counts**, the deferred behaviours, and the
review's two **security positives** (entities inert, depth bounded) now *checked* rather than
asserted in prose. `SharpRuntimeTests_Xml` **400 → 483**, add-only.

Three post-audit tickets created and **not** absorbed: **#2084** (DOCTYPE quoted-literal
injection), **#2085** (an embedded NUL silently truncates writer content at three doors),
**#2086** (`RemoveAllChildren`'s event pair). Each is blocked on a **stated decision**, not effort.

### `System::Net::WebSockets` — review #2087, and #2090

Selected by re-measuring the index: **6 open findings, 2 high, the highest high-severity ratio
(33%) of any unreviewed unit with ≥6 open findings**, a remotely-driven frame parser, and the
smallest coherent component in the candidate set. The `System::Xml` review's reason for skipping
it — *"its highest-value finding is blocked on arrival"* — is **explicitly overruled**: true of
SR-AUD-247, not true of the unit.

**Corrected premise 6.5 is the batch's largest finding: the frame parser is entirely unaudited.**
Eleven post-audit protocol defects (plan §7). **#2090 repaired the first five** — reserved bits,
the opcode domain, masked server frames, fragmented control frames, and oversized control
payloads. The size check sits on the **7-bit length field**, so the 256 MiB Ping is a two-byte
rejection with no allocation; the mask-key read and unmasking loop were **deleted**, not left dead.

**No CCF was minted**, each refusal with a reason: CCF-019 is this module's **sixth** site and
#2066's family design still has two options and no selection, so #2088 records the **second
borrowed edge** (`SendAsync`/`ReceiveAsync` capture the **caller's buffer by reference**) and pins
the ownership model with a `static_assert`; CCF-021's evidence is now complete across three
modules but `net-http-headers` holds two of the four and is unreviewed, so #2089 is instead
**required to reuse `System::Net::Http`'s predicate shape rather than invent a second one**;
CCF-022 unchanged.

### Four non-discriminating checks caught, including a stale-binary false pass

#2076's M7 reported the **previous binary's** result after a suppressed `-Werror` build failure;
#2079's E3 was non-discriminating **twice**, behaviourally and under ASan, until the handler read
a capture after reassigning its own field; #2081's X5 used a run whose members mapped to the same
node type; #2090's W5 used a truncated frame that threw for the wrong reason. Each is recorded —
every one would otherwise have justified deleting a real guard.

### Validation

Gate **15,869 tests / 15,862 passed / 1 skipped / 6 failed** across all 37 executables run
individually. The six failures are the two known causes, **re-measured**: `ping_group_range` is
`1 0` (an empty range) so `SOCK_DGRAM`/`IPPROTO_ICMP` fails `EACCES` while `SOCK_RAW` opens —
**#1962 exactly** — and `/proc/net/if_inet6` is **absent**. Boundaries **41/91**, catalogue
current, seams **2/18**, negative fixtures **11/94** (36.7 s, peak 2 jobs), `git diff --check`
clean. Build clean: **0 errors, 0 warnings**. Maximum compiler concurrency **two jobs**
everywhere. Doxygen, `ccache` and `/rv/tmp/runtime/` all **absent**; tracked
`scripts/__pycache__` files **byte-identical**.

**Remaining:** `System::Net::WebSockets` has **#2089** then **#2091** compatible; #2088, #2092,
#2093, #2094, #2096 blocked; #2095 deferred. **`modules/io` is the recommended next unit** — it
unlocks CCF-022. **#1962 and #1773 remain blocked.**

## 2026-08-04 — `System::Net::Http` closed for compatible work (#2063, #2064), the `System::Xml` review (#2073), and #2074/#2075/#2077

**#2063 — a control character crossed TEN public doors into an HTTP or MIME frame.** SR-AUD-313
now `remediated`, SR-AUD-316's reason half with it. The namespace review scoped this ticket to
four doors; measured against `257106a`, **ten** were open, and two of the six the review missed
are the load-bearing ones. The **request path** reaches the request **LINE** —
`parseUrl("http://host/pa\r\nX: y")` returned path `"/pa\r\nX: y"` and the handler writes
`method << " " << path << " HTTP/1.1\r\n"` — so that is request smuggling, not one extra header
field. And the **media type, charset, multipart subtype and form-data name/file name** doors that
SR-AUD-313's *own audit text* names were dropped by the review's paraphrase and were all still
open: `MultipartFormDataContent::Add(c, "na\r\nX-Injected: yes")` emitted a separately parsed
`Content-Disposition` field, reproducing the audit's multipart probe exactly. Closing only the
four scoped doors would have marked the finding remediated with three of its own named vectors
open. One shared `System::Net::Http::detail` helper now guards all ten, plus two internal sites
with no public door in front of them; `parseUrl` validates the **whole URL string** once, before
any splitting, which also makes #2064's re-split safe by construction. `System::FormatException`
for the protocol-field doors, `System::UriFormatException` (which **is** a `FormatException`) for
`parseUrl`, `HttpRequestException` for `parseStatusLine` and the handler's response-header check,
`System::ArgumentException` for the two multipart doors — all recorded as **this port's choices**,
since `/rv/tmp/runtime/` is absent. Rejected text is deliberately **not** echoed into the message.
+28 tests; nine mutations; ASan/UBSan/LSan clean with the four changed `.cpp` bodies compiled from
source and a control heap-buffer-overflow proving instrumentation.
`docs/Migration-HttpControlCharacterRejection.md`.

**#2064 — `std::sto*` accepted a valid prefix and called it the whole value.** SR-AUD-311 and
SR-AUD-312 now `remediated`. One full-consumption parser (the entire text must be ASCII digits and
the value must fall inside the domain) replaces `std::stoi` in both public static parsers —
CCF-002's remedy shape reduced to what this module needs, citing the family rather than minting
one. The **authority now ends at the first `/`, `?` or `#`**, which closes the finding's worst
row: `http://host?q=1` used to parse to host `host?q=1` with path `/`, so a query string reached
**DNS and the `Host:` header** while the request line asked for `/`. The **host is lowercased**, as
the scheme already was. The **version token was never parsed at all** — `GARBAGE 200 OK` yielded
200 — so there was nothing to tighten and a check to add. `HTTP/1.1 099 OK` is accepted as the
code 99 and `HTTP/9.9` is accepted; both are **pinned as this port's choices**, not claimed as
matches. **Not one existing test needed updating.** Seven mutations; the sharpest is that making
#2063's control check validate only the parsed *host* fails four tests, which is what proves the
two repairs are **ordered** rather than merely co-present.

**`System::Net::Http` is closed for compatible work.** Nine findings own the namespace: three
`remediated`, two **split** (half remediated, half blocked), two `confirmed (design-complete)`
with blocked tickets, one `confirmed` and blocked, one deferred. Every gated behaviour is pinned
and every pin is mutation-checked. **§17's completion criterion 3 was corrected**: SR-AUD-310 is
deliberately **not** marked design-complete, because the review records two competing options with
no selection — marking it so would overstate what exists.

**#2073 — the `System::Xml` namespace review.** Selection re-derived by measurement.
`modules/net-websockets` wins priority 1 **on the letter** (SR-AUD-247 is an ASan-confirmed UAF)
and was **not** selected: that finding is CCF-019 verbatim and #2066's disposition is now known —
blocked, unapproved, two options, no selection — so its highest-value work is blocked on arrival.
`modules/xml`'s two highs are both actionable. Eight findings, six families, ten tickets, **seven
compatible and three deferred**, and **nothing in the namespace needs a layout, vtable, base-class
or public-type change**. The public-input sweep found **two genuine security positives worth
recording as non-findings** — internal entities are never expanded, and nesting depth is bounded
by the substrate — and two post-audit acceptance defects, both deferred for want of evidence.

**#2074 / #2075 / #2077 — three of the seven compatible XML tickets.** `InnerXml` replacement is
atomic (the destructive step used to run before the parse that could fail, so an invalid fragment
emptied the node **with no exception**); four DOM mutators reject a node owned by another parent
(the finding named one, and `InsertAfter` left the new node **nowhere**); and `HasNamespace`
searches every active scope, as its own documented contract and its own sibling already did. The
ASan question the review refused to answer by reading **is answered and is a non-result**: the
dropped node is an orphan freed with the document, so the defect is the silent acceptance and the
behavioural test is the closure evidence. SR-AUD-353 **was pinned by a pre-existing test**, which
is why it survived; that test was corrected in place and renamed, not deleted.

**Remaining, unchanged by this batch:** #2066–#2069 and #2071 blocked; #2070, #2072, #2080, #2082
and #2083 deferred; #2076, #2078, #2079 and #2081 compatible and ready in `System::Xml`.
**#1962 and #1773 remain blocked. CCF-012 and CCF-019 were not marked closed; CCF-021 and CCF-022
were not minted.**

## 2026-08-04 — `System::Buffers` closed for compatible work (#2054), the `System::Net::Http` review (#2062), and #2065

**#2054 — six public generic surfaces required more of `T` than they documented.** SR-AUD-070 and
SR-AUD-077, family B-C, both now `remediated`. Each requirement is stated in the owning type's
Doxygen block and `static_assert`ed at the point where it was already enforced — never at class
scope, which would have rejected a mere declaration that compiles today. Measured against the
pre-change headers materialised from `b294738`: **exactly the same set of programs compiles**, and
only the diagnostic changes. Three corrected premises: there are **six** production sites, not
four (`ArrayBufferWriter(intcs)` resizes on its own and was missed by the finding *and* by the
review plan); the requirement bites at **construction**, because `GetSpan`/`GetMemory` and
`ArrayPool`'s `Rent`/`Return` are `virtual` and instantiated for the vtable; and
**copy-assignability** was a second undocumented requirement that nothing named. 13 negative
consumer sites hold the rejected half, 9 tests the accepted half. `SharpRuntimeTests_Buffers`
**609 → 618**. No runtime code, signature, layout, vtable or `noexcept` change.

**`System::Buffers` is closed for compatible work.** Nineteen findings own the namespace; each has
exactly one disposition — 13 `remediated`, 4 `confirmed (design-complete)` with a blocked ticket
and a pin each (#2056–#2059), 1 deferred verification with a pin (#2060), and SR-AUD-081 left
`confirmed` only because the status vocabulary has no false-positive value. #2056–#2059 were not
implemented and not touched; #2060 stays deferred because `/rv/tmp/runtime/` was re-verified
**absent** on 2026-08-04.

**#2062 — the `System::Net::Http` namespace review.** Selected by measurement over every unit with
≥6 open findings and no durable review: `modules/net-http` won on priority 1 (SR-AUD-310 is an
ASan-confirmed use-after-free) and priority 2 (its inputs are remote-attacker-controlled).
`modules/core` was excluded on **coherence, not count**. Nine findings, one disposition each, six
root-cause families, nine tickets — three compatible (#2063, #2064, #2065), five blocked
(#2066 cites CCF-019 and does not close it; #2067, #2068, #2069, #2071), one deferred for absent
evidence (#2070). Seven corrected premises, all measured. **No CCF was minted**: NH-B's
control-character shape is open in two further modules, but both are unreviewed, so §18 records
the promotion rule for CCF-021 instead.

**#2065 — one socket descriptor leaked per failing request.** `connectToHost` returned a bare
descriptor closed at exactly one point, after the whole body had been read, so every throw in
between leaked it — and all four of those throws are chosen by the **remote peer**. Measured: 20
requests, 20 leaked descriptors, in each of four modes; 0 on the success path. A server answering
~1,024 requests with a garbled status line exhausts a default `RLIMIT_NOFILE`. A file-local
`SocketGuard` now owns it, keeping the original close point on the success path so the mutation
check stays sharp: emptying the destructor fails exactly the four failure-path tests with the
pre-repair count of 19 while the success-path test stays green. **LSan does not cover this** — it
tracks memory, not descriptors — so the `/proc/self/fd` count is the instrument and no clean
sanitizer run is substituted for it; the tests **skip** where that path does not exist. SR-AUD-318
now carries a split record.

**Next:** #2063 (must precede #2064 — both touch `parseUrl`, and the pins must exist first), then
#2064, then `modules/xml` as the next review unit.

*Last verified: 2026-08-04 — branch `feature/remediation-batch-buffers-review`, cut from
`27061bf` and **not pushed; no push was requested during this batch**. No merge, rebase, tag, PR,
force-push, amend or history rewrite; the pre-existing local-only commits were left untouched. All
new commits intentionally unsigned. The batch performed the **`System::Buffers` namespace review
(#2048)** — `docs/BuffersNamespaceReviewPlan.md`, 23 sections, the eighth in the #1950 series —
and then implemented **seven of its eight compatible tickets**: **#2049, #2050, #2051, #2052,
#2053, #2055** plus the disclosure-and-pins ticket **#2061**. **Four findings moved
`confirmed → remediated`** (SR-AUD-072, 073, 076, 083) and **four became `confirmed
(design-complete)`** (SR-AUD-071, 074, 087, 088). **Eight premises were corrected by
measurement**, four of which changed what shipped — the load-bearing one being that SR-AUD-073's
out-of-slice read needs **no forged `SequencePosition`** at all, since an ordinary
`getStartProperty()` held across a `Slice` produces it, which is why the repair validates the
position's **range** rather than the segment marker the audit proposed. **Two post-audit defects
were found and repaired with ordinary ticket numbers**: a reachable UBSan-confirmed signed
overflow in `ArrayBufferWriter`'s growth arithmetic that the audit had filed merely as untested,
and a Θ(n²) `BuffersExtensions::PositionOf` allocating **384 MB** to search 32 KB, which the audit
never mentioned. **The `System::Buffers` namespace is wider than `modules/buffers`: 12 open
findings, not 11** — SR-AUD-088 owns `System::Buffers::MemoryHandle`, indexed under `modules/core`
by path. **No approval was requested, implied or assumed; #2056/#2057/#2058/#2059 are blocked and
all behaviour-pinned; #2060 is a deferred verification; #1962 and #1773 remain blocked; CCF-012
was NOT marked closed.** Audit **121 remediated / 243 confirmed / 364 total**, of which **47** are
design-complete; **no `SR-AUD-*` identifier was created — numbering stays frozen at 364.** Gate
**15,692 / 15,685 passed / 1 skipped / 6 failed** across all 37 executables run individually, the
same two measured causes as before. The prior header stack is retained below.*

*Last verified: 2026-08-04 — branch `feature/remediation-batch-system-net-compatible`, cut from
`4777f95` and **not pushed; no push was requested during this batch**. No merge, rebase, tag, PR,
force-push, amend or history rewrite; the pre-existing local-only commits were left untouched. All
new commits intentionally unsigned. The batch implemented the **whole compatible `System::Net`
queue** created by review #2034 — **#2041, #2035, #2036, #2037, #2038, #2039** in the plan's own
§13 dependency order — plus the mandatory **disclosure-and-pins ticket #2047**. **Five findings
moved `confirmed → remediated`** (SR-AUD-300, 301, 302, 303, 307) and **SR-AUD-304 became
`confirmed (design-complete)`** with three of its four halves repaired and the wildcard half left
to the gated #2043. **Ten premises were corrected by measurement**, the load-bearing one being
that SR-AUD-304's duplicate results do **not** come from the duplicate `sscanf` parser the review
blamed but from `hints.ai_socktype = 0`, which tripled **every** resolved name as well — so
repairing the parser alone would have fixed the one named input *by accident*. Two further
corrections mattered enough to change the code that shipped: each scope-id door reports its **own**
`paramName`, and a family-mismatched `Dns` literal **throws** rather than returning empty, because
two pre-existing tests already pin that as this repository's reasoned contract. **Two new ordinary
inactive tickets, #2045 and #2046**, carry defects deliberately *not* absorbed. **No approval was
requested, implied or assumed; #2040/#2042/#2043/#2044 remain blocked and are now all
behaviour-pinned; #1962 and #1773 remain blocked; CCF-012 was NOT marked closed.** Audit **117
remediated / 247 confirmed / 364 total**, `confirmed (design-complete)` **42 → 43**; **no
`SR-AUD-*` identifier was created — numbering stays frozen at 364.** `SharpRuntimeTests_Net`
**240 → 324**, add-only. Gate **15,619 tests across 37 executables, 15,612 passing, 1 skipped, 6
failing** for the same two measured causes, unchanged and not hidden. **Doxygen NOT run — not
installed here; `ccache` also not installed.** CNA and mobile-eggbert were not inspected. Maximum
aggregate parallelism **2 jobs**.*

*Last verified: 2026-08-04 — branch `feature/remediation-batch-approval-packages-next-review`, cut
from `1b892f6` and **not pushed; no push was requested during this batch**. No merge, rebase, tag,
PR, force-push, amend or history rewrite; the pre-existing local-only commits were left untouched.
All new commits intentionally unsigned. The batch **verified and consolidated both outstanding
approval packages into one request**, `docs/ConsolidatedApprovalPackage.md`: three
`System::Diagnostics` decisions (with **#2032 folded into #2029** — the reader-thread join lives in
`reapIfNeeded` and is reached from **five** public doors, so §14.1's destructor-only sentence would
have left four blocking with no bound, and its recommended option C is **unsound as written**
because detaching a reader that holds a raw pointer into the `Impl` is a use-after-free) and nine
`System::Text` decisions (verified and standing, with #2017 gaining **six** alternatives and a
changed recommendation, CCF-012 re-enumerated across all 16 brace-scanning files, and A1+A2 made
atomic). **#2030's two pins and #2031's one were independently re-verified discriminating** by
applying the proposed change temporarily and reverting it. It split and implemented the **one**
genuinely compatible portion, **#2033** (the reader-join disclosure half: six header contracts made
true, +6 mutation-checked tests, zero executable production change), backfilled the three missing
`plan.md` sections and corrected the previous batch's record-keeping note, and then re-derived the
next namespace by measurement and performed the **`System::Net` review (#2034)**, whose ten premises
**all reproduced** and four of whose audit premises were **corrected**. **No approval was requested,
implied or assumed, and no gated work was implemented; CCF-012 was NOT marked closed.** Audit
**112 remediated / 252 confirmed / 364 total**, `confirmed (design-complete)` **38 → 42**; **no
`SR-AUD-*` identifier was created.** `SharpRuntimeTests_Diagnostics` **219 → 225**, add-only.
**Doxygen NOT run — not installed here; `ccache` also not installed.** **#1773 remains blocked and
its downstream use was not investigated.** CNA and mobile-eggbert were not inspected. Maximum
aggregate parallelism **2 jobs**.*

*Last verified: 2026-08-03 — branch `feature/remediation-batch-text-approvals-next-review`, cut
from `80c804b` and **not pushed; no push was requested during this batch**. No merge, rebase,
tag, PR, force-push or history rewrite; the pre-existing local-only commits were left untouched.
Four new commits, `017d336`, `6abd16e`, `8114518` plus this handoff commit, all intentionally
unsigned. The batch **verified and consolidated the `System::Text` approval package for
#2013–#2021** into `docs/SystemTextApprovalPackage.md` — six families, one exact approval
sentence each — correcting **ten premises** in the review plan's §14 that did not survive
re-measurement; split and implemented the **one** genuinely compatible portion, **#2022**
(test-only: **SR-AUD-294 and SR-AUD-299 had no behaviour-pinning test at all**, so #2018 and
#2021 could have landed unapproved); verified **CCF-012**'s closure scope **without marking it
closed**; and then re-derived the next namespace by measurement and performed the
**`System::Diagnostics` review (#2023)**, whose eight premises **all reproduced**. **No approval
was requested, implied or assumed, and no gated work was implemented.** Audit stays **107
remediated / 257 confirmed / 364 total** with `confirmed (design-complete)` **35 → 38**; **no
`SR-AUD-*` identifier was created.** `SharpRuntimeTests_Text` **288 → 296**, add-only, with no
production change. Tickets **2,030: 1,974 done, 9 todo, 40 blocked, 3 needs_user, 4 wontfix**,
none doing. **Doxygen NOT run — not installed here.** **#1773 remains blocked and its downstream
use was not investigated.** CNA and mobile-eggbert were not inspected. Maximum aggregate
parallelism **2 jobs**.*

*Last verified: 2026-08-03 — branch `feature/remediation-batch-system-text-review`, cut from
`bef4e43` and **not pushed; no push was requested during this batch**. No merge, rebase, tag,
PR, force-push or history rewrite; the three pre-existing local-only commits were left
untouched. Seven new commits, `67cbfa7` … `4f459de` plus this handoff commit, all intentionally unsigned. The batch
performed the **`System::Text` namespace review (#2006)** —
`docs/SystemTextNamespaceReviewPlan.md`, converting the 14 open findings in `modules/text/`
(SR-AUD-286 … SR-AUD-299) into **eleven root causes T-A … T-N** and tickets **#2006–#2021** —
and then implemented its **entire compatible half**: **#2007** SR-AUD-286 (one shared raw
decode argument policy, transcribed from `UTF7Encoding`, adopted by all nine decode entries
plus `Decoder`, `GetCharCount` and `DecoderExceptionFallback`), **#2008** SR-AUD-287 (both
fallback setters reject null — the finding names only the decoder direction), **#2009**
SR-AUD-295 (`StringBuilder::CopyTo`'s unvalidated signed capacity, CCF-004's fourth module),
**#2010** SR-AUD-298's diagnostics half (CCF-012), **#2011** SR-AUD-297's diagnostics half,
**#2012** SR-AUD-290/296/289's disclosure half. **Nine causes remain approval-gated**
(**#2013–#2021**), each with a complete design and an exact approval sentence in the plan's
§14, each with its current behaviour pinned by a permanent test so it cannot land silently;
**no approval was requested, implied or assumed, and none was implemented.**

The namespace was chosen from tracked state: `NEXT.md` §10 of the previous handoff names it,
the audit index confirms the fourteen are contiguous and all `confirmed`, and **no `docs/`
document covered it** — `docs/TextSubsetCompatibilityDecision.md` is *not* one despite its
name, being the #1927/#1928/#1929 numeric and date/time packet whose §1 puts `Encoding`
explicitly out of scope. Scope was narrowed by measurement, not assumption: the C++ namespace
spans three CMake components and this review owns **one** (`Text`), while
`System::Text::NormalizationForm` lives in `modules/core` with **no `Normalize` surface at
all**, and `Encoder`/`Decoder` have **no incremental conversion surface**, so normalization
and streaming state are *absent features* recorded as explicit exclusions rather than
invented as findings.

**Seven audit premises were corrected**, historical text preserved: SR-AUD-286's named
reproduction does **not** reproduce and it names one of **six** failure modes; SR-AUD-287
names one of two directions; SR-AUD-295's overflow **defeats** the bounds check rather than
merely accompanying it, so an invalid capacity became a **silent write** (ASan-confirmed,
call returning normally); SR-AUD-298's same expression also returned a **negative** minimum
argument count; SR-AUD-297's `Decode` defect is **four** defects, three of them silent wrong
bytes; and **CCF-012's exclusion list is wrong to say `System.Text.CompositeFormat` "is not
ported"** — it is, it is a third composite-format grammar, and **CCF-012 still cannot close**
until #2020 lands. Ten post-audit defects were recorded under ordinary ticket numbers only;
audit numbering stays frozen at **364**.

Audit is now **107 remediated / 257 confirmed / 364 total**, **35** of the 257 carrying the
`confirmed (design-complete)` qualifier. The gate is **15,461 tests / 37 executables** run
individually, 15,454 passing, 1 skipped, **6 failing for the same two causes, re-measured,
none hidden and none new** — `ping_group_range` is `1 0` so unprivileged `SOCK_DGRAM` ICMP is
denied while `SOCK_RAW` ICMP **opens**, which is **#1962** rather than the environment alone;
and `/proc/net/if_inet6` does not exist. `SharpRuntimeTests_Text` **238 → 288**, add-only.
Sanitizers, with the changed production bodies compiled **into** the probes: five ASan
heap-buffer-overflows, three UBSan signed overflows and **35 escaped `std::` exceptions**
before; **none** after; TSan **not applicable** to the compatible batch and stated as such.
Graph **41/91**; seams **2/18**; negative fixtures **10/81**, 91 invocations, peak 2; checker
self-tests **45/45** and **15/15**; module-boundary self-tests **7/7**; selective components
**passed**; build **0 warnings / 0 errors**; `git diff --check` clean. **Doxygen was NOT run:
doxygen is not installed in this container.** Tickets: **1,972 done, 4 todo, 37 blocked, 3
needs_user, 4 wontfix** of 2,020; none doing. **#1773 remains blocked and its downstream use
was not investigated; #1995–#1999, #2003 remain blocked and #2005 remains deferred; no
approval was requested or assumed.** CNA and mobile-eggbert were not inspected. Maximum
aggregate parallelism 2 jobs.*

*Last verified: 2026-08-03 — branch `feature/remediation-batch-uri-followup-2000`, cut from
`948f93a` and **not pushed; no push was requested during this batch**. No merge, rebase, tag,
PR, force-push or history rewrite; the two pre-existing local reconciliation commits were left
untouched. Two new commits, `4280903` and `942f27a`, both intentionally unsigned. The batch
worked the **`System::Uri` post-audit follow-up queue #2000–#2005** created by #1987's review.
**Implemented:** **#2000** — an empty authority is rejected wherever a port applies, so
`http://`, `http:///path`, `http://:80/path`, `http://user@/path`, `file://:8080/path` and
`mailto:///c` now throw `UriFormatException` while **`file:///path` stays accepted** (`Port`
is `-1`, so nothing is fabricated) along with every hierarchical scheme that has no
default-port entry; **#2004** — `UriBuilder::GetHashCode` hashes the rendered string instead
of parsing it, removing four measured routes on which `Equals(self)` succeeded but
`GetHashCode` threw, with **no returned value changed** and **no part of #1995's gated identity
policy introduced**; **#2001** — resolution against an opaque base follows RFC 3986 §5.2.2/§5.3
with an undefined authority (`mailto:a@b.com` + `c` → `mailto:c`) instead of fabricating one,
using opacity **derived from members the object already has**, with no layout change;
**#2002** — a relative reference now splits its query and fragment exactly as the other two
branches of the same function always have. **#2003** (embedded NUL) is design-complete and
**blocked** on approval to narrow, its preserve-as-data behaviour pinned by tests after
measurement ruled out any prefix-only parse or truncation; **#2005** (whitespace) stays
**deferred**, its missing reference evidence re-verified absent. The audit is **unchanged at
104 remediated / 260 confirmed / 364 total**, 24 of the 260 carrying the
`confirmed (design-complete)` qualifier; **no finding status moved and no `SR-AUD-*`
identifier was created — numbering stays frozen at 364.** The gate is **15,411 tests / 37
executables**, 15,404 passing, 1 skipped, **6 failing for the same two causes, re-measured,
none hidden and none new** (five `PingTests` → #1962; one `SocketTests` resolver case).
`SharpRuntimeTests_Uri` **213 → 272**. Graph **41/91**; seams **2/18**; negative fixtures
**10/81**, 91 invocations, peak 2; checker self-tests **45/45** and **15/15**; selective
components **passed**; build **0 warnings / 0 errors**; `git diff --check` clean. **Doxygen
was NOT run: doxygen is not installed in this container.** Tickets: **1,965 done, 4 todo, 28
blocked, 3 needs_user, 4 wontfix** of 2,004; none doing. **#1773 remains blocked and its
downstream use was not investigated; #1995–#1999 remain blocked and no approval was requested
or assumed.** CNA and mobile-eggbert were not inspected. Maximum aggregate parallelism 2 jobs.*

*Last verified: 2026-08-03 — branch `feature/remediation-batch-system-runtime-review`,
cut from `66ff8b3` and **not pushed; no push was requested during this batch**. No merge,
rebase, tag or PR. The batch performed the **`System::Runtime` namespace review (#1972)**
— `docs/SystemRuntimeNamespaceReviewPlan.md`, converting the 21 open findings in
`modules/runtime/` into **twelve root causes** and tickets **#1972–#1986** — and then
implemented its **entire compatible half**: **#1973** SR-AUD-155, **#1974** SR-AUD-172,
**#1975** SR-AUD-169, **#1976** SR-AUD-156, **#1977** SR-AUD-170, **#1978** SR-AUD-059 plus
SR-AUD-168's disclosure half, **#1982** SR-AUD-162. Eight of the twelve causes are closed;
the four that remain are three approval-gated (**#1979**, **#1980**, **#1981**, each with a
complete design and an exact approval sentence) and one deferred verification (**#1983**).
The namespace was chosen over `System::Uri` on **severity, not count** — three high-severity
findings against `uri`'s zero — and `docs/ThreadingNamespaceReviewPlan.md` §1's dismissal of
it as *"dominated by reflection and serialization surfaces"* is corrected as that document's
§22: measured, **0** of 21 are reflection findings, **0** are serialization findings, and
**1** falls under a permanent deviation. Audit is now **100 remediated / 264 confirmed / 364
total**, numbering frozen at **364** — no `SR-AUD-*` was issued, including for the two new
post-audit defects **#1985** (self-pipe descriptors survive `exec()`) and **#1986** (a
handler can be invoked after `Dispose()` returns). Fourteen of the 264 now carry the
`confirmed (design-complete)` qualifier. The gate is **15,288 tests / 37 executables**,
15,281 passing, 1 skipped, **6 failing for the same two causes, re-measured, none hidden and
none new** — and the attribution is **sharper**: `SOCK_RAW`/`IPPROTO_ICMP` **succeeds** in
this container, so a working ICMP path exists and `Ping` cannot use it, which is **#1962**
rather than the environment alone. Graph **41/91**; seams **2/18**; negative fixtures
**10/81**, 91 invocations, peak 2; checker self-tests **45/45** and **15/15**; selective
components **passed**; build **0 warnings / 0 errors**; `git diff --check` clean. **Doxygen
was NOT run: doxygen is not installed in this container**, so the 1,942 ceiling stays
historical. Tickets: **1,953 done, 3 todo, 22 blocked, 3 needs_user, 4 wontfix** of 1,985;
none doing. #1773, #1962 and #1963 remain as they were; CNA and mobile-eggbert were not
inspected. Commits are intentionally unsigned.*

*Prior plan snapshot, retained historically: 2026-08-03 — branch
`feature/remediation-batch-tasks-channels-1965-1968`, cut from `a0cd647` and **pushed to
`origin/claude/remediation-batch-1804-namespace-b1yjh5` at the user's explicit mid-batch
request**; no merge, rebase, tag or PR. That batch implemented the entire compatible half of
the `Threading.Tasks` + `Threading.Channels` review (#1965–#1968) and split the
verified-compatible part of #1958 Group A into #1971. Audit was **93 remediated / 271
confirmed / 364 total**; the gate **15,253 tests / 37 executables**, 15,246 passing, 1
skipped, 6 failing. Its full record is the "Batch 2026-08-03 — `Threading.Tasks` +
`Threading.Channels`" section below.*

*Prior plan snapshot, retained historically: 2026-08-03 — branch
`claude/remediation-batch-1804-namespace-b1yjh5`, no upstream. **#1804 was
already `done`** (resolved 2026-07-30) and was re-verified, not redone. The batch's
real work was the **`System::Threading` namespace review (#1950)** plus four compatible
implementations and three defects found while establishing the baseline. Audit was
**72 remediated / 292 open / 364 total**; the gate **15,105 tests / 37 executables**.*

*Prior plan snapshot, retained historically: 2026-08-02 — branch
`feature/remediation-batch-1929-row4-design`, no upstream. Design-only #1938 is
complete; #1929 remains partial/`needs_user`. Rows 1–3 are remeasured and
unchanged, and row 4 is split into independent #1939–#1945 contracts with
#1939 the recommended next approval. No production/header/test semantic change
was made. #1936 remains complete; #1937 remains done/not reproducible;
#1934/#1925 and #1932/#1935 remain done; #1926 remains wontfix; #1773 remains
blocked. The clean socket-enabled gate is **15,092 tests / 37 executables**
(Integration 893, Core.Base 5,585, Collections.Core 2,763); audit **68
remediated / 296 open / 364 total**; graph **41/91**; seams **2/18**; negative
fixtures **10/81**, 91 compiler invocations, peak 2; checker self-tests
**45/45**; Doxygen **1,938/1,942**. Tickets: **1,927 done, 0 todo, 11 blocked,
3 needs_user, 4 wontfix** of 1,945; none doing. #1888/#1889/#1896 remain
declined/blocked and #1894/#1899 remain blocked.*

*Previous plan snapshot, retained historically: 2026-08-01 — 41 physical components, 91 direct production
dependency edges, a clean zero-warning native build, 15,058 passing tests
across 37 executables, and a green two-job selective matrix. The 2026-08-01
**#1932/#1933 design, evidence, and remaining-decision batch** made no
production/header or semantic change: #1932 now has a complete constructor-
specific Option 2R design and remains `todo`; #1933 is `done`, evidence-only,
with no stable whole-surface optimization; #1925 is classified `needs_user`
beside new inactive #1934; and `docs/RemainingApprovalDecisions.md` is the
superseding five-group packet. A corrected #1899 premise now records that a C++
visitor callback can retain its observer, so D/G are migration/diagnostic aids,
not lifetime enforcement. The audit remains **68 remediated / 296 open / 364**,
the graph **41/91**, seams **2/18**, negative fixtures **10/74**, and Doxygen
**1,937/1,942**. The preceding 2026-08-01
**approved text-subset + compatible P3 batch** delivered exact
`docs/TextSubsetCompatibilityDecision.md` §6.5 items (1), (2), and (3) as
independent #1927/#1928/#1929-row-5–6 commits, then completed compatible #1880
and #1875. #1929 remains partial: rows 1, the beyond-seven-digit remainder of
row 2, row 3, and row 4 are unapproved and unchanged. CCF-002 is closed and
SR-AUD-157 is remediated, taking the tally to **68 remediated / 296 open / 364
total**. Inactive #1930/#1931 record inseparable completed corrections;
#1932 retains the separable approval-bound HResult work, while #1933 is now
evidence-complete with no TimeOnly optimization.
No new audit identifier was issued. The 2026-07-31
**Group E subset batch** landed the approved **#1897 option B**: `JsonNode::Parse`
builds its tree iteratively, so the one CCF-019 case reachable from **untrusted
input** — an ASan stack overflow between 16,000 and 18,000 nested levels — is
closed at zero compatibility cost, and CCF-019 now has **no stack-overflow case
left**. It deliberately did **not** take option A (a depth bound), so
`JsonNode::Parse` still accepts text .NET rejects; that deviation is documented
and pinned. Before it, the 2026-07-31
**approved Groups A–D batch** (#1854, #1862, #1858, #1865, #1879, #1884, #1863)
delivered every ticket the decision packet
`docs/RemainingApprovalDecisions.md` put up for approval in those four groups,
moved **seven findings to `remediated`** (SR-AUD-007, 009, 015, 029, 033, 035,
043) and **closed three families outright — CCF-005, CCF-007 and CCF-012** —
taking the tally to **67 remediated / 297 open of 364**. It is
**behaviour-incompatible by design in four places**: `Decimal::Parse` reads `,`
as a group separator (`docs/Migration-DecimalCommaGroupSeparator.md`), the four
date/time parsers reject text they used to accept, `String::Format` adopts
.NET's brace and alignment grammar, and `Single`/`Double`
`ToString(value, format)` emit different `E`/`N`/`G` text. Eleven premises of
the packet and its plans were corrected by measurement, two of them cases where
the packet's stated .NET behaviour is wrong; three inactive tickets (#1927,
#1928, #1929) were filed and **no `SR-AUD-*` identifier was issued**. Before it,
the #1919 batch (#1921–#1924) delivered the approval-blocked half of #1912 and
**closed the family**: every ordered container (6 of 6) and every hashed
container (11 of 11) now carries the default comparison contract, and the floor
rose 14,890 → 14,920. Its two follow-ups, #1925 and #1926, are `todo` and are
not members of #1912's population. Every remaining approval question is
consolidated in `docs/RemainingApprovalDecisions.md`. The 2026-07-31
#1912 batch (#1913–#1918, #1920) before it carried the **default comparison
contract** into `Collections`, leaving only the then-blocked #1919; the floor
rose 14,815 → 14,890. The CCF-010 batch (#1904–#1911) before it closed the family
in `Core` outright: SR-AUD-046 is `remediated`. The tally read **59 remediated,
305 confirmed, of 364** at that point — three of the 305 carried a qualifier, and
the "304" this line previously carried counted neither side of the one split row
(SR-AUD-043); the Groups A–D batch then took it to **67 / 297**. The tracked CI
matrix covers nine fixtures; its missing direct `Collections.Blocking` fixture
is recorded as audit finding `SR-AUD-001`. Post-audit tally: **57 findings
remediated, 306 confirmed, of 364 total** — this line read `43 / 321` until
2026-07-31 and had been stale for several batches; the per-batch sections below
each state the figure they measured. The 2026-07-31 CCF-019 batch (#1886, #1890)
landed the approved owner-side detachment contract and left the tally
**unchanged**, because both SR-AUD-327 and SR-AUD-333 keep the
`confirmed (design-complete)` qualifier: item 1 of
`docs/OwnedTreeLifetimeContractPlan.md` §31 closes 26 of their 29 measured
use-after-free cases, and the remaining three (J11, X15, X17) belong to the
still-unapproved #1889 and #1892. The follow-on 2026-07-31 batch (#1887, #1891)
closed both of CCF-019's silent data-loss paths and likewise left the tally
**unchanged**, for the same reason; so did the 2026-07-31 batch (#1895, #1898)
that completed CCF-019's **compatible** remediation. CCF-019 is
**compatible-remediation-complete, not implemented**: #1888/#1889/#1896 are
declined and deferred, #1897/#1899 hold one open question each, and both
findings keep the `confirmed (design-complete)` qualifier.*

Sharp Runtime is in the post-audit remediation phase. The original type
classification, stabilization, and modularization queues are complete, and the
full native build/test and selective-isolation baselines are healthy. Work now
proceeds from the evidence-backed `audit/` inventory in bounded, independently
validated repair tickets. Consumer-driven API breadth remains legitimate later
work but must stay behind confirmed crash, lifetime, and public-contract
findings.


## 2026-08-03 — `System::Text` approval package verified (#2022) and the `System::Diagnostics` review (#2023)

Branch `feature/remediation-batch-text-approvals-next-review`, cut from `80c804b`, **not
pushed; no push was requested**. No merge, rebase, tag, PR, force-push or history rewrite; the
pre-existing local-only commits were left untouched. Four new commits — `017d336`, `6abd16e`,
`8114518` plus the handoff — all intentionally unsigned.

**Work unit 1 — the #2013–#2021 approval package.** `docs/SystemTextApprovalPackage.md` is now
the single place a decision is asked for, grouping the nine blocked tickets into six families
with one exact approval sentence each, a summary table and a compact checklist. Every "now"
row was re-measured against the shipped library
(`build-probe/2022_probe1_approval_verify.cpp` → `2022_probe1_verify.log`), and **ten premises
in the review plan's §14 did not survive**: the recommended `IsReadOnly`+`Clone()` spelling
costs `sizeof(Encoding)` **40 → 48** and a **vtable slot** (an identity-based spelling costs
neither, and is now recommended); **two** default factories emit a BOM as payload
(`BigEndianUnicode()` as well as `UTF32()`) and the **decode** side silently **consumes** a
leading U+FEFF; the fallback surface takes a **`char`** and cannot carry a non-ASCII scalar, so
#2017 needs a public virtual signature change §14.5 never asks about; `Rune::IsWhiteSpace` is
Unicode-aware **and divergent** (it contains U+FEFF, which .NET excludes); §14.8's
*"any index at or above 1,000,000 begins to throw"* is **false** (the shared grammar accepts
`{1000000}`…`{9999999}`), adoption also **widens** (`{0 }` is rejected here and accepted
there), and `runCompositeFormat` is a **formatting** engine that **pads while validating**, so
#2020 must extract a non-rendering scanner into `modules/core`; `EncodingInfo::GetEncoding`
returns `Encoding::UTF8()` **itself**, coupling #2021 to #2013; and exactly **two**
composite-format grammars exist today, not three or four.

**The one compatible portion found and implemented — #2022.** The previous batch claimed every
gated behaviour was pinned "so none can land silently". **SR-AUD-294 (#2018) had no pin at all**
— every `Rune` test in the repository uses ASCII and passes identically before and after a
Unicode repair — **SR-AUD-299 (#2021) had no test anywhere**, and three more were pinned only
in part. `TextGatedBehaviourPinTests.cpp`, **+8 add-only**, `SharpRuntimeTests_Text`
**288 → 296**, **no production file touched**, mutation-checked three ways (each mutation fails
the new pins and passes the old ones). No other blocked ticket contained a separable compatible
portion.

**CCF-012** was verified rather than restated: the population is exactly two implementations,
every other brace scanner in the repository is a different grammar, acceptance changes in
**both** directions, exception ordering does not, and the family closes **only** under the
shared-scanner option. **It is not marked closed.**

**Work unit 2 — the next namespace, re-derived by measurement.** `System::Diagnostics`:
**8 open findings of which 5 are `high`** (62.5 %, the highest high-ratio of any un-reviewed
namespace; `modules/io`, the nearest by count at 11, has **zero**), **no** existing `docs/`
plan, `PUBLIC_DEPENDENCIES Core.Base` only, one module, one namespace, seven of eight findings
on the same class. `docs/SystemDiagnosticsNamespaceReviewPlan.md` (#2023, 18 sections)
reproduced **all eight** premises: `WaitForExit(-1)` returns `false` after 0 ms; an
unredirected destroyed `Process` leaves state `'Z'` while a **redirected** one **blocks
2005 ms**; restart is `SIGABRT`; a non-`SA_RESTART` `SIGALRM` makes the **blocking**
`WaitForExit()` return early with `ExitCode` throwing; a `setsid` grandchild **survives**
`Kill(true)`; the output reference reads 4 bytes then 8. Seven causes **D-A … D-G**, five
compatible tickets **#2024–#2028** (`todo`) and three design tickets **#2029–#2031**
(`blocked`). **Nothing was implemented.** `Process` is a **pimpl** and `Debug`/`Trace` have no
data members, so every repair but #2030 is layout- and signature-invisible; and **TSan applies
to this namespace's compatible half** (#2027), the first time in the programme.

**Audit:** `SR-AUD-269`, `SR-AUD-271` and `SR-AUD-273` move to `confirmed (design-complete)`.
**107 remediated / 257 confirmed / 364 total**, design-complete **35 → 38**; the confirmed
total is unchanged because this batch remediated nothing, by design. **No `SR-AUD-*`
identifier was created.**


**Validation.** Gate **15,469 tests across 37 executables run individually, 15,462 passing,
1 skipped, 6 failing** for the same two measured causes (five `PingTests`, `ping_group_range` is
`1 0` — **#1962**; one `SocketTests`, no `/proc/net/if_inet6`), **no new failure**, and the +8 is
exactly #2022's. Build **0 errors / 0 warnings**; graph **41 / 91**; seams **2 / 18**; negative
fixtures **10 / 81** (91 invocations, peak 2 jobs); checker self-tests **45 / 45** and
**15 / 15**; module-boundary self-tests **7 / 7**; catalogue current; DB consistency OK;
`git diff --check` clean. **Doxygen NOT run — not installed here.**
**`scripts/check_selective_components.sh` did NOT complete**, and its result is **not claimed**:
a harness backgrounding mishap briefly left **two** concurrent runs (four compiler jobs, above
`CLAUDE.md`'s ceiling of two) — detected from `ps`, both killed at once, temp trees removed, and
the check restarted as a single invocation verified at exactly two `cc1plus` processes; that
single run then spent ~35 minutes on `Core.Base` alone and was stopped so the batch could be
committed. No other command exceeded two jobs and no claimed result was produced during the
overlap. This batch added **no public header, no component, no module edge and no CMake
metadata**, and the graph is confirmed unchanged, so nothing it touched is within the selective
check's subject.

## 2026-08-03 — #1804 re-verified, `System::Threading` namespace review, four repairs

**#1804 needed no work.** The batch instruction described it as blocked; the
database says it was **resolved 2026-07-30** by adding `_is_class_template_head`
to `scripts/check_version_seam_odr.py`, so a class template whose PRIMARY is
*defined* inside `namespace SharpRuntime::Testing` in a `modules/*/include`
header re-enters discovery instead of silently leaving it, and existing rule 1
rejects it. Re-verified rather than redone: the checker reports **2 seams / 18
specialisation definitions**, its self-tests **15/15**, and the two consumer-side
negative fixtures still reject every site. Nothing about the reported false-pass
condition is still live.

**The queue was empty, so the namespace review was the work.** `plan.sqlite3`
held zero `todo` tickets and the `task` porting queue is exhausted (0 rows at
`''`/`todo`), so `prompt.md` Step 1 selects nothing.

| Unit | Result |
|---|---|
| #1950 review | `docs/ThreadingNamespaceReviewPlan.md`; 38 findings → 9 causes → 12 tickets |
| #1946 | `MathF::Round` used `std::floorf`/`std::ceilf`, which libstdc++ does not declare |
| #1960 | GCC 13 `-Wdangling-reference` false positive under `-Werror` |
| #1961 (P0) | `Dns::GetHostEntry` recursed without bound → SIGSEGV |
| #1947 | `Semaphore`/`SemaphoreSlim` `Release` signed overflow (SR-AUD-206) |
| #1948 | `CountdownEvent::Reset` never woke its waiters (SR-AUD-211) |
| #1949 | two threading assertions that could not fail (SR-AUD-195/197) |
| #1962 | `Ping` has no `SOCK_RAW` fallback — opened `blocked`, unstarted |

### The repository did not compile

The first build of the checkout failed at object 7 of 723. `MathF::Round` used
the C99 float-suffixed `<cmath>` spellings; libstdc++ declares the TR1/C99 set
(`nearbyintf`, `roundf`, `truncf`) inside `namespace std` but **not** the C89 set
(`floorf`, `ceilf`). Fixing that exposed a second break at object 489: GCC 13's
new `-Wdangling-reference`, part of `-Wall` and fatal under `-Werror`, on a
binding `ListIndexerProxyTests.cpp` deliberately pins. It is a false positive —
`ElementReference<T>::operator const T&()` returns `*slot_`, a reference into the
owning collection — and the suppression is scoped to that one statement and
guarded off for Clang. Neither is a semantic change; both are recorded as their
own tickets rather than folded into unrelated work.

### #1961 — an unbounded recursion reachable from ordinary input

`Dns::GetHostEntry(const IPAddress&)` reverse-resolved with
`::getnameinfo(..., 0)` and fed the result straight to `GetHostEntry(string,
family)`. Without `NI_NAMEREQD`, `getnameinfo` **succeeds** for an address with no
reverse mapping and returns it in numeric form; the string overload re-parsed
that as a literal and called back in with the same address. Measured here:
`/etc/hosts` has `127.0.0.1 localhost` but no `::1` line, so
`getnameinfo(127.0.0.1)` returned `"localhost"` and terminated while
`getnameinfo(::1)` returned `"::1"` and killed `SharpRuntimeTests_Net` — which is
why only the IPv6 case was fatal and the whole executable's later tests were lost
with it. When the reverse lookup yields a literal there is nothing left to
resolve, so the entry is now built directly.

### Two corrections to SR-AUD-206, both measured

The audit recorded one overflowing expression per type and "the exception is
missing". The pre-fix UBSan probe reports **four** — the guard *and* the
increment in each type, the latter reached precisely because the guard's own
comparison overflowed — and `SemaphoreSlim`'s `CurrentCount` was left at
**-2147483648**, so `count_ > 0` could never hold again and every later `Wait`
blocked forever. That is a liveness failure, not a diagnostics gap, and it is why
#1947 was scheduled first.

### What is left, and what is honestly not green

Five `todo` tickets remain (#1951–#1955, all `System::Threading`, all compatible,
in that dependency order) and four designs (#1956–#1959) are `blocked` on the four
approval questions stated together in the review plan's §9. The gate is **15,105
tests / 37 executables**, 15,098 passing, **6 failing**: five `PingTests` because
this container's `net.ipv4.ping_group_range` is `1  0` — a real gap in `Ping`,
ticket #1962, left open — and one `SocketTests` because IPv6 is absent from the
container entirely. `scripts/check_selective_components.sh` **passed** in full.
**Doxygen could not be run: doxygen is not installed here**, so the 1,942
ceiling is unverified this batch.

## 2026-08-02 — #1938 row-4 design complete

The design-only batch changes documentation and planning records, not any
production header/body, ordinary test assertion, accepted/rejected input,
result, exception, provider, culture, style, kind, exact-format behavior, or
XML bridge. Rows 1–3 remain: fixed two-digit month/day; one-through-seven exact
fraction digits with the eighth rejected; and `Z`/`z` or exactly `±HH:MM`
offsets with short/compact forms rejected. Current .NET accepts each wider
general form, but the recommendation remains to retain/document the port's
subset.

`docs/DateTimeExactParsingAndKindDesign.md` contains the five-type overload
inventory, 50-row exact grammar matrix, provider/culture and style legality
matrices, kind matrix, pinned dotnet/runtime commit
`0eb5481340ea675857c7a7abf18f68a60b52a686`, corrected premises, component and
timezone dependencies, selected/rejected policies, exact approvals, source/
ABI/layout/vtable/symbol analysis, migration, tests, sanitizers, performance,
and rollback. All exact families are missing APIs; DateTimeKind is declared but
unstored; no production culture type implements IFormatProvider; styles are in
Globalization while parsers are in Core.Base; and XmlConvert format/mode bodies
ignore their argument.

Validation is green: focused existing date/time tests 370/370; full
socket-enabled clean gate 15,092/15,092 across 37 executables; ten selective
components; module graph 41/91 and validator 7/7; seams 2/18 and self-tests
15/15; negative fixtures 10/81 over 91 compiler invocations at peak two and
checker 45/45; component catalogue and DB consistent; Doxygen 1,938/1,942;
whitespace clean. No sanitizer claim is made because no production object
changed. `ccache` requires repository-local `CCACHE_DIR`; socket gates require
the approved socket-enabled path.

Compilation never exceeded two aggregate jobs. The repository started at
40 MiB with `build`, `build-probe`, and `build-tmp` absent. The completed build
and cache cleanup reclaimed 703,438,848 allocated bytes, preserving only the
6,877-byte current-behavior probe source and 2,446-byte raw log in a 16 KiB
`build-probe`. Final total is 171 MiB: the 133,087,232-byte ignored canonical
Doxygen output is retained because a safety review rejected deleting its
parent, and the pinned googletest checkout occupies 4,755,456 bytes. No build
tree was created under `/tmp`, `/var/tmp`, or `/dev/shm`.

## 2026-08-02 — #1929 row-4 implementation decomposition

Ticket #1938 established that row 4 is not one implementation decision. All
five exact overload families are absent; providers and culture types are
disconnected; styles are declared in Globalization but unused by Core.Base;
DateTimeKind is declared but DateTime has no kind state; and XmlConvert ignores
existing format/mode arguments. No production behavior is changed by this
design classification.

The future work is split at independent approval, dependency, API/ABI,
migration, benchmark, and rollback boundaries:

| Ticket | Group | State after design | Exact boundary |
|---|---|---|---|
| #1939 | 4A | `needs_user` | four additive invariant string-only single-format DateOnly/TimeOnly exact APIs and a private scanner |
| #1940 | 4B | `blocked` | provider/culture ownership and lookup after SR-AUD-280/SR-AUD-285 premises and an explicit component/ABI transition |
| #1941 | 4D | `needs_user` | storage-only packed DateTimeKind representation and explicit kind surface; conversion remains blocked on timezone capability |
| #1942 | 4C | `blocked` | exact DateTimeStyles/TimeSpanStyles validation and effects after provider/kind/timezone/overload prerequisites |
| #1943 | 4E1 | `blocked` | remaining single-format provider/style exact APIs for DateTime, DateTimeOffset, TimeSpan, DateOnly, and TimeOnly |
| #1944 | 4E2 | `blocked` | separately selected candidate-collection and span-like overload shapes |
| #1945 | 4F | `blocked` | existing-body XmlConvert exact-format/mode corrections and explicitly approved additive bridges |

#1939 is the recommended next approval batch. #1941's storage-only phase is
also independently worded but must not be combined with #1939. #1940,
#1942--#1945 cannot be implemented from the current repository premises.
`docs/DateTimeExactParsingAndKindDesign.md` owns the complete inventory,
matrices, dependencies, source/API/ABI analysis, tests, benchmarks, rollback,
exclusions, and exact copyable wording. No new SR-AUD identifier is created;
audit numbering remains frozen at 364.

## 2026-08-01 — #1936 exact Option 1 delivered

The approved change is confined to the inline generic
`ImmutableSortedSet<T>::SetEquals` body. Shared backing now returns true in
O(1); otherwise `other` is rebuilt under this set's existing comparator, the
post-collapse count check remains, and both ordered ranges are scanned using
`!less(a,b) && !less(b,a)`. No raw element or raw `std::set` equality remains,
and no floating/nullable specialization or other collection was changed.

The retained 105-row prefix matrix was reproduced byte-for-byte. Postfix,
direct float/double/long-double NaN self, copy, independent, insertion-order,
payload, mixed, duplicate, and comparer-identical equality cases are true;
equal sets are no longer proper subsets or supersets. Case-insensitive string
sets are likewise fixed. Nullable floating controls and all non-proper
relations, overlap, intersection, union, except, and symmetric-except results
remain unchanged. Eleven permanent tests raise Collections.Core 2,752 →
2,763 and the repository 15,081 → 15,092.

The eight-mutation campaign has six killed mutations and two correctly
classified equivalents: removing the count check is redundant with the final
both-ended scan, while removing the shared-data fast path changes performance
only. No unexpected survivor remains. Representative source/DWARF/nm probes
show unchanged public declarations, aliases, iterator types, size/alignment
(16/8), sole field offset (zero), vtable/virtual surface, `noexcept`,
`constexpr`, public mangled names, and 35 undefined names. Approved inline
template effects remove 50 weak raw equality/old optional helpers, add six
weak shared-pointer equality helpers, and grow the measured `SetEquals` body
to 0x380.

Five alternating warm-up pairs and eleven measured alternating pairs prove
self/shared copy at about 9 microseconds per 5,000 calls versus about 39
milliseconds before. Independent comparisons retain O(m log m + n); ordinary
controls were noisy and did not justify a new regression ticket. Focused
ASan+UBSan tests pass 16/16 and a 2,000-iteration stateful-comparer lifecycle
probe is clean. The LSan-enabled tests complete semantically but LeakSanitizer
then fails at its ptrace step, so no clean LSan discovery is claimed.

All affected main and sanitizer objects were newer than the edited header and
the new test before relinking. The full two-job socket-enabled CI gate passes
15,092 tests across 37 executables with zero warnings/errors; the ten-component
selective matrix passes; Doxygen remains 1,938/1,942. No new defect, ticket, or
SR-AUD identifier was created. Audit numbering and totals stay frozen.

## 2026-08-01 — #1936 design and #1937 evidence disposition

Ticket #1936 remains todo and unapproved. The complete retained matrix covers
direct and optional float/double/long double, NaN payloads, signed zero,
finite/infinity, empty/single/mixed/duplicate/proper sets, self/copy/
independent/insertion-order cases, same/different comparer relations, every
related immutable set operation, concrete/interface availability, and a
non-floating case-insensitive string control. Direct NaN-equal sets and the
custom string sets return `SetEquals=false` while both non-proper relations are
true, causing both proper relations to be true. Optional floating is already
correct under #1925. Other operations are correct.

The root cause is the generic fallback to raw `std::set::operator==` after
otherwise-correct rehashing under this set's comparator. Current .NET performs
a comparator-equivalence ordered scan. Recommended Option 1 replaces the raw
fallback and nullable specialization with one generic two-direction
comparator-equivalence scan after the current post-collapse count check, plus
an optional shared-data fast path. It changes one inline template body, no
declaration, alias, iterator, layout, or vtable, but intentionally changes
direct-floating and custom-comparer semantics and emitted code. Exact approval
wording and alternatives are in
`docs/ImmutableSortedSetFloatingEqualityDesign.md` and the consolidated packet.

Ticket #1937 is now done with disposition not reproducible and no production
optimization. The historical 2.092x result is retained but corrected: it used
one warm-up and separate seven-row pre/post campaigns, not alternating pairs,
and “rehash” recorded no bucket history. The replacement GCC 14.2/libstdc++ 14
campaign uses O2 and O3, five warm-up and 25 measured alternating pairs, 18
cases per configuration, 360 warm-up rows, 1,800 measured rows, and 900 paired
results. The exact-work nullable-double finite rehash-hit ratio is 1.026 at O2
and 0.964 at O3, with 12/13 and 14/11 wins/losses and wide spreads crossing
one. Counts, finds, finite hashes, buckets, loads, and collisions match;
unchanged direct-double and optional-int controls exhibit the same noise.

Generated code contains semantically required NaN checks, but optional-double
cache selection, 24-byte node, ordinary iterator, 56-byte SetType, 64-byte
wrapper, bucket distribution, and locality opportunity are identical. NaN and
mixed pre/current paths are not like-for-like because the old set cannot find
NaNs; null hashes intentionally differ. No alias, iterator, representation,
layout, symbol, or behavior was changed. Exact raw results, mechanism, and
reopening conditions are in
`docs/NullableFloatingHashSetPerformanceEvidence.md`.

The optional #1929 row-4 design was not attempted after these substantial
units. Rows 1–4 remain todo/inactive/unapproved. #1894/#1899 remain blocked;
#1888/#1889/#1896 remain declined and are not re-proposed; #1773 remains
blocked. #1934/#1925 and #1932/#1935 remain complete; #1926 remains wontfix.
No new ticket or SR-AUD identifier was created; audit numbering stays 364.


## 2026-08-01 — coordinated #1934 then bounded #1925 delivered

The user approved the operative wording in
`docs/CompositeFloatingKeyPolicyDesign.md` for only direct
`std::optional<float>`, `std::optional<double>`, and
`std::optional<long double>`. #1934 was activated, completed, validated, and
committed before #1925 was activated. No nested optional, pair, tuple, array,
variant, vector, arbitrary object traversal, new hash capability, second
policy framework, reserved `std` specialization, #1926 work, or unrelated
ticket was absorbed.

### #1934 generic/default semantics

`ComparisonPolicy.hpp` now identifies only the three direct approved forms.
Presence is decided first: null equals null, hashes to zero, and orders before
present values. Present values use existing CCF-010 floating comparison,
equality, and hashing: all NaN payloads compare/equal together and hash
canonically; signed zeros compare/equal and hash together; finite values and
infinities retain numeric ordering/equality. Generic `Comparer` and
`EqualityComparer`, their interfaces, object wrappers, and dedicated Nullable
comparers agree. Raw optional/Nullable operators remain NaN-nonreflexive.

Before, generic/interface NaN equality was false and comparison against finite
was zero while dedicated Nullable paths returned true/-1. After, all approved
default/dedicated paths agree. Non-floating optional int, unsigned, string,
enum, and user-type controls are identical. Five permanent tests plus three
existing focused tests pass 8/8 at the #1934 checkpoint. Six mutations are
accounted for (5 killed, 1 compile-rejected, zero unexpected). Comparer objects
and interfaces stay 8/8; 438 sharp-runtime defined symbols, 54 vtables, and 26
undefined symbols are identical. Thirteen no-longer-used libstdc++ helpers
disappear from the whole fixture. Commit: 5e384fd8.

### #1925 collection selection

Only after #1934, `DefaultKeyLess`, `DefaultKeyHash`, and `DefaultKeyEqual`
select those same three forms. Sixteen consumers are permanently instantiated:
Dictionary, HashSet, both Frozen, both ReadOnly, both Immutable hashed,
ConcurrentDictionary, OrderedDictionary, KeyedCollection, SortedSet and view,
SortedDictionary, SortedList, ImmutableSortedDictionary, and
ImmutableSortedSet. Null, multiple NaN payloads/insertion orders, signed zero,
finite extrema, infinities, load/trim/rehash, erase/reinsert, copy/move,
iteration, union/intersection/equality/self-comparison, projection creation and
lookup, and explicit-comparer precedence are covered.

The identical pre-change postcondition suite had 201 assertion failures over
the three complete matrices; the final five tests pass. Ten mutations are
accounted for: five behavior mutations killed and five alias/propagation
mutations compile-rejected, with zero equivalent or unexpected survivors.
Pair/tuple dictionaries remain ill-formed. The negative boundary grows seven
sites to 10 fixtures / 81 sites / 91 compiler invocations, peak two. Commits:
6b5403c4 production and 47f84eea permanent proof/migration evidence.

### Compatibility, representation, and corrected premises

All three public `DefaultKey*` aliases and the six public Dictionary, HashSet,
Frozen, and ReadOnly `MapType`/`SetType` backing aliases move. Instantiated
public functions that name those aliases and private backing types in the
other consumers move accordingly. All 48 outer object size/alignment,
standard-layout, and trivial-copy measurements remain equal; public field
offsets are not observable. Predicate noexcept/constexpr state is preserved.

The design's broad iterator premise is corrected: libstdc++ 14 erases
predicates from float/double hash iterators and from tree iterators. Only
optional-long-double hash iterators and its dependent immutable-dictionary
deduced iterator move from cached to uncached node spelling. This does not
make the change ABI-neutral. The full fixture has 12,708 → 12,745 defined
symbols (2,124 removed, 2,161 added), 182 unchanged undefined symbols, and
113/113 vtables: six standard predicate-bearing control-block vtables move;
31 relevant sharp-runtime vtables and slots are identical.

`ImmutableSortedSet<optional<F>>::SetEquals` required a branch gated explicitly
on the approved trait because raw element equality is NaN-nonreflexive. The
same existing defect reproduces for direct `double`; inactive #1936 owns it.
Inactive #1937 owns the separate 2.092x finite rehash-heavy nullable-double
HashSet lookup observation. Neither was absorbed and no audit identifier was
issued.

### Performance, sanitizer, rebuild, and gates

Two seven-round campaigns cover Dictionary/HashSet/SortedSet finite,
null-heavy, NaN-heavy, mixed, and rehash-heavy paths. Exact medians are retained
in `build-probe/1925_nullable_collection_benchmark_summary_combined.log`.
Corrected NaN-heavy hash paths take 0.211–0.294x while finding all 40,000 probes
instead of 8,000. Mixed SortedSet insert/lookup takes 10.157x/12.729x because
the corrected tree keeps 10,005 keys where the old one silently kept 2; this
is defect removal, not a like-for-like regression. Same-work finite/null paths
are recorded without optimization. #1926 remains wontfix.

Focused ASan+UBSan is clean, including the rebuilt retained Collections.Core
target at 24 nullable-focused tests. LSan test discovery is ptrace-blocked and
is not reported clean. Sanitizers do not establish comparer/hash semantics.
Dependency tracking rebuilt 65 main objects/4 directly affected executables,
206 modular objects/all 37 executables, 76 sanitizer objects/Collections.Core,
ten selective consumers, and 91 negative compiler cases without a target
clean. All commands were non-overlapping and capped at two jobs.

Final socket-enabled local CI is 15,081/15,081 across 37 executables with zero
warnings/errors; Collections.Core 2,752, Integration 893, Core.Base 5,585.
Selective components, module/catalog/database checks, seams 2/18, checker
45/45, and Doxygen 1,938/1,942 are green. Audit stays 68/296/364. The optional
#1929 row-4 planning tail was not attempted after the rebuild-sensitive work;
rows 1–4 remain unchanged. Detailed ABI, mutation, performance, disk, remote,
stash, and safety evidence is in the first `NEXT.md` handoff and
`docs/CompositeFloatingKeyPolicyDesign.md` §§11–12.


## 2026-08-01 — #1926 approved `wontfix` closure

The independently approved decision closes only #1926. Retained
`build-probe/1926_fasthash*` evidence covers GCC 14.2.0 / libstdc++ 14 on
x86-64 and an isolated `Dictionary<long double, int>` insertion workload of
200,000 keys. Across 25 alternating rounds, today's uncached
`DefaultHash<long double>` node shape was 1.319× the pre-#1919 cached shape and
slower in 24 of 25 rounds. The prior lookup-improvement claim is withdrawn as
noise. The mechanism is libstdc++'s private `std::__is_fast_hash` selection:
the current node is 48 bytes with the uncached iterator form; the probe-only
candidate is 64 bytes and restores the cached iterator form.

Correctness, portability, and maintainability take precedence over this
isolated insertion result. CCF-010's findable/nonduplicating NaN keys and
canonical NaN/signed-zero hashes must not regress. No reserved-library
specialization, second policy framework, container representation, public
alias, iterator, layout, ABI, or symbol change is authorized. Reopen only for
a relevant standard-library update, a portable public customization point, a
stable behavior- and representation-preserving optimization, or evidence from
another supported toolchain. #1925 and #1934 remain `needs_user`; #1929 remains
partial; #1932 remains done; #1773 remains blocked. No audit identifier was
issued.


## 2026-08-01 — #1935 bounded compilation-job tooling

The previous checker had a built-in default of 3 while the wrapper scripts
separately defaulted `SHARP_RUNTIME_BUILD_JOBS` to 3. A direct no-argument
checker invocation therefore started three compiler processes, and direct
`check_repository()` calls in its own self-tests bypassed the wrapper's value.
That historical violation was retained, not recreated.

Ticket #1935 centralizes resolution in `scripts/job_count_policy.py`:

1. explicit argument/API value;
2. `SHARP_RUNTIME_BUILD_JOBS`;
3. deterministic safe default 2.

Only decimal 1 and 2 are accepted. Zero, negative, malformed, and excessive
values fail clearly; no CPU detection or silent clamp exists. The checker
CLI/API, `local_ci_check.sh`, and `check_selective_components.sh` share the
resolver. Both wrappers export the result, local CI also passes `--jobs`
explicitly, and the component workflow pins the variable to 2. There is no
nested independent compiler pool.

The checker suite is 45/45. It covers explicit 1/2, omission, environment 1/2,
argument precedence, all invalid classes, deterministic ordering, child
failure, and peak accounting. Its fake compiler reaches exactly peak 2 without
performing real compilation; no three-job reproduction was run. The final real
checker is 10 fixtures / 74 sites / 84 invocations / peak 2. Full socket-enabled
CI remains 15,071/15,071 across 37 executables, and the selective matrix is
green including WebSockets 24/24.

This is tooling/process safety only: no production header/source, accepted or
emitted behavior, public API, alias, iterator, ABI, layout, symbol, vtable,
`noexcept`, `constexpr`, or component edge changed. Retained summaries are
`build-probe/1935_job_policy_selftest.log` and
`build-probe/1935_negative_fixture_final.log`. No audit identifier was issued.


## 2026-08-01 — #1932 exact Option 2R implementation

Branch **feature/remediation-batch-1932-option-2r**, no upstream, based on
cc833f5d. Commits: 0e622298 production, 66243a02 permanent tests, 976be6df
design/audit/decision reconciliation, followed by the final handoff commit.

The approval was applied only to HRE H3/H4/H5 and WebException W3/W5. A
non-null pointer rethrowing System::Exception supplies its exact HResult,
including zero. Null and non-System pointers keep the existing family base;
request error and both status types remain orthogonal. H3/W3 perform the
constructor-local classification and H4/H5/W5 delegate to them before assigning
existing metadata. There is no base helper, producer wrapping, new API, field,
or representation change.

The pre-fix semantic suite was 2/12 and failed only outer HResult assertions;
postfix is 13/13, combined #1875/#1932 28/28, ASan+UBSan 13/13, and full CI
15,071/15,071. Exact zero/default/type-specific/custom/nested/null/non-System,
copy/move/assignment/rethrow, and sync/async forwarding are permanent. HRE is
still 176/8 and WebException 168/8; declarations, default arguments, symbol
sets, vtables, virtual slots, fields/offsets, `noexcept`, and `constexpr` are
unchanged. LSan discovery is ptrace-blocked and is not claimed.

Queue population: 1,921 done, 2 todo, 6 blocked, 2 needs_user, 3 wontfix of
1,934. #1932 is done; no ticket is doing. No new inactive ticket or audit ID.
Audit totals stay 68/296/364. Pending decisions are unchanged: #1926's wontfix
recommendation; #1929 rows 1–4; coordinated #1934/#1925; and #1899/#1894.
#1773 remains blocked and downstream inspection remains prohibited.

All required module/catalog/seam/fixture/selective/full-CI/Doxygen/database/
diff gates are green. Final disk and repository-state details are in the first
NEXT.md handoff. One no-argument negative-fixture invocation used the script's
default three-worker pool before the corrected final `--jobs 2` run; this is
recorded as a process-policy violation and no false two-job-only claim is made.
All actual CMake builds were capped at two, and no build tree was created under
`/tmp`, `/var/tmp`, or `/dev/shm`.

Recommended next clean-context decision: independently accept #1926's wontfix
wording, or explicitly approve the source/ABI-sensitive #1934 then bounded
#1925 group. Neither is authorized by #1932.


## 2026-08-01 — #1932/#1933 design, #1925 classification, and remaining decisions

Branch **feature/remediation-batch-1932-1933-decisions**, no upstream, based on
0e1b47d6. This was deliberately a design/evidence batch. No production/header
file, public declaration, accepted/rejected input, value, exception result,
symbol, layout, vtable, noexcept or constexpr contract changed, and no
permanent test was added.

| Commit | Planning unit | Status/result |
|---|---|---|
| 67433746 | #1932 | complete network-exception HResult design; todo pending semantic approval |
| 4b2fb2d1 | #1933 | done, evidence-only; optimization designed but not implemented |
| d000f059 | #1925 / #1934 | direct nullable-floating subset classified needs_user; new inactive #1934 |
| eb686b48 | consolidated decisions | packet replaced; #1899 visitor-escape premise corrected |

**#1930/#1931 reconstruction:** #1930 is done inside #1927 commit 28e72ba7;
#1931 is done inside #1929 commit 83cfb10a. They were not merely recorded or
left open. #1932 remains todo. #1933 is now done. No ticket is doing.

### #1932 decision

The complete matrix shows one constructor-specific .NET rule, not a universal
Exception rule:

- HRE H1/H2 and null-inner H3/H4/H5 retain 0x80131500.
- WebException W1/W2/W4 and null-inner W3/W5 retain 0x80131509.
- A non-null System inner makes H3/H4/H5/W3/W5 copy its exact HResult,
  including zero; HttpRequestError, HttpStatusCode and WebExceptionStatus never
  override it.
- A C++-only non-System exception_ptr retains the outer base value.
- Copy/move/exception_ptr preserve the already-stored result and every message,
  inner identity and networking field.

Recommended **Option 2R** implements exactly those two types and five causal
shapes. It adds no field/helper/API and has no declaration/ABI/layout/vtable/
symbol/noexcept/constexpr change; HRE remains 176/8 and WebException 168/8.
It is nevertheless an observable constructor-result change and was not
implemented. The exact approval and rollback/test matrix are in
docs/NetworkExceptionHResultPropagationDesign.md and Group A of the packet.

### #1933 decision

The retained actual binaries reproduce a material direction but not one
universal multiplier: original 7+7 rounds were 12.638 versus 17.103 ms
(1.353x, disjoint 11.998–14.735 and 16.024–18.945); independent 28+28 rounds
were 26.343 versus 33.907 ms (1.287x aggregate, 1.286x paired, current slower
22/28, broad overlapping 16.747–40.501 and 21.304–44.703 ranges). The ticket's
attribution is corrected: those binaries compare all of #1929 rows 5–6's
trimming, wider digit scan, tick scaling and direct commit, not tick storage
alone.

A scale-lookup candidate passed 11,111,110 scaling states, 1,941,233 TryParse
inputs and 1,030 exact Parse observations. It then ran five warmups plus 25
measured rounds at GCC 14.2 C++23 -O2 and -O3 over no fraction; 1/3/4/6/7
digits; whitespace; invalid eighth digit; and out-of-range time, separately for
Parse/TryParse and success/failure. It improved several valid TryParse rows,
but -O3 failures favored current and -O2 seven-digit Parse was 5.9% slower,
losing 23/25. GCC already unrolls current scaling to immediate multipliers at
-O3. Classification: **optimization designed but not implemented**, with a
toolchain-specific component. Exact tick behavior and TimeOnly 16/4 remain.

### #1925/#1934 classification

#1925's optional<double> defect is real, but “all composites recurse” is false.
Direct/nested optional and variant hashed NaNs are unfindable; ordered
NaN/1/2 can silently collapse; pair/tuple hashed Dictionary is currently
ill-formed; .NET delegates for mapped Nullable and ValueTuple, not arbitrary
fields. Generic Comparer/EqualityComparer<optional<F>> also remains wrong while
dedicated Nullable comparers are correct, so separate inactive #1934 records
that surface.

Recommended bounded approval is exactly direct
optional<float/double/long double>, #1934 semantics first and #1925 key
selection second. It changes comparator-bearing C++ type/symbol identity and
public MapType/SetType and iterator/deduced types even where measured
size/alignment stays equal, so it is needs_user and requires a coordinated
rebuild. No nested optional, pair, tuple, array, variant, vector, user-field
recursion or new hash support is implied.

### Remaining packet and queue

docs/RemainingApprovalDecisions.md is now authoritative:

1. #1932 Option 2R: recommend independent approval.
2. #1899/#1894: recommend retaining #1898 and closing #1899 wontfix. D is four
   additive visitor conveniences; G is D plus five deprecation warnings. A
   callback can retain its observer, so neither is lifetime enforcement.
3. #1929 rows 1–4: recommend retaining/documenting rows 1–3 and design-only row
   4; separate wording exists for each widening/reversal.
4. #1934 then bounded #1925: recommend only the direct nullable-floating group,
   with explicit source/ABI rebuild approval.
5. #1926: recommend wontfix; the 1.319x insert cost is real (24/25) but the
   reserved libstdc++ specialization grows nodes 48→64 and is nonportable; the
   old lookup claim is noise.

The #1899 visitor correction is appended in
docs/OwnedTreeLifetimeContractPlan.md §46 and
docs/TextSubsetCompatibilityDecision.md §9. D does not unblock #1894. G could
give #1894 five -Werror deprecation-diagnostic sites, but ordinary calls still
compile and the fixture could not be called a structural lifetime proof.

Current database population is 1,934 tickets: 1,920 done, 3 todo, 6 blocked,
2 needs_user and 3 wontfix. Open rows are #1773/#1888/#1889/#1894/#1896/#1899
blocked; #1926/#1929/#1932 todo; and #1925/#1934 needs_user. The three declined
rows remain blocked by design. No new audit identifier was issued.

### Closure evidence and resources

The full socket-enabled local gate passed 15,058/15,058 across 37 executables,
including Integration 880 and Core.Base 5,585, with zero build warnings/errors.
The selective matrix passed, including WebSockets 24/24 without skips. Module
boundaries are 41/91; seams 2/18; negative fixtures 10/74 (84 invocations,
peak two jobs); catalogue/database checks green; Doxygen 1,937/1,942; audit
68/296/364. No new sanitizer result is claimed because production objects did
not change; retained sanitizer evidence and semantic probes are distinguished
in NEXT.md.

Build sizes in KiB, start→final: build 795,596→795,600; build-asan
4,014,420→4,014,420; build-modular 1,331,520→1,331,520; build-probe
74,492→75,932; build-consumer 12→12; build-tmp 8→4; build-ubsan/build-tsan
absent. cmake-build-debug ended 89,296 KiB; only the inherited approximately
88 MiB start was recorded, a known limitation. Retained evidence grew
build-probe by 1,440 KiB. Batch-created validation cache/temp cleanup reclaimed
13,876 KiB.

All compilation was sequential by command with at most two jobs. Every
mktemp-based script used repository-local build-tmp; no build tree was made
under /tmp, /var/tmp or /dev/shm. The negative-fixture unit test required the
repository-local ccache path; its corrected run passed 37/37. Socket-bearing
gates used approved socket-enabled execution.

Remote refs and stashes were read-only. The anomalous remote ref remains
f3a2bb56 with the two unchanged update-by-push reflog entries. All three
stashes are byte-for-byte listed unchanged in NEXT.md. No push/fetch/pull/
merge/rebase/tag/publication/stash mutation occurred. #1773 remains blocked;
CNA/mobile-eggbert were not inspected or modified.

Next action requires a clean context and explicit wording from one or more
independent packet groups. Prefer #1932 alone as the smallest implementation
batch if Option 2R is approved; keep #1934/#1925 in a separate rebuild-sensitive
batch.


## 2026-08-01 — approved #1927/#1928/#1929 rows 5–6, then compatible #1880/#1875

Branch `feature/remediation-batch-approved-text-subset-p3`, with no upstream.
Five independent ticket commits on `f3a2bb56`:

| Commit | Ticket | Result |
|---|---|---|
| `28e72ba7` | #1927 | exact .NET-style Single/Double Round delegation; large finite inputs no longer become infinity/stray ULP; private Math funnel preserves negative zero (#1930) |
| `88020c8d` | #1928 | exact leading-`Rounding` digits-range message, with taxonomy/order unchanged |
| `83cfb10a` | #1929 rows 5–6 | outer whitespace, 1–2 digit DateTime clock fields and 1–7 digit exact fractions; TimeOnly exact-tick representation corrected in-place (#1931) |
| `e3caaedf` | #1880 | all four date/time TryParse false exits publish MinValue/default; CCF-002 closed |
| `f912b98b` | #1875 | 45-type current-.NET matrix; 12 constants plus Win32-root correction, 27 exact inheritance controls; SR-AUD-157 remediated |

The exact §6.5 approval boundary was held. #1929 remains `todo` and explicitly
partial; row 1, row 2 beyond seven digits/general all-digit acceptance, row 3,
and row 4 remain unapproved and permanently pinned unchanged. #1894 and #1899
remain blocked with no option chosen; #1888/#1889/#1896 remain declined and
blocked; #1925 was not started; #1926 remains deferred, leaning `wontfix`;
#1773 remains blocked without downstream inspection.

Corrected premises are additive, not historical rewrites: the original #1927
ordinary sweep remains bit-exact but stronger volatile probes found the private
negative-zero defect (#1930); TimeOnly's public tick contract was silently
millisecond-truncated despite its unchanged 16/4 layout (#1931); historical
#1929 row 2 overlaps operative row 6 only through seven digits; and #1875's
population has three categories (12 constants, 30 inheritance, 3 conditional),
not two. The separable HttpRequestException/WebException inner-HResult policy is
inactive #1932. A stable TimeOnly valid-parse cost, median 12.638→17.103 ms
(1.353× over seven identical-work rounds, with disjoint ranges), is required
exact-tick work and is retained as inactive #1933 rather than optimised inside
the correctness commit.

Permanent tests **15,024→15,058** (+34) across 37 executables: Core.Base
5,585/5,585 and Integration 880/880. Combined ASan+UBSan focused runs are clean
against newer instrumented objects; semantic guarantees remain permanent-test
claims. LSan is not claimed where sandbox `ptrace` prevented discovery; TSan is
inapplicable to these non-shared-state changes. No public signature, base,
overload, `noexcept`, `constexpr`, calling convention, symbol set, vtable,
layout or module edge changed. Final structural baselines: graph 41/91, seams
2/18, negative fixtures 10/74, Doxygen 1,937/1,942, audit 68/296/364 with
numbering frozen.

Build directories began/ended: `build` 753/777 MiB, `build-modular` 1.3/1.3
GiB, `build-asan` 3.8/3.9 GiB, `build-probe` 45/73 MiB,
`build-consumer` 12/12 KiB, `build-tmp` 8/8 KiB and `cmake-build-debug` 88/88
MiB; UBSan/TSan standalone trees remained absent. Reclaimed space: 0, because
all reproducible evidence was retained. Every compile used at most two jobs and
no build tree was created outside the repository. The restricted-sandbox
WebSocket failure was reproduced as `socket()` denial; the unchanged
socket-enabled selective and full gates passed without skips. Remote/stash
state remained read-only; the three stashes are unchanged and the observed
remote reflog remains `563b832d` then `f3a2bb56`, both `update by push`.

Next bounded work after a context refresh: independently design #1932 and
isolate #1933 if still wanted. Do not infer approval for the remaining #1929
rows or for #1894/#1899/#1925/#1926.


## 2026-07-31 — Group E subset: #1897 option B, and three decision records (#1899, #1926, #1927-#1929)

Branch `feature/remediation-batch-group-e-subset-decisions`. The batch
instruction approved **exactly one implementation**, `#1897` **option B** as
worded in `docs/RemainingApprovalDecisions.md` §E.1, and nothing else in Group E.

**What was wrong.** `fromNlohmann` recursed once per nesting level while turning
the parsed document into `JsonNode` objects, so `JsonNode::Parse` of deeply
nested text killed the process with a stack overflow — measured between **16,000
(survived)** and **18,000 (SIGSEGV)** levels on an 8 MiB stack, with every ASan
frame in the port's own recursion rather than in nlohmann's parser. It is the
**only CCF-019 case reachable from untrusted input**: the caller merely has to
pass `Parse` a string it did not write.

**What landed.** An explicit heap worklist of suspended containers, the same
shape #1895 used for teardown. Arrays, objects and alternating containers now
build correctly to **200,000**, and the change is **fully compatible** — the 19
round-trip shapes, the null semantics, the 13 malformed inputs and the
1,000-sibling ordering are **byte-for-byte identical** before and after; strong
symbols are 13 before and 13 after, identical, with an identical undefined set;
layout, vtable and exception specification are untouched; parse throughput is
neutral (median 0.932×, best-of-7 1.012×, inside a much wider noise band).
Design record: `docs/OwnedTreeLifetimeContractPlan.md` §44.

**What it deliberately did not do.** Option A — applying the existing
`JsonDocumentOptions::DefaultMaxDepth = 64`, as .NET's `JsonNode.Parse(string)`
and this module's own `JsonDocument::Parse` both do — **was not approved and is
not implemented**. `JsonNode::Parse` therefore still accepts text .NET rejects.
That deviation is now stated in the `Parse` doc-comment and pinned by two tests,
so option A cannot land, or be forgotten, silently.

**Baselines after the ticket:** 15,024 tests across 37 executables (was 14,998,
**+26**); 244/244 `Text_Json` clean under ASan+UBSan+LSan against a binary newer
than the changed body; post-audit tally **unchanged at 67 remediated / 297 open
of 364**, numbering frozen at **364** — SR-AUD-327 keeps its
`confirmed (design-complete)` qualifier; module graph **41 / 91** unchanged;
version seams **2 / 18** and negative fixtures **10 / 74** unchanged, because
option B outlaws no spelling and so does **not** unblock #1894.

**Three design units accompanied it, and none implemented anything.**

- **#1899** (`blocked`, no option chosen) gained four measured corrections:
  the surface is **four** overloads not two and all four take a range, so option
  D's single-node visitor is a counterpart to none of them; option B is a
  **silent** ABI break of the class declined in #1889, because the accessor *is*
  emitted as a weak symbol and an Itanium mangled name does not encode the return
  type; `Ancestors`/`AncestorsAndSelf` contribute **zero** symbols, so options C
  and E are pure source changes; and the disputed accessor is **one of eighteen**
  borrowed `const&` accessors in the same headers. Two options the record lacked
  were added — a debug-only lifetime **registry** and **deprecation** beside
  additive safe spellings — because none of A–E gave a diagnostic without a
  break. `docs/OwnedTreeLifetimeContractPlan.md` §45.
- **#1927/#1928/#1929** were resolved into one packet,
  `docs/TextSubsetCompatibilityDecision.md`. It finds the **numeric half already
  closed** — every disputed row matches .NET, including .NET's own permissive
  grouping rule — corrects #1927's premise (below each round limit the two
  funnels agree **bit for bit** over 140,000 samples, so it is a defect fix, not
  a value change), and finds **six** date/time narrowings rather than four, two
  of them the port disagreeing with **itself** about whitespace and about
  fractional-second precision. It recommends approving #1927, #1928 and #1929's
  self-consistency rows together and deciding the widening rows separately.
- **#1926** was isolated (`docs/CollectionsComparisonContractPlan.md` §20): the
  mechanism is **proven** — restoring only `__is_fast_hash` restores both the
  node size and the iterator type — the insert regression **replicates** at
  1.319× (24 of 25 rounds), and the recorded **lookup improvement does not**.
  Recommendation unchanged: defer, leaning `wontfix`, since it means specialising
  a reserved libstdc++ internal that is dead code on every other standard
  library.

## 2026-07-31 — approved Groups A–D of the decision packet (#1854, #1862, #1858, #1865, #1879, #1884, #1863)

Branch `feature/remediation-batch-approved-groups-a-d`, six commits. **All seven
tickets the batch instruction approved are `done`**, in the packet's own
dependency order and, for group B, in the packet's own three-commit split.

| Group | Tickets | Result |
|---|---|---|
| **A** | #1862, #1854 | five `noexcept` specifications and one `constexpr` dropped so five boundaries can reject bad input |
| **B** | #1865, #1858 | `,` means "group separator" to `Decimal`, `Single` and `Double` alike; an out-of-range magnitude saturates or overflows instead of being a format error |
| **C** | #1879, #1884 | the date/time parsers and both composite-format engines consume their whole input or fail |
| **D** | #1863 | `E`, `N` and `G` emit .NET's text |

**Seven findings → `remediated`**: SR-AUD-007 (with 007a), 009, 015, 029, 033,
035, 043. **CCF-005, CCF-007 and CCF-012 are complete.** Post-audit tally
**59 → 67 remediated, 305 → 297 open, of 364**; numbering frozen at 364 and no
new `SR-AUD-*` issued.

**Behaviour-incompatible by design in four places.** Only one of them is silent
to the caller in the dangerous direction — `Decimal::Parse("1,5")` was `1.5m` and
is now `15m`, with `Parse(",5")` moving from `0.5m` to `FormatException`. That
row landed as **its own commit with its own migration note**
(`docs/Migration-DecimalCommaGroupSeparator.md`) because §B.5 of the packet
required exactly that, so it can be reverted alone. The date/time and
composite-format changes reject rather than mis-answer, so the caller finds out;
the `ToString` change alters emitted text and needs golden files re-baselined.

**Eleven corrected premises**, all appended beside the original text rather than
edited over it. Two matter most, because in each the packet's stated .NET
behaviour is simply wrong: `.NET`'s `ParseFraction` **accepts** `".1234567"`, and
its `ParseTimeZone` **accepts** `"+2:5"` and reads it as 2h05m — 125 minutes,
exactly what this port already produced, so §C.4's "wrong answer that survives
round-tripping" was not wrong at all. Both rejections were implemented as
approved and are recorded as deliberate **narrowings** of the port's fixed-width
subset — the same subset that has always rejected `"2024-6-15"` — with the
widening question filed as inactive **#1929**. Two more inactive tickets,
**#1927** and **#1928**, hold value and message divergences found while
implementing #1862 and deliberately not absorbed into it.

**Nothing moved in the representation.** No parameter list, return type, object
layout, `sizeof`/`alignof`, member offset, virtual function, vtable slot or
mangled name changed across all seven tickets. Four new private headers under
`modules/core/include/System/detail/` hold the shared grammar and formatting
helpers, so the paired types cannot drift apart. Gate **14,920 → 14,998 across
37 executables**; Doxygen 1,937/1,942; graph 41/91; seams 2/18; fixtures 10/74 —
all unchanged. ASan and UBSan over 267 probe cases, before and after: zero
diagnostics, every answer identical to the plain build, which restates rather
than claims coverage — **no sanitizer can see a missing argument check, an
over-permissive grammar, an exception taxonomy or emitted text.**

Records: `docs/ConversionBoundaryFamilyPlan.md` §19.6,
`docs/FloatingValueFidelityPlan.md` §18, `docs/DecimalBoundaryFamilyPlan.md` §12,
`docs/DateTimeValidationBoundaryPlan.md` §20.3,
`docs/CompositeFormatBoundaryPlan.md` §21.


## 2026-07-31 — #1919 Collections public-representation containers (#1921–#1924)

Branch `feature/remediation-batch-1919-collections-comparison`. The
approval-blocked half of #1912, approved in the exact words of
`docs/CollectionsComparisonContractPlan.md` §10 and delivered as four bounded
tickets. **#1912 and the CCF-010 `Collections` continuation are now closed.**

- **#1921 — `SortedSet<T>`.** `Add(NaN); Add(1); Add(2)` left `Count` **1**
  holding `[NaN]`; every element added after a NaN was silently discarded,
  because `std::less<double>` violated `[associative.reqmts]` for the
  container's whole lifetime.
- **#1922 — `Dictionary<K,V>`, `HashSet<T>`.** A NaN key was accepted without
  limit and then unfindable forever, and did not survive a rehash.
- **#1923 — `FrozenSet`, `FrozenDictionary`, `ReadOnlySet`,
  `ReadOnlyDictionary`.** The projections now agree with their sources;
  `ReadOnlySet<double>.SetEquals(*this)` answered **false** before.
- **#1924 — evidence and closure.** 19 mutations (9 killed, 6 rejected at
  compile time, 2 controls and 2 declared *equivalents* survived), 57
  `sizeof`/`alignof` readings with **0** changed, a symbol inventory in which
  **no symbol moved for any non-floating instantiation**, 7 benchmark rounds,
  and `docs/RemainingApprovalDecisions.md`.

**Two of §10's own premises were wrong and are corrected additively:**
`SortedSet`'s `SetIterator`/`comparer()` are **private**, so the split is
11 / 6 rather than 10 / 7; and the `double`/`float` iterator typedefs **did not
change** — only `long double`'s did, because its hash-code cache is switched
off.

**New, not members of #1912:** #1925 (a nullable/composite floating key keeps
raw IEEE equality) and #1926 (`long double` hashed insertion 1.300× slower).

Gate **14,890 → 14,920** across 37 executables. Module graph 41/91, seams 2/18,
negative fixtures **9/66 → 10/74**, Doxygen 1,937 of 1,942, `local_ci_check.sh`
passed. No `SR-AUD-*` issued; numbering frozen at **364**. Maximum aggregate
compilation parallelism **2 jobs**.


## 2026-07-31 — #1912 Collections default comparison contract (7 of 8 tickets)

Branch `feature/remediation-batch-1912-collections-comparers`. The `Collections`
continuation of CCF-010, recorded as ticket **#1912** by CCF-010's own §18a.
**No `SR-AUD-*` identifier issued; audit numbering frozen at 364.**

- **#1913** (design) — `docs/CollectionsComparisonContractPlan.md`, 15 sections,
  from a 74-case probe run one case per process in four build flavours.
- **#1914** — the six *named* default comparers (`Generic::Comparer<T>::Default`,
  `EqualityComparer<T>::Default`, `ObjectComparer`, `ObjectEqualityComparer`,
  `NullableComparer`, `NullableEqualityComparer`), which #1912 did not name and
  which are the port's own `Comparer<T>.Default`. `Compare(NaN, x)` returned 0
  for every `x`, making the object an invalid comparator in its own right.
- **#1915** — six default-ordering sites (`List::Sort`, `List::BinarySearch`,
  `ImmutableList::Sort` ×2, `ImmutableArray::Sort`, `ImmutableList::BinarySearch`).
  The 196-shape sweep went from 164 corrupted shapes / 216,078,912 worst-case
  inversions to 0/0.
- **#1916** — 38 sequence equality sites across 9 headers.
- **#1917** — 16 associative value-equality sites across 7 headers.
- **#1918** — ten containers whose backing comparator is private or a
  `std::function`, at measured zero representation cost.
- **#1920** — 12-mutation matrix (10 killed, 2 declared negative controls
  survived), paired performance with three regressions found and removed, the
  additive §18b correction to `docs/ComparisonContractPlan.md`, closure.
- **#1919 — `blocked`.** `SortedSet`, `Dictionary`, `HashSet`, `FrozenSet`,
  `FrozenDictionary`, `ReadOnlySet`, `ReadOnlyDictionary`: same defect, but each
  names its backing `std::` container in its public surface. Design complete;
  exact approval wording in the plan's §10.

Tooling: `scripts/check_selective_components.sh` and `scripts/local_ci_check.sh`
now honour `SHARP_RUNTIME_BUILD_JOBS` (default 3, rejected above 3) so a session
required to run below CLAUDE.md's ceiling can use them; the maximum is unchanged.

Gate **14,815 → 14,890** across 37 executables. Module graph 41/91, Doxygen
1,941 (ceiling 1,942), negative fixtures 9/66, seams 2/18. Maximum compilation
parallelism **2 jobs** (user lowered the ceiling mid-batch).

## Sources of truth

Planning is deliberately split:

- `plan.md` is the versioned roadmap and milestone index.
- `NEXT.md` is the current cold-start handoff and ordered list of bounded next
  tasks.
- `plan.sqlite3` is the local, git-ignored detailed database:
  - `task` classifies .NET types.
  - `ticket` records concrete stabilization and architecture work.
- `CLAUDE.md` defines non-negotiable implementation and validation rules.
- `prompt.md` defines the database workflow.

The old namespace-table workflow and `plan_namespaces.md` were retired and
removed in commit `528d9ab7`. `plan_files.md` was referenced historically but
was never created. Neither file should be linked as current documentation.

## Current measured state

### Code and validation

- Native Linux/GCC build: zero errors and zero warnings.
- Tests: **14,514** passing across 36 component binaries plus one integration
  binary, verified 2026-07-30 by the CCF-002 batch on branch
  `feature/remediation-batch-ccf002-datetime-validation` (design #1876; #1877
  SR-AUD-006 +32, #1878 SR-AUD-007a +6 — total +38), raised from the 14,476
  verified by the CCF-014 + CCF-016 batch on branch
  `feature/remediation-batch-ccf014-ccf016` (design #1871 and #1873; #1872
  SR-AUD-075 + SR-AUD-085 +14, #1874 SR-AUD-093/094/095/096/100 +18 — total +32
  over the 14,444 floor, closing **two** cross-cutting causes), itself raised
  from the 14,444 verified by the CCF-011 empty-callable batch on branch
  `feature/remediation-batch-empty-callable` (design #1866; #1867 SR-AUD-065 +
  SR-AUD-099 +14, #1868 SR-AUD-058 + SR-AUD-121 +13, #1869 SR-AUD-052 +13,
  #1870 SR-AUD-134 +8 — total +48 over the 14,396 floor the CCF-007 Pi-trig +
  parse-whitespace batch left, which itself reached 14,396 via #1861 +20 and
  #1864 +8 over the 14,368 the CCF-005 Decimal slice verified via #1855 +9,
  #1856 +6, #1857 +4, #1859 +3 and #1860 +2 over 14,344), itself raised from
  the 14,344 verified by the CCF-005 conversion/memory-safety +
  CCF-006 float-format batch on branch
  `feature/remediation-batch-ccf005-convert-decimal` (#1851 SR-AUD-041 +46,
  #1852 SR-AUD-043a +12, #1853 SR-AUD-026/027 +41, #1849 SR-AUD-021 float slice
  +12 — total +111 over the 14,233 floor the CCF-003-close/CCF-005-plan batch
  left), itself raised from the 14,199 the numeric-wrapper namespace-review batch
  verified (#1804 closed a seam-checker discovery gap; #1843 fixed UInt128 shift
  UB, +3 tests), raised from the 14,196 the CCF-004 / Stream-capability batch
  verified (#1836/#1837/#1842/#1838/#1824/#1828/#1827), itself raised from the
  14,070 verified by ticket #1817 (SR-AUD-079, the canonical final-quantum
  rule) through the full repository gate, raised from the 14,002 verified by
  ticket #1816 (SR-AUD-078 / CCF-013, the in-place Base64
  encoders' write order), itself raised from the
  13,994 verified by ticket #1814 (SR-AUD-236,
  `HttpContentJsonExtensions`'s null content), itself raised from the 13,987
  verified by ticket #1812 (SR-AUD-242, `ZipArchive`'s null stream), itself
  raised from the 13,979
  verified by ticket #1811 (SR-AUD-257, the compression streams' null
  inner stream), itself raised from the 13,970
  verified by ticket #1810 (SR-AUD-132, the interpolated-string
  handler's raw destination pointer), itself raised
  from the 13,958
  verified by ticket #1807 (SR-AUD-097, AggregateException's null inner
  exception_ptr), itself raised from the 13,948
  verified by ticket #1806 (SR-AUD-338, the text stream wrappers'
  null base stream), itself raised from the 13,937
  verified by ticket #1805 (SR-AUD-341, the MemoryStream raw-buffer
  constructor), itself raised from the 13,923
  verified by ticket #1789 from a fully fresh configuration and a
  clean-first rebuild -- which the BitArray::Enumerator object-layout change
  made mandatory rather than merely prudent, exactly as #1788's LinkedList<T>
  one did -- itself raised from the 13,880 verified by ticket #1788, from the 13,840
  verified by
  ticket #1791, itself raised from the 13,790 verified by ticket #1802 and
  re-measured by ticket #1800. #1800 moved test code between files without
  adding or removing a case, so the figure was unchanged rather than stale at
  that point. (The 12,991 figure this line once carried was a stale relic; each
  remediation ticket's own section below states the count it measured.)
- Component graph: 41 physical modules and 91 direct production edges. The
  ninety-first was added by ticket #1814: `Net.Http.Json` now declares the
  `Core.Base` public dependency its header needs to throw
  `ArgumentNullException`, an edge it previously took only transitively through
  `Net.Http`.
- Boundary validator: no cycles, duplicate public include paths, orphan
  files, unresolved includes, undeclared edges, stale edges, or visibility
  mismatches.
- Dependency allow-list: empty.
- Selective matrix: all ten positive consumers pass. The Text.Json target
  absence assertion verifies that neither `Threading` nor `TimeZone` is
  configured, and negative include-leakage fixtures remain rejected.
- Tracked CI: Ubuntu selective matrix (nine of the ten local fixtures), full
  compatibility build, and a pinned Ubuntu 24.04 Doxygen-warning-baseline job
  in `.github/workflows/components.yml`. The missing direct
  `Collections.Blocking` consumer is `SR-AUD-001`.
- Doxygen 1.9.8: 1,941 current warnings against a 1,942-warning ceiling.
  `scripts/check_doxygen_warnings.sh` enforces that ceiling; lower counts are
  accepted and a Doxygen upgrade requires a deliberate re-baseline.

### Local planning database

The 2026-07-29 local snapshot contains:

| Table | State |
|---|---|
| `task` | 16,201 rows: 1,082 `ported`, 140 `ignore`, 14,979 legacy `ignored`; no unclassified or `tobedecided` rows |
| `ticket` | 1,804 rows: 1,801 `done` — including audit ticket #1766, post-audit tickets #1767, #1768, #1769, #1770, and #1771, follow-up correction ticket #1774 (`REMED-COLL-COPYTO-EMPTY-SPAN`), ticket #1775 (`REMED-COLL-HASHTABLE-VIEWS`), ticket #1776 (`REMED-CORE-ARGNULL-MESSAGE`), ticket #1777 (`REMED-COLL-COPYTO-DOC-SYNC`), ticket #1778 (`REMED-COLL-CONCURRENTDICT-ADDORUPDATE`), ticket #1779 (`REMED-COLL-READONLYDICT-EMPTY-DESIGN`), ticket #1780 (`REMED-COLL-READONLYDICT-EMPTY`), ticket #1781 (`REMED-DOCS-DOXYGEN-COUNT-RECONCILE`), ticket #1782 (`REMED-COLL-SORTEDSET-VIEW-DESIGN`), ticket #1783 (`REMED-COLL-SORTEDSET-LIVE-VIEW`), ticket #1784 (`REMED-COLL-SORTEDSET-VIEW-COUNT-RACE`), ticket #1786 (`REMED-COLL-VERSION-COUNTER-OVERFLOW`), and ticket #1787 (`REMED-COLL-VERSION-COUNTER-OVERFLOW-SWEEP`) design ticket #1790 (`REMED-COLL-LIST-INDEXER-VERSION`), design ticket #1792 (`REMED-COLL-ENUMERATOR-CURRENT-CONSTCAST`), implementation ticket #1793 (`REMED-COLL-IENUMERATOR-CURRENT-SAFETY-IMPLEMENT`), and design ticket #1785 (`REMED-COLL-SORTEDSET-NESTED-EXCEPTION-ORDER`, opened inactive by #1784 and closed by adopting .NET's nested-view validation order), design ticket #1795 (`REMED-COLL-IDICTENUM-KEYVALUE-SAFETY-DESIGN`, opened because #1794 is an implementation row and was deliberately not reused), and implementation ticket #1794 (`REMED-COLL-IDICTENUM-KEYVALUE-SAFETY`, which landed #1795's design under the full four-item approval: owning `std::any` Key/Value, a mandatory `MoveNext`-time snapshot on both implementations, two `ListDictionaryInternal` parity corrections, and an acknowledged silent ABI break through two independent mechanisms) — one `wontfix` (#1772, obsoleted by #1771), two deliberately inactive `blocked` rows (#1773, the out-of-repository CNA / mobile-eggbert `CopyTo` sweep; and, since 2026-07-29, #1804 `REMED-TOOLING-SEAM-DISCOVERY-VACUITY`, opened inactive by #1803 because `scripts/check_version_seam_odr.py` exits 0 when a seam *leaves* its discovery rule — measured, covered by the two consumer fixtures, and not a defect today. **This clause previously named #1803 `REMED-TOOLING-SORTEDSET-SEAM-NEGATIVE-FIXTURE` as the second inactive row, for the one seam — `SortedSetVersionAccess` — whose *consumer-side* unreachability had no negative fixture; #1803 is now `done`, having added `test/consumer/collections_sorted_set_version_negative.cpp` (15 sites, 9 fixtures / 66 sites in total) with no production change, and the count is taken from the database on each edit**; **this clause said "five" and still listed #1791 and #1788 as blocked until ticket #1788 corrected it: #1791 was closed earlier the same day and #1788 closed itself, so two of the five were already stale when written**; implementation ticket #1788 (`REMED-COLL-LINKEDLIST-VERSION-WIDEN`) is now `done`, having widened `LinkedList<T>`'s mutation counter and its enumerator's snapshot to 64 bits under the explicit approval that `sizeof(LinkedList<T>)` may grow 40 → 48 on LP64, with a measured silent binary break and a mandatory full consumer rebuild; implementation ticket #1789 (`REMED-COLL-BITARRAY-VERSION-WIDEN`) is likewise now `done`, having done the same for `BitArray` under its own separate approval that `sizeof(BitArray::Enumerator)` may grow 32 → 40 on LP64 while `sizeof(BitArray)` stayed 48, so **no collection retains a 2^32 enumerator-snapshot ABA horizon and `detail::NarrowMutationCounter` has no user left** — this clause listed #1789 among the inactive `blocked` rows and said "three" until #1789 itself corrected it to two, the count being taken from the database on each edit; **all three rows #1799 opened inactive are now closed**: #1802 `REMED-COLL-HASHTABLE-REMOVE-VERSION`, #1800 `REMED-COLL-VERSION-SEAM-ODR` and #1801 `REMED-TOOLING-NEGATIVE-FIXTURE-CI`, see below; **this line said "eight" while listing seven until #1802 corrected it, and the count is taken from the database on each edit**); design ticket #1797 (`REMED-COLL-HASHTABLE-VALUE-ACCESS-DESIGN`, opened because #1796 is an implementation row and was deliberately not reused), design ticket #1799 (`REMED-COLL-LISTDICT-SETITEM-DESIGN`, opened for the same reason against #1798 and closed 2026-07-29 with no production change), implementation ticket #1798 (`REMED-COLL-LISTDICTINTERNAL-PARITY`, which landed #1799's design under the full three-item approval: a private `ValidatedKey` making null-key rejection structurally unskippable across all five raw-key entry points, one `setItem` upsert whose bump follows the mutation and covers replacement and equal-value replacement, deletion of the `const_cast` that made the key view's `CopyTo` publish a writable pointer to a caller's `const` object, two deliberate deviations from .NET's bump-first shape on a throwing `Add` and an absent `Remove`, and an acknowledged **silent** stale-object hazard requiring a full consumer rebuild), and implementation ticket #1796 (`REMED-COLL-HASHTABLE-WRITE-ESCAPES`, which landed #1797's design under the full four-item approval: owning `std::any` from `getItem`/`at`/the `const` indexer, a non-copyable `ValueReference` proxy making `table[key] = value` a tracked insert-or-replace and a bare read no longer insert, `KeyNotFoundException` in place of `std::out_of_range`, and an acknowledged silent ABI break requiring a full consumer rebuild) are both `done`, as is tooling ticket #1801 (`REMED-TOOLING-NEGATIVE-FIXTURE-CI`, which made all **seven** — not six — negative consumer fixtures compile per site from `scripts/local_ci_check.sh`, 37 sites, after reproducing the whole-file false pass and proving the checker against a 7/7 mutation campaign; no production source, signature, symbol or layout changed); no `todo`, `doing`, or `needs_user` rows |

Because `plan.sqlite3` is git-ignored, these counts describe the maintainer
snapshot, not data shipped in a fresh clone.

## P0 completed: Collections blocking isolation

Ticket #1737 resolved the post-modular closure regression caused by
`BlockingCollection.hpp`. It created the physical `Collections.Blocking`
component, moved that header and its eight dedicated tests there, and declared
the direct public dependencies `Collections.Core`, `Core.Base`, and
`Threading`. `Collections.Core` now depends publicly only on `Core.Base` and
the `Collections` compatibility umbrella includes all four collection
components.

The repair restores lean closures for `Text.Json`, `Net.Http.Headers`,
`Net.Mime`, and `Numerics`: none configures `Threading` or `TimeZone` unless a
requested component actually requires them. The Text.Json negative assertion
and a direct `Collections.Blocking` consumer fixture guard this result. Do not
move `BlockingCollection<T>` back to `Collections.Core` or weaken that
assertion without an explicit architecture decision.

## Completed milestones

### Porting and stabilization

- Classified the indexed .NET type surface and completed the original
  porting/stabilization queue.
- Established fixed-width public API aliases, property/indexer naming, SPDX
  headers, .NET-reference review, and regression-test requirements.
- Completed native TSan, ASan, and UBSan passes during stabilization and
  fixed the production findings discovered by those runs.
- Revalidated `ConcurrentBag`, `BlockingCollection`, `ConditionalWeakTable`,
  generic task continuations, and `TaskExtensions::Unwrap` with focused TSan
  and ASan/LSan scenarios under ticket #1742.
- Added `ImmutableList<T>` predicate/action queries (`ForEach`, `Exists`,
  `Find*`, and `TrueForAll`) under ticket #1743, including empty-delegate,
  empty-list, ordering, and immutability regression coverage.
- Added default full-list `ImmutableList<T>::Sort` and `Reverse` under ticket
  #1745, leaving range and custom-comparer overloads explicitly deferred.
- Added `ImmutableList<T>::GetRange` under ticket #1746, including exact
  boundary and invalid-range regression coverage.
- Added `ImmutableList<T>::ConvertAll<TOutput>` under ticket #1747, including
  empty-source and empty-converter regression coverage.
- Added seekable `BinaryReader::PeekChar` under ticket #1744. It returns the
  next UTF-8 character or EOF without advancing, restores the position after
  decode failure, and explicitly rejects non-seekable streams rather than
  pretending a general decoder buffer exists.
- Added `BinaryReader::ReadChars` and `Read(char[])` under ticket #1748. They
  preserve UTF-8 decoder output across calls, return a partial result at clean
  EOF, propagate truncated input, and expose supplementary scalars as UTF-16
  surrogate pairs.
- Added all three `ImmutableList<T>::CopyTo` overloads under ticket #1749,
  using checked fixed-size `std::vector` destinations, including source range
  and destination offset handling without signed-overflow-prone bounds checks.
- Added `ImmutableList<T>::Sort(Comparison<T>)` under ticket #1750. It follows
  the established signed comparison-delegate convention, rejects an empty
  delegate, and returns an independently backed sorted list.
- Added `ImmutableList<T>::Reverse(index, count)` under ticket #1751. It
  reverses only a valid subrange in an independently backed list and reuses
  the established zero-length-boundary and invalid-range contract.
- Added full-list and range `ImmutableList<T>::Sort(IComparer<T>)` overloads
  under ticket #1752. They use the established generic comparer interface and
  retain the full-list immutability and range-boundary contracts.
- Added default and `IEqualityComparer<T>` item mutations for
  `ImmutableList<T>` under ticket #1753: `Remove`, vector `RemoveRange`, and
  `Replace`. Removals process inputs sequentially, sources remain immutable,
  and a missing old value now throws `ArgumentException` as required.
- Added default and `IEqualityComparer<T>` range queries for
  `ImmutableList<T>` under ticket #1754: `IndexOf` and `LastIndexOf`. The
  forward and backward range contracts are validated separately, including the
  valid empty `LastIndexOf(..., 0, 0, ...)` case.
- Added full-list and range `ImmutableList<T>::BinarySearch(IComparer<T>)`
  overloads under ticket #1755. They respect custom ordering and return the
  complement of the absolute insertion point for a missing value.
- Added default-comparison `ImmutableList<T>::Sort(index, count)` under
  ticket #1756. It sorts only a valid range, preserves the source and outside
  elements, and accepts zero-length boundary ranges.
- Added `BigInteger` bitwise AND, OR, XOR, complement, and compound assignment
  operators under ticket #1757. The base-10^9 backing converts internally to
  a sign-extended two's-complement form so negative and large operands retain
  .NET semantics.
- Added signed `BigInteger` left/right shifts and compound assignments under
  ticket #1758. Negative counts reverse direction, while right shifts retain
  arithmetic floor semantics for negative values.
- Added `BigInteger` byte-vector construction and serialization under ticket
  #1759. The default is signed little-endian two's complement; callers can
  select unsigned and/or big-endian conversion, and output is minimal.
- Added the core `ImmutableList<T>::Builder` workflow under ticket #1760:
  `CreateBuilder`, `ToBuilder`, checked mutable mutations, and independent
  `ToImmutable` snapshots. Its vector-backed implementation intentionally
  copies rather than claiming the tree-backed .NET conversion complexity.
- Completed `UTF7Encoding` under ticket #1761 with RFC 2152 modified-Base64
  shifts over UTF-16BE units, optional-direct-character control, astral
  Unicode support, and U+FFFD recovery for malformed input. It remains
  obsolete and unsuitable for new protocols.
- Added conditional `Trace::WriteIf` and `Trace::WriteLineIf` under ticket
  #1762. They suppress output when false and retain the existing stderr
  write/newline behavior when true.
- Added explicit ProcessStartInfo child environment overrides under ticket
  #1763. They are validated before fork, passed only to the child, and retain
  inherited values not explicitly overridden.
- Added synchronous POSIX Process startup failure reporting under ticket #1764.
  Child setup and exec errors now reach Start() with the executable name and
  native error text instead of appearing later as exit code 127.
- Established the Doxygen warning baseline under ticket #1765: Doxygen 1.9.8
  emits 1,942 warnings. The dedicated check permits incremental reductions,
  rejects regressions, and avoids a mass comment-only rewrite.
- Remediated SR-AUD-356 and SR-AUD-364 / CCF-018 under ticket #1767. A shared
  lifecycle state protects `Current` across ten collection enumerators;
  BitArray additionally detects every mutation. Thirteen permanent regressions,
  the direct ASan/UBSan probe, 1,435 Collections.Core tests, and the
  network-permitted 12,694-test repository gate pass; Doxygen remains below
  its ceiling at 1,941/1,942.
- Remediated SR-AUD-357 / CCF-019 under design ticket #1768 and implementation
  ticket #1769. `LinkedListNode<T>` now refers to an independently allocated,
  reference-counted node with an explicit null/detached/attached state, so
  removal, `Clear`, and destruction of the owning `LinkedList<T>` detach the
  node and retain its value instead of leaving a dangling `std::list` iterator.
  The repair also added the .NET existing-node insertion overloads, the
  detached-node constructor, `Value` setter, `List` accessor, node identity
  comparison, defined list copy/move semantics, and a bidirectional
  `LinkedList<T>::iterator`. Forty-nine permanent regressions, a clean
  ASan/UBSan/LeakSanitizer probe, a `-Werror` standalone `Collections.Core`
  consumer fixture, 1,484 Collections.Core tests, and the network-permitted
  12,743-test repository gate pass; Doxygen stays at 1,941/1,942. The contract
  is recorded in [`docs/LinkedListNodeLifetime.md`](docs/LinkedListNodeLifetime.md).
- Completed the SR-AUD-358 / CCF-020 raw-`CopyTo` design under ticket #1770, a
  design-only ticket that changed no production or test source. The selected
  contract — a length-aware, statically typed `System::Span<std::any>`
  destination behind a non-virtual `ICollection`, so capacity and element type
  are validated exactly once before any implementation writes — is recorded in
  [`docs/ICollectionCopyToDesign.md`](docs/ICollectionCopyToDesign.md). Seven
  repository-local probes back it, including a clean ASan/UBSan/LeakSanitizer
  `-Werror` prototype and a re-verification that the current boundary still
  produces three sanitizer aborts plus one silent, LeakSanitizer-only
  element-type corruption. SR-AUD-358 stays `confirmed`; implementation is
  inactive ticket #1771, gated on explicit approval of the narrow public-API
  break.
- Remediated SR-AUD-358 / CCF-020 under implementation ticket #1771, after the
  user explicitly approved the public source- and ABI-breaking change.
  `CopyTo(void*, intcs)` is removed from `System::Collections::ICollection` and
  replaced by non-virtual, validating `CopyTo(ObjectSpan, intcs)` and
  `CopyTo(std::vector<std::any>&, intcs)` over one protected pure virtual
  `copyToCore(ObjectSpan, intcs)` hook, with checked typed
  `std::vector<void*>` / `std::vector<DictionaryEntry>` overloads on the
  concrete collections and `using ICollection::CopyTo;` where one is added. No
  deprecated shim was retained, so a stale call site is a compile error naming
  the replacement rather than a run-time throw; because a pure virtual member
  was removed, every consumer must rebuild. 128 permanent regressions across
  every implementation, a clean ASan/UBSan/LeakSanitizer probe and suite run, a
  `-Werror` standalone `Collections.Core` consumer fixture, 1,612
  Collections.Core tests, and the 12,871-test repository gate pass; Doxygen
  stays at 1,942/1,942 -- the new README link to the migration document
  produces the same unresolved-markdown-link warning that every other README
  documentation link already produces, offsetting one warning removed from
  ICollection.hpp. Consumer guidance is in
  [`docs/Migration-ICollectionCopyTo.md`](docs/Migration-ICollectionCopyTo.md).
- Corrected a follow-on defect in #1771's own validation rule under ticket
  #1774: `detail::requireValidCopyDestination` had rejected every null-pointer
  destination outright, including a valid empty `ObjectSpan{nullptr, 0}` or a
  default-constructed empty `std::vector<std::any>` copied from an empty
  collection. The rule now rejects a null pointer only when paired with a
  positive length; a non-empty collection copied into a zero-length
  destination still fails, but on capacity, not nullness. SR-AUD-358 remains
  `remediated`. `CopyToBoundaryTests.cpp` grew to 1,662 Collections.Core tests
  and a new standalone probe
  (`build-probe-copyto/probe10_empty_span_correction.cpp`) is clean under
  ASan/UBSan/LeakSanitizer; the 12,921-test repository gate passes with zero
  build warnings/errors and Doxygen stays at 1,942/1,942. Recorded in section 22
  of [`docs/ICollectionCopyToDesign.md`](docs/ICollectionCopyToDesign.md).
- Added consumer-driven coverage across core, collections, IO, networking,
  threading/tasks, text/JSON, XML, numerics, globalization, and cryptographic
  hashing/random APIs.

### Platform work

- MinGW library cross-build audit completed under ticket #40; its post-modular
  `All` and selective `Text.Json` library revalidation passed with MinGW-w64
  GCC 14-win32 and CMake 3.31.6 under ticket #1741.
- Emscripten's earlier library audit is ticket #41. Its post-modular `All` and
  selective `Text.Json` revalidation passed with Emscripten 5.0.7 and CMake
  3.31.6 under ticket #1741, which also corrected an Emscripten-only unused
  `WebProxy` DNS helper warning promoted by `-Werror`.
- Both cross-build validations are compile evidence only; GoogleTest/runtime
  tests were not cross-built or run.
- Real downstream Apple Clang/Xcode 15.4 builds drove the portability fixes
  in commits `1d22a7b2` through `b797928f`.

These library builds use the final 41-component architecture. They are
evidence of portability, not a current cross-platform test matrix.

### Modular architecture

The remediation plan MOD-001 through MOD-008 was implemented in
`b0e944ad`, documented in `27e4d680`, and closed as tickets 1729–1736.
It delivered:

- One physical owner for every production header, source, and module test.
- Explicit public, private, and test-only dependency visibility.
- Narrow `Core.Base`, `Collections.Core`, `Collections.Blocking`,
  `Collections.Async`, and `Collections.ObjectModel` targets while preserving
  compatibility umbrellas.
- Component-scoped test executables and a separate integration executable.
- Automated boundary validation, catalogue generation, isolated consumers,
  negative fixtures, and GitHub Actions coverage.
- Generated component documentation in `docs/ComponentCatalog.md`.

The graph landed with 85 production edges and has since grown to 90. The
validator, generated catalogue, native gate, and selective matrix are green.

### Post-modular API expansion

The first consumer-driven ports after modularization added:

- Runtime compiler-services helpers, including `ConditionalWeakTable` and
  `RuntimeHelpers`.
- Component-model notification, initialization, and async-completion
  metadata.
- HTTP handler/invoker/request-option primitives and web-proxy APIs.
- `ConcurrentBag` and `BlockingCollection`.
- `MemoryStream(buffer, size)` writability parity, while `BinaryData::ToStream`
  and read-mode ZIP entry streams retain their explicit read-only contracts.
- `TaskT<TResult>::ContinueWith` for action and result-producing callbacks,
  including terminal-state filtering, chaining, and weak-ownership teardown.
- `TaskExtensions::Unwrap` for generic and non-generic nested tasks.
- `XmlWriter::WriteWhitespace` and `XText::WriteTo`'s document-level
  whitespace distinction.
- XML schema exception types.

The verified test baseline grew from 12,494 at the modularization checkpoint
to 12,991, most recently through the post-audit remediation regressions.

## Completed repository audit

Ticket #1766 was a P1, evidence-only, repository-wide audit. It mirrors every
tracked first-party text-like source, test, build, CI, and relevant
documentation file under `audit/`, following the CNA audit format. Its scope,
exclusions, final manifest, findings index, and handoff state are maintained in
that directory. The audit was deliberately not a repair stream: confirmed
defects, missing assertions, weak diagnostics, and parity gaps become
evidence-backed follow-up tickets only after the manifest is reconciled.
The 2026-07-27 audit closure has all 1,748 of 1,748 mirrored reports complete
and three hundred sixty-four findings confirmed at closure;
`audit/AUDIT_FINAL_REPORT.md`
and `audit/AUDIT_PROGRESS.md` are the authoritative handoff. The final
142-file Collections shard passed 1,422/1,422 and adds SR-AUD-356 through
SR-AUD-364 for unsafe enumerator lifecycle, LinkedListNode lifetime, raw CopyTo
storage, mutable ReadOnlyDictionary.Empty, concurrent update loss, non-live
SortedSet views, FrozenDictionary duplicates, Hashtable contracts, and BitArray
enumeration. Final reconciliation built all configured targets and passed the
database/boundary/diff controls. Audit-only work made no production or
test-source changes; begin post-audit remediation only through separately
approved, bounded tickets.

## Candidate roadmap

The audit is complete. Ticket #1767 remediated SR-AUD-356 and SR-AUD-364 /
CCF-018, tickets #1768/#1769 remediated SR-AUD-357 / CCF-019, tickets
#1770/#1771 remediated SR-AUD-358 / CCF-020, and ticket #1775 remediated
SR-AUD-363. The findings index therefore
retains 364 original findings while recording 359 as open `confirmed` and five
as `remediated`. Post-audit remediation is the active priority; optional P2
breadth stays behind confirmed safety defects.

Design ticket #1768 selected the SR-AUD-357 / CCF-019 `LinkedListNode`
lifetime contract — independently allocated, reference-counted nodes with an
explicit null/detached/attached state model — and recorded it in
[`docs/LinkedListNodeLifetime.md`](docs/LinkedListNodeLifetime.md).
Implementation ticket #1769 (`REMED-COLL-LINKED-NODE`) completed it, taking the
index to 361 open `confirmed` findings and three `remediated` at that point.

Design ticket #1770 then answered SR-AUD-358 / CCF-020. It inventoried all six
`ICollection` implementations, the three test call sites, and the zero
production callers of the raw boundary; compared them with the current .NET
`ICollection.CopyTo(Array, int)` sources; and selected a length-aware, statically
typed `System::Span<std::any>` destination behind a non-virtual `ICollection`,
recorded in [`docs/ICollectionCopyToDesign.md`](docs/ICollectionCopyToDesign.md).
No production change was made under #1770, so SR-AUD-358 stayed `confirmed`
until implementation closed.

The user approved the narrow public-API break on 2026-07-27, and implementation
ticket #1771 (`REMED-COLL-COPYTO`) landed it: the pure virtual
`CopyTo(void*, intcs)` is removed from `System::Collections::ICollection`, so
SR-AUD-358 is now `remediated` and the index records 360 open findings and four
`remediated`. Cleanup ticket #1772 (`REMED-COLL-COPYTO-CLEANUP`) is `wontfix`:
both of its items had to be completed inside #1771, because the deprecated shim
it would have deleted was never created and the `Array.hpp` / `Buffer.hpp`
doc-comments citing `ArrayList::CopyTo(void*, int)` could not be left pointing at
a member that no longer exists. The only follow-up is inactive ticket #1773
(`REMED-COLL-COPYTO-DOWNSTREAM`, P2, size S), the CNA and mobile-eggbert sweep
described in [`docs/Migration-ICollectionCopyTo.md`](docs/Migration-ICollectionCopyTo.md);
neither repository is in this checkout, so nothing is claimed about their current
usage.

Follow-up ticket #1774 (`REMED-COLL-COPYTO-EMPTY-SPAN`, P1, size XS), opened and
closed 2026-07-27 on the same branch, then corrected a narrow defect found in
#1771's own validation rule: `detail::requireValidCopyDestination` rejected
every null-pointer destination outright, including a valid empty
`ObjectSpan{nullptr, 0}` or default-constructed empty `std::vector<std::any>`
copied from an empty collection. The rule now rejects a null pointer only when
the destination's length is positive; a non-empty collection copied into a
zero-length destination still fails, but on capacity, not nullness. SR-AUD-358
remains `remediated` — this did not reopen it. See
[`docs/ICollectionCopyToDesign.md`](docs/ICollectionCopyToDesign.md) section 22.

Ticket #1775 (`REMED-COLL-HASHTABLE-VIEWS`, P1, size M), opened and closed
2026-07-27 on local branch `feature/remediation-coll-hashtable-views`, then
remediated SR-AUD-363, taking the index to 359 open `confirmed` findings and
five `remediated`. `Hashtable::getKeysProperty()`/`getValuesProperty()`
returned `nullptr` although `IDictionary` documents each as returning an
`ICollection` over the keys/values, so a contract-following consumer
null-dereferenced (ASan-confirmed SEGV) while the sibling
`ListDictionaryInternal` answered identical caller code correctly; and the
raw-key entry points stringified a null key as the address text `"0"`, which
also aliased the ordinary string key `"0"`. Both properties now return a live,
caller-owned `MemberCollection` reusing the #1771/#1774 copy boundary, and
`toKey()` is the single validating conversion site every raw-key path passes
through. No public signature changed and no virtual member was added or
removed, so this is neither a source nor an ABI break. Evidence: 70 permanent
regressions parameterised over both non-generic `IDictionary` implementations,
clean under ASan + UBSan + LeakSanitizer; Collections.Core 1,732/1,732; a
`-Werror` standalone consumer fixture; and a 12,991/12,991 full gate.

Two separate pre-existing defects found during #1775 were recorded as inactive
tickets rather than folded in: #1776 (`System::ArgumentNullException(paramName)`
emits its `(Parameter 'x')` suffix twice) and #1777 (four typed `CopyTo`
doc-comments still described ticket #1771's superseded null-destination rule).
Both are now done; see below.

**Correction:** the sentence above and #1776's own opening notes described
neither as a new audit identifier under the frozen SR-AUD-001..364 numbering.
That was inaccurate for #1776: SR-AUD-089 (the null-`const char*` crash in the
same constructors) and SR-AUD-090 (the duplicate suffix itself) already
existed as `confirmed` findings within that frozen range. Ticket #1776
(`REMED-CORE-ARGNULL-MESSAGE`, P2, size XS), opened and closed 2026-07-27 on
local branch `feature/remediation-argument-null-message`, corrected
`ArgumentNullException(paramName)` to pass its raw default message straight to
the `ArgumentException(message, paramName)` base constructor — matching
.NET's own `ArgumentNullException(paramName) : base(SR.ArgumentNull_Generic,
paramName)` — so the `(Parameter 'x')` suffix is appended exactly once, and,
as a side effect of removing the unsafe local `makeMsg()` concatenation, a
null `paramName` no longer null-dereferences. `ArgumentException` and
`ArgumentOutOfRangeException` were unaffected and remain regression-tested as
such. SR-AUD-089 and SR-AUD-090 are now `remediated` in
`audit/AUDIT_FINDINGS_INDEX.md`. Evidence: 26 new permanent regressions across
`ArgumentNullExceptionTests.cpp`, `ArgumentExceptionTests.cpp`, and
`ArgumentOutOfRangeExceptionTests.cpp`; the two pre-existing exact-message
workarounds this defect forced (`DictionaryKeyAndViewContractTests.cpp` from
#1775, `LinkedListNodeLifetimeTests.cpp` from #1769) now assert the
single-suffix message directly; the `Core.Base` standalone consumer fixture
extended to cover throw/catch through `System::Exception`; and a full
`scripts/local_ci_check.sh build` gate. No public signature, virtual member,
or inheritance changed, so this is neither a source nor an ABI break.

Ticket #1777 (`REMED-COLL-COPYTO-DOC-SYNC`, P3, size XS), opened and closed
2026-07-27 on local branch `feature/remediation-copyto-docs`, then corrected
the four typed `CopyTo(std::vector<T>&, intcs)` doc-comments on `Hashtable`,
`Queue`, `Stack`, and `ListDictionaryInternal` that still cited #1771's
superseded rule (`ArgumentNullException` for any null-pointer destination).
Each now states the rule #1774 corrected: `ArgumentOutOfRangeException` for a
negative index, `ArgumentException` for insufficient capacity (including a
non-empty collection into a zero-length destination), and
`ArgumentNullException` only for a null pointer paired with a positive length.
A repository-wide search found no other current public header with the stale
text; `ICollection.hpp`, the design and migration documents, and `README.md`
were already corrected under #1774. Documentation only — no implementation,
test assertion, or public signature changed, so this is neither a source nor
an ABI break; SR-AUD-358 and CCF-020 remain `remediated` and ticket #1773
remains `blocked`. Evidence: the 225 focused `CopyTo` tests and the full
1,732-test `Collections.Core` suite are unchanged; the `-Werror` standalone
`test/consumer/collections_copyto.cpp` consumer fixture recompiles and runs
successfully; a full `scripts/local_ci_check.sh build` gate passed
13,017/13,017 tests across 37 executables with zero warnings/errors; and
Doxygen 1.9.8 stayed at exactly 1,942/1,942 — unchanged, at the ceiling.

Ticket #1778 (`REMED-COLL-CONCURRENTDICT-ADDORUPDATE`, P2, size S), opened and
closed 2026-07-27 on local branch
`feature/remediation-coll-concurrentdict-addorupdate`, then remediated
SR-AUD-360: `ConcurrentDictionary::AddOrUpdate` (both overloads) snapshotted
the existing value, ran the update factory outside the lock, then
unconditionally overwrote the entry with the factory's result even if another
thread had mutated or removed the entry meanwhile, silently discarding the
intervening write. Real .NET's `TryUpdateInternal` gates the commit on
`EqualityComparer<TValue>.Default.Equals` against the previously observed
value and retries (re-observes, re-invokes the factory) on a mismatch. Both
overloads now loop the same way, still never holding the internal mutex
across either factory call, and require `TValue::operator==` — the same
requirement `TryUpdate` on this class already carries. No public signature
changed and no virtual member was added or removed, so this is neither a
source nor an ABI break. Selected over the only other signature-compatible
candidate, SR-AUD-362 (`FrozenDictionary::Create` duplicate keys), after
checking SR-AUD-362 against the current .NET `FrozenDictionary.cs` source and
finding its premise does not hold: .NET's own doc-comment states
last-value-wins is the intended `Create`/`ToFrozenDictionary` behavior,
contrasted explicitly with `Enumerable.ToDictionary`'s throw-on-duplicate
behavior, and sharp-runtime's current implementation already matches it.
SR-AUD-362 was left untouched, not selected, and not reopened as a second
ticket; see `audit/AUDIT_FINAL_REPORT.md`'s planning-accuracy note.
SR-AUD-359 (`ReadOnlyDictionary::Empty`) and SR-AUD-361
(`SortedSet::GetViewBetween`) were set aside per NEXT.md's own note that they
may need a public-surface design decision. Evidence: a deterministic
coordinated-thread pre-fix reproduction (gitignored
`build-probe-concurrentdict/probe1_lost_update.cpp`) matching the finding's
own `add-or-update-result=1 final=1` symptom, clean post-fix under
ASan+UBSan+ThreadSanitizer plus a 16-thread/32,000-operation TSan stress
probe; 4 new permanent regressions in `ConcurrentDictionaryTests.cpp`;
`Collections.Core` 1,736/1,736 (was 1,732); and a full
`scripts/local_ci_check.sh build` gate of 13,021/13,021 tests across 37
executables with zero warnings/errors (was 13,017).

Design ticket #1779 (`REMED-COLL-READONLYDICT-EMPTY-DESIGN`, P2, size S),
opened and closed 2026-07-27 on local branch
`feature/remediation-coll-readonlydict-empty-design`, then answered SR-AUD-359,
selected over SR-AUD-361 after comparing both in detail. `ReadOnlyDictionary
<K,V>::Empty()` returns a non-`const` reference to a process-wide `static`
singleton; because the class relies on its compiler-generated copy assignment
operator, ordinary assignment through that reference silently rebinds the
singleton's private backing map for the remainder of the process. .NET's own
`Empty` is a get-only auto-property with no setter, so this is a C++-port-only
hazard, not a parity gap. SR-AUD-361 (`SortedSet::GetViewBetween`) would
instead require replacing `SortedSet<T>`'s `std::set` backing with a
tree structure supporting live, bounded, write-through sub-range views —
.NET's own `TreeSubSet` nested class is 378 lines — before any bounded
implementation ticket could be written, so it was left `confirmed` and not
selected. Recorded in
[`docs/ReadOnlyDictionaryEmptyDesign.md`](docs/ReadOnlyDictionaryEmptyDesign.md):
change `Empty()`'s return type to `const ReadOnlyDictionary<K,V>&`, the
literal C++ expression of ".NET has no setter." This is a public signature
change, so per the same approval boundary ticket #1770/#1771 used, it requires
explicit user approval; implementation was proposed as inactive ticket **#1780**
(`REMED-COLL-READONLYDICT-EMPTY`, P2, size XS), `blocked` on that approval. No
production or test source changed under #1779. Evidence: three repository-local
ASan/UBSan-clean probes in the gitignored `build-probe-readonlydict/` tree
independently reproducing the audit's `empty-before=0`/
`empty-after-assignment=1` symptom (plus confirming the contamination is
visible process-wide, not local to one call site) and proving the proposed fix
both rejects the hazardous assignment at compile time and preserves every
existing observable behavior; the existing 17 `ReadOnlyDictionary` regression
tests rerun unchanged; and a full `scripts/local_ci_check.sh build` gate of
13,021/13,021 tests across 37 executables, zero warnings/errors (unchanged,
since no production/test source changed).

Ticket #1780 (`REMED-COLL-READONLYDICT-EMPTY`, P2, size XS), opened and closed
2026-07-27 on local branch `feature/remediation-coll-readonlydict-empty`, then
implemented #1779's design after the user explicitly approved the public
return-type change, remediating SR-AUD-359: `Empty()`'s declared return type
changed from `ReadOnlyDictionary<K, V>&` to `const ReadOnlyDictionary<K, V>&`,
so assignment through its result is now a compile error instead of a silent,
process-wide rebind of the shared empty singleton's backing map. No other
member, constructor, or the class's copy/move assignment operators changed, so
ordinary, non-singleton instances remain freely assignable exactly as before;
no virtual member was added or removed (the class has none). This is a
source-breaking change only for the exact hazardous pattern of declaring an
explicit non-`const` reference to `Empty()`'s result or assigning through
it — confirmed absent everywhere in this repository — and not an ABI break: a
direct `nm`/`c++filt` comparison of the mangled `Empty()` symbol before and
after the change shows byte-identical names (the Itanium C++ ABI does not
encode a function's return type in its mangled name), and `Collections.Core`
is a header-only `INTERFACE` CMake target with no exported archive. Evidence:
pre-fix reproduction re-ran the design phase's own gitignored
`build-probe-readonlydict/probe1_mutable_empty.cpp` against the
still-unmodified production header, reconfirming the exact hazard; two new
post-fix probes compiled directly against the real, now-modified header (not a
copy) show the hazardous assignment now fails with `error: passing 'const
ReadOnlyDictionary<...>' as 'this' argument discards qualifiers`, while a
companion behavior-preservation probe runs clean under ASan+UBSan; two new
permanent regressions in `ObjectModelTests.cpp::ReadOnlyDictionaryTests` (a
`static_assert` pinning the exact return type, and
`Empty_RemainsEmptyAfterConstructingUnrelatedInstances`), with
`Empty_IsEmptyAndCached` retained verbatim; a new standalone
`test/consumer/collections_object_model_readonlydictionary.cpp` positive
fixture compiling `-Werror` and running successfully, plus a companion
`test/consumer/collections_object_model_readonlydictionary_negative.cpp`
negative fixture that fails to compile with the same diagnostic through the
repository's own consumer-fixture harness; `SharpRuntimeTests_Collections_ObjectModel`
grew from 124/124 to 125/125; and a full `scripts/local_ci_check.sh build` gate
of 13,022/13,022 tests across 37 executables, zero warnings/errors (was
13,021). Module boundaries stayed at 41 modules/90 edges; validator tests 7/7;
catalogue current; database consistent; the ten-job selective-component matrix
green; `git diff --check` clean; Doxygen re-measured at exactly 1,942/1,942 —
unchanged, at the ceiling.

While verifying the Doxygen baseline for ticket #1779, an independent
re-measurement using the repository's own canonical
`scripts/check_doxygen_warnings.sh` on the identical tree ticket #1778 left
behind returned exactly **1,942** warnings — the documented ceiling — not the
1,944 ticket #1778 recorded. A looser, non-canonical grep pattern reproduces
1,944 by additionally matching two `Inheritance graph ... not generated`
advisory lines that are not `file:line: warning:` diagnostics. This suggests
ticket #1778's figure came from a looser counting method, not a real
regression; inactive ticket **#1781**
(`REMED-DOCS-DOXYGEN-COUNT-RECONCILE`, P3, size XS) tracked re-verifying and
correcting this at pickup time. Not begun under #1779. See `NEXT.md`'s
equivalent correction note for full detail.

Ticket #1781 (`REMED-DOCS-DOXYGEN-COUNT-RECONCILE`, P3, size XS), opened and
closed 2026-07-27 on local branch
`feature/remediation-docs-doxygen-count-reconcile`, then picked up that
re-verification: it re-ran `scripts/check_doxygen_warnings.sh` on a clean,
current tree three times, including once from a fully clean
`docs/generated/`, and got exactly **1,942** warnings every time, matching the
documented ceiling and confirming ticket #1779's re-measurement above still
holds with no drift since. Per the ticket's own acceptance criteria, it then
corrected ticket #1778's own `plan.sqlite3` notes and
`audit/AUDIT_PROGRESS.md`'s #1778 entry, both of which stated 1,944 as a
measured fact, with preserved-history Correction notes rather than rewriting
them; this document and `NEXT.md` already carried an accurate correction from
#1779 and needed no further content change beyond this closure paragraph and
the ticket-count line above. No production or test source changed; no
`SR-AUD-*` finding was reopened or created; this was kept a
documentation/measurement-methodology-only change, not folded into any
unrelated ticket.

Design ticket #1782 (`REMED-COLL-SORTEDSET-VIEW-DESIGN`, P2, size M,
design-only), opened and closed 2026-07-27 on local branch
`feature/remediation-coll-sortedset-view-design`, then answered SR-AUD-361
without changing any production or test source.
`SortedSet<T>::GetViewBetween(lower, upper)` returns an independent snapshot
copy of the in-range elements instead of .NET's live, range-enforced,
bidirectionally write-through `TreeSubSet`, so ported C# that relies on
write-through compiles unchanged and silently mutates the wrong object.
Recorded in
[`docs/SortedSetLiveViewDesign.md`](docs/SortedSetLiveViewDesign.md): make
`SortedSet<T>` hold `std::shared_ptr<State>` — the `State` owning the
`std::set<T>` and the single version counter — plus `std::optional<T>` lower and
upper bounds, so one public type is either an owning full set or a bounded live
view over the same state and `GetViewBetween` keeps returning `SortedSet<T>` by
value. `std::shared_ptr` reproduces .NET's GC lifetime rule, so a view or an
iterator that outlives its set is well-defined rather than dangling; copying
preserves the object's role and assignment rebinds without mutating state
another handle observes. Four alternatives were rejected against a fourteen-row
compatibility matrix, including a dedicated `SortedSetView<T>` type (which does
not avoid the layout change, adds a return-type break on top, and breaks .NET's
structural parity in which a view **is a** `SortedSet<T>`) and retaining
snapshot semantics (a permanent, silent, undiagnosable divergence — the failure
mode ticket #1771 refused when it declined a throwing `CopyTo` shim).

**Correction:** the ticket #1779 paragraph above states that SR-AUD-361 "would
instead require replacing `SortedSet<T>`'s `std::set` backing with a tree
structure supporting live, bounded, write-through sub-range views". That premise
does not hold: `std::set` already provides `lower_bound`, `upper_bound`, and
stable iterators, and ticket #1782's working prototype demonstrates a bounded
live view on top of it. The real cost is the ownership model, the copy/move
semantics, and one required `const` removal. Recorded here rather than rewritten
in place, per this repository's preserved-narrative practice.

Evidence: six repository-local, gitignored probes in `build-probe-sortedset/`.
`probe1` independently reproduces the finding's own
`source-add-visible-in-view=0` / `view-add-visible-in-source=0` symptom and the
complete pre-fix contract, clean under ASan+UBSan+LeakSanitizer — the current
implementation is memory-safe and semantically wrong. `probe4` runs the
identical matrix against a prototype of the selected architecture with
`failures=0`, clean under the same sanitizers, including owner destruction with
surviving views and a 100,000-element scale case. `probe5` measures the
compatibility consequences rather than asserting them:
`sizeof(SortedSet<int>)` 56 → 40, `sizeof(SortedSet<std::string>)` 56 → 104,
`sizeof(Iterator)` 24 → 40, and the Itanium mangled name changing `_ZNK…` →
`_ZN…` when `const` is dropped — unlike ticket #1780's `Empty()`, whose mangled
name was byte-identical. Four adjacent defects measured inside the same member's
surface — `GetViewBetween` requiring `operator>` although the class documents
`operator<` only, bounds not enforced after construction, nested views silently
widening, and whole-object assignment defeating the fail-fast version guard
(silently wrong dereference on copy-assign, ASan-confirmed
`heap-use-after-free` on move-assign) — are folded into the implementation
ticket rather than given new `SR-AUD-*` identifiers, the numbering being frozen
at 364. The three existing `GetViewBetween` tests and the 41
mutable-`SortedSet` tests rerun unchanged and passing; boundaries stayed at 41
modules/90 edges; validator tests 7/7; catalogue current; database consistent;
`git diff --check` clean; Doxygen 1.9.8 at exactly 1,942/1,942 — unchanged,
since only `docs/*.md` and `audit/*.md` were added and Doxygen scans neither.
The full `scripts/local_ci_check.sh build` gate was run rather than omitted and
passed **13,022/13,022 tests across 37 executables** with zero build warnings
and zero errors — unchanged from ticket #1780, as expected when no production
or test source changes. `scripts/check_selective_components.sh` was not run: no
public header and no component metadata changed.

SR-AUD-361 stays **`confirmed`**, qualified `confirmed (design-complete)`, so
the index still records 355 open and nine `remediated`. Implementation is
separate ticket **#1783** (`REMED-COLL-SORTEDSET-LIVE-VIEW`, P2, size L),
created **`blocked`** and not begun, pending explicit user approval of three
things together: removing the `const` qualifier from `GetViewBetween`, the
snapshot-to-live-view semantic change, and the object-layout change requiring
every consumer to be rebuilt — the same approval category tickets #1770/#1771
and #1779/#1780 needed. There is no in-repository source break: all three
`GetViewBetween` call sites are tests on non-`const` sets and none asserts a
snapshot property. If approval is refused, the recorded fallback keeps snapshot
semantics while fixing the four adjacent defects; it needs no approval and
closes none of SR-AUD-361. Ticket #1773 remains `blocked` and untouched.

Implementation ticket #1783 (`REMED-COLL-SORTEDSET-LIVE-VIEW`, P2, size L),
opened and closed 2026-07-28 on local branch
`feature/remediation-coll-sortedset-live-view`, then landed that design after
the user granted the exact approval, scoped to this ticket. **SR-AUD-361 is now
`remediated`**, so the index records 354 open and ten `remediated`.
`GetViewBetween` keeps its return type and parameters, loses its `const`
qualifier, and returns a live bounded handle onto the same tree: inclusive
bounds enforced for the life of the view, out-of-range `Add` throwing
`ArgumentOutOfRangeException("item")`, out-of-range `Remove` returning `false`,
range-scoped `Clear`, narrowing-only nested views flattened onto the root state,
.NET's exact invalid-range message, a version-cached view `Count`, range-scoped
`Min`/`Max`, one shared version counter invalidating every handle's enumerators
in both directions, and bounds-enforcing write-through set algebra whose
self-aliasing guard now compares shared state rather than object identity.
Copying preserves the object's role, assignment rebinds without disturbing other
handles, and the additive `ToSortedSet()` materializes an independent range —
the documented replacement for the old snapshot behavior. The four adjacent
defects are closed with it, still without new `SR-AUD-*` identifiers.

Measured compatibility matched every #1782 prediction exactly:
`sizeof(SortedSet<int>)` 56 → 40, `sizeof(SortedSet<std::string>)` 56 → 104,
`sizeof(Iterator)` 24 → 40, traits preserved, and the mangled name changing
`_ZNK…` → `_ZN…`. Two limitations are recorded in design-record section 30
rather than hidden: a bounded exception-ordering divergence from .NET for a
nested call that is simultaneously inverted and widening, and a
ThreadSanitizer-measured race when concurrent threads call `getCountProperty()`
on *one* view object — documented in the header, not synchronized, since
`SortedSet<T>` claims no thread safety and this ticket adds none.

Closure evidence: 47 new permanent regressions plus a positive standalone
consumer fixture (`-Werror`, `Collections.Core` only, exits 0) and a negative
`const`-caller fixture (correctly rejected); the three pre-existing
`GetViewBetween` tests and all 41 mutable-`SortedSet` tests passing unchanged;
`SharpRuntimeTests_Collections_Core` 1,783/1,783; `scripts/local_ci_check.sh
build` at **13,069 tests across 37 executables**, up from 13,022, with zero
build warnings and zero errors; 41 modules/90 edges with no new dependency edge;
validator tests 7/7; catalogue current; database consistent; `git diff --check`
clean; Doxygen 1.9.8 at **1,937**/1,942; all ten selective components passing
with a repository-local `TMPDIR` (run this time, because a public header
changed); and a clean ASan+UBSan+LeakSanitizer campaign with LSan verified
active. Ticket #1773 stays `blocked`; CNA and mobile-eggbert were not inspected
or modified.

Ticket #1784 (`REMED-COLL-SORTEDSET-VIEW-COUNT-RACE`, P1, size S), opened and
closed 2026-07-28 on local branch `feature/remediation-coll-sortedset-count-race`,
then removed the second of those two limitations. It is a **post-audit defect
with no `SR-AUD-*` identifier** (the numbering stays frozen at 364), and
**SR-AUD-361 stays `remediated`** — this corrects a defect introduced by that
finding's remediation rather than reopening it, so the index counts are
unchanged at 354 open and ten `remediated`.

#1783's judgement that the race was acceptable "since `SortedSet<T>` claims no
thread safety" is reversed here on three grounds: a C++ data race is undefined
behavior rather than a merely unhelpful result; `getCountProperty()` is `const`
and warns nobody at the call site that calling it is a write; and it was a
*regression*, because the pre-#1783 header's `const` members wrote nothing. The
.NET comparison does not transfer either — a racing `int` write is defined in
the CLR, and .NET documents that its collections support multiple concurrent
readers.

Five repair alternatives were **measured** rather than argued
(`build-probe-sortedset/probe11_cache_alternatives.cpp`): removing the cache
gives `sizeof(SortedSet<int>)` 40 → 32, a `std::mutex` 80, a `std::shared_mutex`
96, and a published `shared_ptr` snapshot 48 — every one breaking the layout
#1783 had approved — while same-width atomics stay at exactly 40 and 104. A
cache relocated into the shared `State` was rejected structurally, since
arbitrary overlapping view bounds would require an unbounded keyed map, new
allocation, and a new element-type requirement. Selected: two
`std::atomic<intcs>` fields with a release/acquire publication protocol — count
stored first (`relaxed`), version stored last (`release`), version loaded first
(`acquire`) — so the (count, version) pair can never be read torn. Two relaxed
atomics would not have sufficed. `state_->version` deliberately stays plain, and
two `static_assert`s make a padded-atomic platform a compile error rather than a
silent ABI break.

The header now states the contract in two unequal halves: concurrent **mutation**
stays unsupported and undefined, with a set and every view over it one collection
for that purpose and **no new promise of mutation safety**; concurrent
**read-only** access is race-free, because no `const` member writes an
unsynchronized field. The type is still not thread-safe — it is merely free of
*internal* races when read.

Compatibility is unchanged in every layer: `sizeof(SortedSet<int>)` 40,
`sizeof(SortedSet<std::string>)` 104, `sizeof(Iterator)` 40, `alignof` 8, all
four value-semantics traits, and the mangled `GetViewBetween` symbol are
byte-identical to #1783's stored probe output, so no consumer rebuild is needed
on this revision's account and no new user approval was required. Count keeps its
complexity — O(1) for an owning set, O(k) once per version for a view — and
allocates nothing.

Closure evidence: 29 new permanent regressions in `SortedSetCountCacheTests.cpp`
(functional Count matrix, cache-sensitive properties, and the exact
pointer-to-member type of fourteen public members, with the published sizes
behind a 64-bit guard rather than an unconditional `static_assert`); post-fix
ThreadSanitizer clean in all nine real modes with the self-test still reporting
2, and #1783's own unmodified probe going 1 race → 0; ASan+UBSan+LSan 76/76 with
LSan verified active by a deliberate-leak self-test;
`SharpRuntimeTests_Collections_Core` 1,812/1,812 (was 1,783);
`scripts/local_ci_check.sh build` at **13,098 tests across 37 executables** (was
13,069), zero warnings and errors, after which the 13,069 floor in `README.md`
and `CLAUDE.md` was raised; all ten selective components passing plus
`Collections.Core` in isolation; the extended positive consumer fixture
compiling `-Werror` and exiting 0 with the negative fixture still rejected;
41 modules/90 edges; validator tests 7/7; catalogue current; database
consistent; `git diff --check` clean; Doxygen 1.9.8 unchanged at **1,937**/1,942.

Two **inactive** tickets were opened and not begun, neither with a new
`SR-AUD-*` identifier: **#1785** (`REMED-COLL-SORTEDSET-NESTED-EXCEPTION-ORDER`,
P3, XS) for the nested-view exception-ordering divergence #1783 recorded, which
needs a semantic decision rather than a bug fix; and **#1786**
(`REMED-COLL-VERSION-COUNTER-OVERFLOW`, P3, S) for the `int32_t` mutation-version
counter, which is incremented without bound and compared only for equality by
both the Count cache and `Iterator::checkVersion`. Both properties predate #1783
— they arrived with ticket 1713 — and #1784 changed only memory ordering, not
the values, the type, or the equality test.

Ticket #1786 (`REMED-COLL-VERSION-COUNTER-OVERFLOW`, P3, size S), opened
inactive by #1784, was then completed on local branch
`feature/remediation-coll-sortedset-version-overflow`. It carries **no new
`SR-AUD-*` identifier**, does not reopen SR-AUD-361, and is not a regression
from #1783 or #1784. Opened as an assessment; the assessment found a fully
compatible repair, so it was implemented in the same ticket. Four defects were
reproduced against the real production header before anything changed, using a
single probe source built against both the committed pre-fix header and the
working tree and positioning the counter with `-fno-access-control` rather than
performing billions of mutations: `++state_->version` at `INTCS_MAX` is
signed-integer overflow, reported by UBSan as
`signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'`
inside `Add`; a counter wrapped 2^32 mutations on silently revalidates a stale
`Iterator`; the same wrap silently revalidates a stale cached view `Count`; and
— a fourth defect the ticket's own description did not list, and the worst of
them — the `-1` `kCountNotCached` sentinel is itself a reachable counter value,
so a view that had **never** computed its Count read its cache as warm and
answered 0. .NET's own `SortedSet` has defects two, three, and four as
*defined-but-wrong* behaviour, since the CLR defines signed overflow as wrapping
where C++ makes it undefined; matching .NET's integer width would therefore not
have made the C++ code correct, and this port deliberately exceeds it. The
shared counter and the `Iterator` snapshot are now `SharpRuntime::ulongcs`,
which is free because the counter is not a member of `SortedSet<T>` and
`Iterator` already had four bytes of tail padding; the Count cache's 32-bit tag
cannot be widened without breaking the approved layout, so it is stored biased
by one and compared widened, which identifies a counter value exactly, cannot be
produced by a never-filled cache, and stops the cache being written once the
counter outgrows it. A first implementation used an explicit horizon branch and
measurably cost +1 ns on every `Count` call, including an owning full set's; two
variant headers isolated the branch as the cause and the biased tag removed it.
Closure evidence: 29 new permanent regressions in
`SortedSetVersionOverflowTests.cpp`, whose near-boundary cases reach the counter
through a test-only friend seam rather than a production hook;
`SharpRuntimeTests_Collections_Core` 1,841/1,841 (was 1,812) with no assertion
edited; `scripts/local_ci_check.sh build` at **13,127 tests across 37
executables** (was 13,098), zero warnings and errors, after which the 13,098
floor in `README.md` and `CLAUDE.md` was raised; UBSan clean post-fix;
ASan+UBSan+LSan 105/105 with LSan verified active; ThreadSanitizer clean across
#1783's, #1784's, and a new six-mode probe covering the recompute path, with
both self-tests still reporting races; `sizeof`, `alignof`, all traits, the
mangled `GetViewBetween` symbol, and **every member offset** unchanged; both
consumer fixtures behaving as before; 41 modules/90 edges; validator tests 7/7;
catalogue current; database consistent; `git diff --check` clean; Doxygen 1.9.8
unchanged at **1,937**/1,942; all ten selective components plus
`Collections.Core` in isolation. The contract is recorded in
`docs/SortedSetVersioningDesign.md`, with a pointer from
`docs/SortedSetLiveViewDesign.md` section 32.

One further ticket was opened by #1786 and not begun, again with no
`SR-AUD-*` identifier: **#1787**
(`REMED-COLL-VERSION-COUNTER-OVERFLOW-SWEEP`, P3, M), covering the other
collections that carry the identical `intcs version_` counter. #1786's
stored acceptance criteria asked for a repository-wide implementation; the
instruction governing that working session scoped #1786 to `SortedSet<T>` and
required the remainder to become a separate inactive ticket. The divergence is
recorded in the design document rather than silently absorbed, and the full
inventory the criteria asked for is delivered there. **#1787 is now done — see
below.**

### Completed repository-wide mutation-counter sweep: ticket #1787

Ticket #1787 (`REMED-COLL-VERSION-COUNTER-OVERFLOW-SWEEP`, P3, size M) is
**done**, closed 2026-07-28 on local branch
`feature/remediation-coll-version-counter-sweep`, with no new `SR-AUD-*`
identifier and no audit finding reopened. The full record is
[`docs/CollectionVersionCounterSweep.md`](docs/CollectionVersionCounterSweep.md).

It corrected #1786's inventory in three ways rather than inheriting it. The
count is **sixteen** counter-carrying types, not fifteen: `BitArray` was
missed, and it is also the one whose counter was already `std::uint32_t` rather
than `intcs`, so it never had the signed-overflow undefined behaviour. And a
**third defect class** was found that appears in neither ticket's description
and is more serious than either recorded one: the implicitly declared copy/move
assignment operator transplanted the *source's* counter into the destination, so
an enumerator outstanding over the destination survived having every element it
could refer to destroyed. It needs **no overflow at all** — the counters merely
have to match — and six of the fourteen affected types reproduced as
AddressSanitizer `heap-use-after-free` or `heap-buffer-overflow` rather than as
wrong answers. `LinkedList<T>` was immune, because ticket #1769 had already
given it a bumping `operator=`.

Pre-fix evidence, all against the committed headers before anything changed
(gitignored `build-probe-collversion/`, one probe source built against both
revisions): **14** UBSan signed-overflow reports, one per collection; **15**
iterator/enumerator ABA reproductions at the 2^32 alias distance; **8**
assignment-alias reproductions; **6** ASan memory errors.

The repair replaces each bare integer with the new
`System::Collections::detail::BasicMutationCounter`, whose increment is unsigned
and whose **assignment advances the destination instead of taking the source's
value**, while copy construction still inherits it (matching .NET's
`ArrayList.Clone`/`Hashtable.Clone`). Thirteen collections took the 64-bit
`MutationCounter`; `LinkedList<T>` and `BitArray` took the 32-bit
`NarrowMutationCounter` because widening them grows a public object — measured,
arithmetically unavoidable in any member order — so both keep a documented,
test-pinned 2^32 residual and both have a **blocked** ticket stating the exact
approval required.

Closure evidence: **336** new permanent regressions in
`CollectionVersionCounterTests.cpp`, whose near-boundary cases reach every
counter through **one** test-only friend seam
(`SharpRuntime::Testing::CollectionVersionAccess<T>`) generalising #1786's;
`SharpRuntimeTests_Collections_Core` **2,177/2,177** (was 1,841) with no
existing assertion edited; `scripts/local_ci_check.sh build` at **13,463 tests
across 37 executables** (was 13,127), zero warnings and errors, after which the
13,127 floor in `README.md` and `CLAUDE.md` was raised; UBSan and ASan clean
post-fix on every probe mode; ASan+UBSan+LSan **349/349** with LSan verified
active twice over — it caught a real 24-byte leak in this ticket's own first
draft test; ThreadSanitizer **0 races** in three real modes with the self-test
still reporting 2; every `sizeof`, `alignof`, and counter offset unchanged, with
**0 symbols removed or renamed** and 10 new weak inline definitions for the new
counter class; a new positive consumer fixture compiling `-Werror` and exiting 0
and a new negative one correctly rejecting the test seam as an incomplete type;
41 modules/90 edges; validator tests 7/7; catalogue current; database
consistent; `git diff --check` clean; Doxygen 1.9.8 at **1,938**/1,942 — one
warning more than the pre-ticket 1,937, attributable entirely to the single new
`README.md` markdown link into `docs/`, which `Doxyfile` does not scan (the six
existing README links into `docs/` each cost the same, and the ceiling is
unchanged); all ten selective components plus `Collections.Core` in isolation;
performance within run-to-run noise on every benchmarked path.

Three tickets were opened and deliberately not begun: **#1788**
(`REMED-COLL-LINKEDLIST-VERSION-WIDEN`, P3, S, **blocked**) needs approval for
`sizeof(LinkedList<T>)` 40 → 48; **#1789**
(`REMED-COLL-BITARRAY-VERSION-WIDEN`, P3, XS, **blocked**) needs approval for
`sizeof(BitArray::Enumerator)` 32 → 40; and **#1790**
(`REMED-COLL-LIST-INDEXER-VERSION`, P3, L, `todo`) records the separate,
pre-existing, non-versioning divergence that `List<T>::operator[]` returns a
plain `T&` and so cannot bump the counter the way .NET's index setter does.
#1788 and #1789 are deliberately **not** one ticket: they share the symptom and
nothing else, and a user might reasonably approve one and not the other.

Tickets #1785 and #1773 remain untouched and inactive. **Ticket #1790 is now
design-complete — see the next section.**

### Completed List<T> indexer versioning design: ticket #1790

Ticket #1790 (`REMED-COLL-LIST-INDEXER-VERSION`, P3, size L, category `parity`)
is **done as a design ticket**, closed 2026-07-28 on local branch
`feature/remediation-coll-list-indexer-design`, with **no `SR-AUD-*`
identifier** — the numbering stays frozen at 364. It changed no production
behaviour, signature, layout, or exception; the single production edit is a
doc-comment correction in `List.hpp`. The durable record is
`docs/ListIndexerVersioningDesign.md`.

It answers the ticket's own question with a **no**: there is no fully
source-compatible correction. A plain `T&` cannot be intercepted, because C++
provides no mechanism by which a collection learns of a write through a
reference it previously returned. Acceptance-criteria route (a) — declare the
divergence permanent — was rejected, because the same `T&` is a **reproduced
use-after-free** (four AddressSanitizer heap-use-after-free reports: across a
reallocating `Add()` on both read and write, across `Clear()`, and across move
assignment), not merely a fail-fast divergence. The selected architecture is a
tracked proxy, `System::Collections::detail::ElementReference<T>`, chosen
because it is the only alternative that closes the write path while keeping
`list[i] = v` — the exact spelling C# uses — compiling.

Three of the ticket's own premises were corrected rather than inherited. The
indexer is **not** the widest hole: the non-const `ToVector()` hands out the
whole backing `std::vector<T>&`, permitting `push_back`/`clear` — a *structural*
mutation the guard never sees, previously undocumented anywhere. The migration
premise was wrong for this repository: measured across **all 625 translation
units** by compiling against a `[[deprecated]]`-tagged shim, the non-const
indexer has **61 call sites, all in two test files**, and **no library source
includes `List.hpp` at all** — while the CNA/mobile-eggbert burden stays real,
unmeasured, and out of scope. And `IList<T>` has **four** implementers, not one
(`List<T>`, `ObjectModel::Collection<T>`, `ObjectModel::ReadOnlyCollection<T>`,
and a hand-written one in the test suite), found by compiling against the
candidate header rather than by grepping — a `grep` for `public IList<` finds
only the first.

Measured source break, from four generated shims compiled against the whole
repository at `-fsyntax-only`: the refined proxy breaks **1 site in 1 of 625
translation units** — the hand-written implementer, i.e. migration — against
**8 sites in 3 units** for the rejected value/setter alternative, of which only
2 are genuine call-site breaks. The two figures are close and the decision is
explicitly **not** made on them; it is made because the value alternative
deletes `list[i] = v` from the API. Layout measured: `sizeof(List<T>)`
**40 → 40** unchanged, `sizeof(ObjectModel::Collection<T>)` **32 → 40**, proxy
16 bytes. The unavoidable cost, stated rather than buried: `list[i].member` and
`list[i].method()` stop compiling for value-type elements, because `operator.`
cannot be overloaded.

Closure evidence: **14 new permanent regressions** in
`ListIndexerVersionTests.cpp`, split into a `Contract` suite (must survive
#1791) and a `Divergence` suite (each case asserting today's behaviour with
.NET's named alongside, carrying `static_assert`s #1791 cannot land without
editing); `SharpRuntimeTests_Collections_Core` **2,191/2,191** (was 2,177) with
no existing assertion edited; `scripts/local_ci_check.sh build` at **13,477
tests across 37 executables** (was 13,463), zero warnings and errors; 41
modules/90 edges; validator tests 7/7; catalogue current; database consistent;
`git diff --check` clean; Doxygen 1.9.8 unchanged at **1,938**/1,942.

Two tickets were opened and deliberately not begun. **#1791**
(`REMED-COLL-LIST-INDEXER-VERSION-IMPLEMENT`, P2, L, **blocked**) carries the
implementation in two phases: Phase 1 (tracked `getItem`/`setItem`, a pure
addition) needs **no** approval but does not close the defect; Phase 2 needs the
exact four-part approval in design section 28 — public source breaks to
`List<T>::operator[]` and to the `IList<T>` interface, an object-layout change
to `Collection<T>`, and acknowledgement that CNA's usage is unmeasured. The
#1771, #1780, and #1783 approvals do **not** carry over. **#1792**
(`REMED-COLL-ENUMERATOR-CURRENT-CONSTCAST`, P3, M, `todo`) records a **newly
discovered defect**: `Generic::IEnumerator<T>::getCurrentProperty()` does
`const_cast<T*>(&Current())` and publishes a mutable `void*` to the live element
on a public interface, so a write through it mutates a collection mid-walk with
the counter at rest. It affects **every** collection, not `List<T>`, which is
why it is its own ticket and not absorbed. No new `SR-AUD-*` identifier.
**Correction (ticket #1792, 2026-07-28): the "every collection" claim in the
sentence above is wrong** — `Dictionary`, `HashSet`, `SortedSet`, and
`SortedDictionary` implement no `IEnumerator` at all. See the next section.

Tickets #1785, #1788, #1789, and #1773 remain untouched and inactive.
**Ticket #1792 is now done — see the next section.**

### Completed enumerator Current safety design: ticket #1792

Design ticket #1792 (`REMED-COLL-ENUMERATOR-CURRENT-CONSTCAST`, P3, size M,
category `defect`), opened inactive by #1790, was completed on local branch
`feature/remediation-coll-ienumerator-current-design`. It carries **no
`SR-AUD-*` identifier** and reopens no finding. It **changed no production
behaviour, no public signature, no object layout, and no exception** — it edits
no production source at all. The record is
[`docs/IEnumeratorCurrentSafetyDesign.md`](docs/IEnumeratorCurrentSafetyDesign.md).

Opened as "remove the `const_cast` or document the divergence as deliberate", it
answers the first: the divergence is remediable and is **not** recorded as
permanent. The selected architecture is **`std::any` returned by value from the
non-generic accessor** — the direct C++ counterpart of .NET's `object
IEnumerator.Current`, which returns a value, boxes value types, and hands out no
pointer. `Generic::IEnumerator<T>::Current()` stays `const T&`.

It corrected four of its own premises rather than inheriting them. **The defect
does not reach every collection**: the four hash- and tree-backed generic
collections expose STL-style iterators and no `IEnumerator`, so the measured
reach is thirteen generic plus eight non-generic implementations plus two
hand-written test-local ones. **The bridge's `const_cast` is not the only one**:
four more live outside it, one of which publishes a writable pointer to a live
`std::unordered_map` key. **It is six distinct defect classes, not one**, closed
by different measures — and `const void*` was *measured* to close only the first
of them, because a one-line `const_cast` restores the write. **The most dangerous
property is the ABI**: `void*`, `const void*`, and `std::any` all produce the
byte-identical mangled name while `this` moves from `%rdi` to `%rsi`, so a
partially rebuilt consumer links with no diagnostic and corrupts memory.

Evidence: four ASan `heap-use-after-free` reports plus two non-faulting
stale-aliasing shapes; a `ReadOnlyCollection<T>` mutated through its own
enumerator, reaching the caller's shared backing vector; a `Hashtable` entry made
unreachable by both its old and its new key while `Count` still reported it; 0
UBSan diagnostics and 0 LSan leaks; a five-candidate allocation and layout
comparison; calling-convention disassembly; a 626-translation-unit deprecation
sweep measuring 28 non-generic, 4 bridge, and 27 typed call sites with 0 compile
failures; and three fully migrated header shims breaking 6, 7, and 6 translation
units at 12, 14, and 12 sites with **zero library sources broken under any of
them**, so the proposed bodies are compile-validated.

Closure evidence: **17 new permanent regressions** in
`EnumeratorCurrentSafetyTests.cpp`, split into a `Contract` suite (8 cases that
must survive #1793) and a `Divergence` suite (9 cases carrying `static_assert`s
#1793 cannot land without editing); `SharpRuntimeTests_Collections_Core`
**2,208/2,208** (was 2,191) with no existing assertion edited;
`scripts/local_ci_check.sh build` at **13,494 tests across 37 executables** (was
13,477), zero warnings and errors; 41 modules/90 edges; validator tests 7/7;
catalogue current; database consistent; `git diff --check` clean; all ten
selective components passing; Doxygen 1.9.8 unchanged at **1,938**/1,942.

One ticket was opened and deliberately not begun — **#1793**
(`REMED-COLL-IENUMERATOR-CURRENT-SAFETY-IMPLEMENT`, P2, L), which is now
**done**; see the next section. As opened it was **blocked**, in two
phases. Phase 1 (write the ownership/lifetime/validity rules into both headers)
needs **no** approval and does not close the defect. Phase 2 needs the exact
three-part approval in design section 33 — public source breaks to
`System::Collections::IEnumerator` and to `Generic::IEnumerator<T>`, and
acknowledgement of the silent ABI break requiring a full consumer rebuild. There
is **no** object-layout change. The #1771, #1780, and #1783 approvals do **not**
carry over. **#1793 should land before #1791**, and the two must not be merged:
they are independent defects on disjoint surfaces and neither repairs the other.

Two residual limitations are recorded rather than buried: the typed `Current()`
reference hazard is **not** closed (closing it needs a by-value `Current()`,
which makes move-only `T` uninstantiable), and
`IDictionaryEnumerator::getKeyProperty()`/`getValueProperty()` keep returning
`const void*` into live storage, which is a separate follow-on rather than a
widening of this approval.

Tickets #1785, #1788, #1789, #1791, and #1773 remain untouched and inactive.

### Completed enumerator Current safety implementation: ticket #1793

Implementation ticket #1793 (`REMED-COLL-IENUMERATOR-CURRENT-SAFETY-IMPLEMENT`,
P2, size L, category `defect`), opened blocked by #1792, was completed on local
branch `feature/remediation-coll-ienumerator-current-safety` after the user
granted design section 33's three-part approval explicitly and scoped to it. It
carries **no `SR-AUD-*` identifier** and reopens no finding; SR-AUD-356 stays
`remediated` from ticket #1767. Both phases landed together. The record is
[`docs/IEnumeratorCurrentSafetyDesign.md`](docs/IEnumeratorCurrentSafetyDesign.md)
section 34, appended below #1792's design record rather than rewriting it.

`System::Collections::IEnumerator::getCurrentProperty()` now returns an owning
`std::any` **by value** instead of a mutable `void*`.
`Generic::IEnumerator<T>::Current()` is unchanged at `const T&`; its inherited
bridge boxes a copy and throws `System::NotSupportedException` for an element
type that cannot be copied, matching .NET's documented `ref struct` answer. All
four `const_cast`s outside the bridge are gone and both `mutable` members are
ordinary again. Eight production non-generic overrides, the bridge, two
test-local implementers, and the three in-library call sites migrated; **zero
library sources broke**, exactly as the design's shim sweep predicted.

The defect was reconfirmed before any production edit, with the output preserved
under `build-probe-ienumerator/prefix1793/`: 15 defects across six modes, four
ASan `heap-use-after-free` reports, a `ReadOnlyCollection<T>` mutated through
its own enumerator into the caller's shared vector, a `Hashtable` entry made
unreachable by both its old and its new key while `Count` still reported it, and
a same-width wrong cast silently wrong with no sanitizer diagnostic.

Four corrections to the design's section 14 sketch are recorded in section 34.3,
two of them caught only by running the new suite: the `if constexpr`
else-branch had to call `Current()` and discard it, or a move-only `T` would
have reported `NotSupportedException` where the pre-#1793 bridge reported
`InvalidOperationException`; `Generic::List<std::any>` cannot be instantiated at
all, because `std::any` is not equality-comparable; `std::any(Current())` for
`T = std::any` selects the copy constructor, so the box is never nested; and the
non-generic `Stack`/`Queue` `ICollection` constructors gained a
`std::bad_any_cast` path.

Closure evidence: the nine `Divergence` cases were **flipped, not deleted** —
renamed `EnumeratorCurrentSafety`, each asserting the opposite outcome through
the same accessor, with the `static_assert`s now pinning `std::any` so a revert
cannot land silently; `SharpRuntimeTests_Collections_Core` **2,229/2,229** (was
2,208); a **clean full rebuild** in a dedicated `build-abi-1793` tree at
**13,515 tests across 37 executables** (was 13,494), zero warnings and errors;
ASan+UBSan clean on all six migrated lifetime shapes; 0 LSan leaks with
LeakSanitizer proved active by a deliberate-leak self-test; TSan deliberately
not run, with the reason stated; object layout `diff`-identical to the stored
baseline; the mangled name byte-identical and the vtable slot unchanged at
offset `0x20`; a stale-object probe in which an old caller and a new
implementation **linked with zero diagnostics** and then took a SEGV; a positive
consumer fixture passing under `-Werror` and a negative fixture rejected at all
six marked sites; 41 modules/90 edges; validator tests 7/7; catalogue current;
database consistent; `git diff --check` clean; Doxygen 1.9.8 at **1,939**/1,942
— one above the canonical 1,938, because `Doxyfile` does not scan `docs/` and the
new `README.md` link into it resolves as an unresolved `\ref`, exactly as its two
neighbouring entries already do (design record section 34.8).

Allocation was measured rather than assumed: 0 for `int`, a raw pointer, and an
already boxed `int`; **1** for a small SSO `std::string` and a
`std::shared_ptr`; 2 for a 64-char `std::string` and a `DictionaryEntry`. The
middle row corrects design section 22, which predicted 0 for any type at most
one pointer wide — libstdc++'s `std::any` small-buffer optimisation admits only
types that *fit in* a `void*`. A full consumer rebuild is mandatory and the
linker will not say so; README.md carries that warning with the migration table.

Three residual limitations stand, unchanged from the design's risk register: the
typed `Current()` reference hazard is **not** closed (its validity window is now
in the header, together with the statement that #1793 did not close it);
`IDictionaryEnumerator`'s `const void*` accessors are untouched, with a warning
now on that interface and a ticket of their own — **#1794**
(`REMED-COLL-IDICTENUM-KEYVALUE-SAFETY`, P3, size M, `blocked`, not begun); and CNA's and
mobile-eggbert's usage remains unmeasured, with #1773 still `blocked`.

Tickets #1785, #1788, #1789, #1791, and #1773 remain untouched and inactive. No
repair ticket is active.

### Completed SortedSet nested-view exception ordering: ticket #1785

Ticket #1785 (`REMED-COLL-SORTEDSET-NESTED-EXCEPTION-ORDER`, P3, size XS,
category `design`) is **done**, closed 2026-07-28 on local branch
`feature/remediation-coll-sortedset-nested-order` after the user explicitly
approved acceptance branch **(b)** — adopt .NET's ordering — scoped to this
ticket alone. It carries **no `SR-AUD-*` identifier** (numbering frozen at 364)
and reopens nothing; SR-AUD-361 stays `remediated`. The record is
[`docs/SortedSetLiveViewDesign.md`](docs/SortedSetLiveViewDesign.md) section 33,
appended below sections 1–32 rather than rewriting them; §15's original ordering
rule is preserved verbatim under two supersession markers pointing at §33.

`SortedSet<T>::GetViewBetween` now validates in .NET's order — lower widening,
then upper widening, then the lower-versus-upper relationship — where #1782
selected and #1783 shipped the reverse. Exactly one `if` moved.
`SortedSet.TreeSubSet.cs:342-353` performs both widening tests against its own
bounds and only then delegates to `SortedSet.cs:1508-1515`, which owns the
`SR.SortedSet_LowerValueGreaterThanUpperValue` check; the widening tests are
therefore unconditionally first, the lower bound precedes the upper, and
`_underlying` is the root set, which is why nesting flattens to depth 1.

A probe printed the whole matrix before and after
(`build-probe-sortedset/probe18_{prefix,postfix}.log`): **exactly 7 of 32 outcome
rows changed**, every one a nested call that is *simultaneously* widening and
inverted. `view[3,7]` asked for `(2, 1)` is now
`ArgumentOutOfRangeException("lowerValue")` and `(12, 9)` is now
`ArgumentOutOfRangeException("upperValue")`, both formerly
`ArgumentException("Must be less than or equal to upperValue.", "lowerValue")`.
Every success, every widening-only failure, every inverted-only failure, and
every **top-level** call is byte-identical: an owning set activates neither
bound. Widening *both* ends while inverted is arithmetically unreachable and is
proved so by an exhaustive grid rather than asserted.

Nothing else moved: no public signature, return type, `const` qualification,
mangled symbol, vtable, `sizeof`, `alignof`, or member offset; ownership, live
write-through, bounds inclusivity, nested flattening, Count caching, iterator
invalidation, the thread-safety contract, and the allocation behaviour are all
unchanged, and a rejected call still bumps no version. Every in-repository
caller was reviewed — six test files and two consumer fixtures, no production
`src/` caller — and none asserted a doubly-invalid nested call.

Closure evidence: `SortedSetNestedViewOrderTests.cpp` adds **23** permanent
cases, including an exhaustive `(lower, upper)` grid checked against .NET's
decision procedure transcribed as an independent oracle;
`SharpRuntimeTests_Collections_Core` **2,252/2,252** (was 2,229);
`scripts/local_ci_check.sh build` at **13,538 tests across 37 executables** (was
13,515), zero warnings and errors; ASan+UBSan+LSan over four SortedSet suites at
**128 tests, 0 diagnostics, 0 leaks**, with LeakSanitizer proved active by a
deliberate-leak self-test; TSan deliberately not run, with the reason stated;
the SortedSet consumer fixture extended and passing under `-Werror`; 41
modules/90 edges; validator tests 7/7; catalogue current; database consistent;
`git diff --check` clean; Doxygen 1.9.8 **unchanged at 1,939**/1,942; all ten
selective components plus `Collections.Core collections_sorted_set_view.cpp` in
isolation. The disposable `build-abi-1793` tree from #1793 was removed after
confirming its results are already recorded here — **1.46 GiB reclaimed** — with
its two evidence logs kept.

Tickets #1788, #1789, #1791, #1794, and #1773 remain untouched and inactive. No
repair ticket is active.

SR-AUD-362 (`FrozenDictionary::Create` duplicate keys) was reconciled
conservatively alongside #1779, per that finding's own instruction to inspect
rather than repair it: its per-file audit report and
`audit/AUDIT_FINDINGS_INDEX.md` row now carry a Correction note
cross-referencing ticket #1778's finding that its premise does not hold
against the current .NET reference. The repository's index status vocabulary
supports only `confirmed`/`remediated`, so it stays `confirmed` rather than
being assigned an invented status — but it must not be read as an active,
un-investigated defect, and it is not counted as `remediated`.

No repair ticket is active.

### Completed IDictionaryEnumerator key/value design: ticket #1795

Design ticket #1795 (`REMED-COLL-IDICTENUM-KEYVALUE-SAFETY-DESIGN`, P3, size M,
category `design`) was completed on local branch
`feature/remediation-coll-idictenumerator-keyvalue-design` on 2026-07-28, with
**no production or test-source change**. Durable record:
[`docs/IDictionaryEnumeratorKeyValueSafetyDesign.md`](docs/IDictionaryEnumeratorKeyValueSafetyDesign.md).

**Ticket #1794 was not reused.** Its row is an *implementation* row — it migrates
the accessors and is blocked on approval to do so, not on a decision about what
to do. Converting it into a completed design ticket to reuse the number would
have recorded implementation work as done when none was. It stays `blocked`, and
#1795 is the design.

Selected: **Entry-canonical owning accessors with a mandatory `MoveNext`-time
snapshot.** `getEntryProperty()` stays `DictionaryEntry` by value and becomes
canonical; `getKeyProperty()`/`getValueProperty()` return an owning `std::any`
by value equal by construction to that entry's members; `getCurrentProperty()`
keeps #1793's signature and boxes the `DictionaryEntry` on **both**
implementations, matching .NET's `Current => Entry`; and every implementation
must answer every accessor from state the enumerator owns.

**The return-type change alone is not the fix.** Neither accessor
version-checks, so both dereference a container iterator a mutation may have
invalidated. On `ListDictionaryInternal` that makes even `getEntryProperty()` and
the already-migrated `getCurrentProperty()` AddressSanitizer
`heap-use-after-free` after `Clear()` or destruction. .NET's own
`HashtableEnumerator` snapshots `_currentKey`/`_currentValue` at `MoveNext` and
never reads `_buckets` from an accessor; the design adopts that.

Three premises in #1794's own description are contradicted by measurement and
corrected in the design record. The most important: *"there is no write path
through them"* is **false for `Hashtable`** — `getValueProperty()` returns a
pointer to the live map's non-`const` `std::any`, so `const_cast` + assignment is
well-formed, defined C++ that rewrites dictionary storage with the mutation
counter unmoved and a second enumerator silent; and `getKeyProperty()` reaches
the `const std::string` key, producing at 64 entries an entry `Count` still
reports that **no lookup can return by either its old or its new key**.

Two previously unrecorded `ListDictionaryInternal` parity defects were found and
decided: its `getCurrentProperty()` boxes the key where .NET is
`Current => Entry`, and it disagrees with itself about `const` on a value.

Measured: 10 unique call sites across 628 translation units (0 compile
failures); a fully migrated three-header shim breaks **1 of 628** at 1 line and
**2 of 2,252** permanent tests, both being the two parity defects above; 20
pre-fix defects with **8 ASan `heap-use-after-free`** reports, 0 UBSan
diagnostics on the corruption, and three fatal scenarios that complete *silently*
under UBSan alone; 42 post-fix assertions clean under ASan+UBSan+LSan; the
mangled name **byte-identical** and the vtable slot unchanged at `0x30` while
`this` moves `%rdi` → `%rsi`; a stale caller and new implementation **link with
zero diagnostics** then SEGV with a UBSan invalid-vptr report; a **second**
stale-object vector — `ListDictionaryInternal::NodeEnumerator` 40 → 72 bytes with
an `inline` `GetEnumerator()`, reproduced as ASan `heap-use-after-free` — noting
`NodeEnumerator` is a **private nested class**, so this is not a public layout
change; allocations 0 → 1/2 for a `Hashtable` key and **0 → 0** for
`ListDictionaryInternal`, with `Entry` and `Current` unchanged.

Seven alternatives were evaluated with a compatibility matrix. Alternative F
(enumerator-owned copies behind an unchanged `const void*`) was **measured** —
0 of 628 translation units break, no calling-convention change — and rejected as
the selected design because it leaves type safety and implementation divergence
entirely open and reintroduces the enumerator/collection desynchronisation
#1793 removed. It is the documented fallback if the approval is declined and
must never be called a remediation.

**#1794 stays `blocked`**, now depending on #1795, with a rewritten four-item
approval: a public source break to `IDictionaryEnumerator`; two observable
`ListDictionaryInternal` behaviour changes; acknowledgement of a silent ABI break
through **two** independent mechanisms requiring a full consumer rebuild; and
item 2 separately declinable. #1793's, #1771's, #1780's and #1783's approvals do
not carry over. Recommended order: **#1794 before #1791**; the two migrations
must not be merged.

Validation: boundaries 41 modules / 90 edges, validator tests 7/7, catalogue
current, database consistent, `git diff --check` clean, Doxygen 1.9.8 at
**1,939/1,942** unchanged, and `scripts/local_ci_check.sh build`
**13,538/13,538 across 37 executables** with zero warnings/errors —
all unchanged, as expected for a design-only ticket. No new `SR-AUD-*`
identifier; **SR-AUD-356 stays `remediated`** and CCF-018 is not reopened. The
defect is **not** marked remediated. Tickets #1773, #1788, #1789, #1791, and
#1794 remain `blocked` and untouched.

### Completed IDictionaryEnumerator key/value safety implementation: ticket #1794

Implementation ticket #1794 (`REMED-COLL-IDICTENUM-KEYVALUE-SAFETY`, P3, size M,
category `defect`) is **done**, on local branch
`feature/remediation-coll-idictenumerator-keyvalue-safety`, landing the
architecture #1795 selected under the §33 approval granted **in full** (items 1,
2a, 2b, 3).

`getKeyProperty()` and `getValueProperty()` return an **owning `std::any` by
value**; `getEntryProperty()` is canonical and unchanged; `getCurrentProperty()`
keeps #1793's signature and now boxes the `DictionaryEntry` on **both**
implementations. The load-bearing half is the **mandatory `MoveNext()`-time
snapshot**: `ListDictionaryInternal::NodeEnumerator` gained a `DictionaryEntry
current_` (40 → 72 bytes, a **private nested** class), and **no accessor on
either implementation dereferences a container iterator** — which is what closes
the lifetime class, since no accessor version-checks and .NET's own
`HashtableEnumerator` snapshots for exactly this reason.

Re-measured before any source changed: `defects=20` on the write paths,
identical to #1795; and **nine** ASan `heap-use-after-free` reports across
sixteen lifetime scenarios — **correcting #1795 §8.2's prose, which said
"eight" where its own table listed nine**. Three of the nine needed no caller
misuse at all. Three further corrections to the design record are written into
`docs/IDictionaryEnumeratorKeyValueSafetyDesign.md` §37.1: §24 never measured
`MoveNext()`, and the snapshot costs `ListDictionaryInternal::MoveNext()`
**2.8 → 23.9 ns per position** while `Hashtable::MoveNext()` is unchanged;
§12.3's predicted 2,250/2,252 became **2,252/2,252** once both predicted
assertions were updated; and §22's synthetic ABI numbers were **re-measured on
the real interface**, where every prediction held.

Both **silent ABI mechanisms were reproduced end to end on the real headers**:
byte-identical mangled names and unchanged vtable slots (`0x30`, `0x38`) with
`this` displaced `%rdi` → `%rsi` behind a hidden `sret` — links with zero
diagnostics, then SEGV, UBSan invalid vptr, and a bogus
`System::InvalidOperationException` out of garbage; and the `NodeEnumerator`
40 → 72 growth against an `inline` `GetEnumerator()` — links clean, then ASan
`heap-use-after-free`. **A full consumer rebuild is mandatory**; README.md
carries the entry with per-shape migration guidance.

Validation: **+64 permanent tests** in a new suite parameterised over both
implementations; the three pinned assertions **updated, not deleted**; post-fix
probe on real headers at 42 assertions with **0 ASan/UBSan/LSan diagnostics and
0 leaks**; the new suite under ASan+UBSan+LSan at 78 tests clean, leak detection
proved active by the 284-byte self-test; **TSan not run, precondition verified**
(no `mutable` member, every accessor `const`, every `current_` write inside the
non-`const` `MoveNext()`/`Reset()`); pre-fix caller source no longer compiles;
positive consumer fixture clean under `-Werror` and passing, negative fixture
rejected at every marked site; boundaries 41 modules / 90 edges; validator tests
7/7; catalogue current; database consistent; `git diff --check` clean;
`scripts/check_selective_components.sh` run with a repository-local `TMPDIR`;
Doxygen 1.9.8 at **1,940/1,942**, the single new warning identified as the
unresolvable `\ref` for the new `README.md` link into `docs/`. The full gate ran
from a dedicated clean **`build-abi-1794`** tree at **13,602 tests across 37
executables**, zero warnings/errors, `SharpRuntimeTests_Collections_Core` at
**2,316**.

**SR-AUD-356 and CCF-018 are recorded as remediated by this ticket.** No new
`SR-AUD-*` identifier. Left open and stated rather than buried:
`MoveNext()`/`Reset()` after collection destruction remain undefined, and the two
**pre-existing** `Hashtable` write escapes outside this interface now have their
own inactive ticket **#1796** (`REMED-COLL-HASHTABLE-WRITE-ESCAPES`, P3,
`blocked`) instead of only a risk note. Tickets #1773, #1788, #1789, #1791 and
#1796 remain `blocked` and untouched.

### Completed Hashtable value-access design: ticket #1797

Design ticket #1797 (`REMED-COLL-HASHTABLE-VALUE-ACCESS-DESIGN`, P3, size M,
`design`) is **done**. No production or test source changed. Durable record:
[`docs/HashtableValueAccessSafetyDesign.md`](docs/HashtableValueAccessSafetyDesign.md).

**Ticket #1796 was not reused.** Its row is an *implementation* row — it closes
the escapes and is blocked on approval to perform that change, not on a decision
about what it is — so recording it as a completed design would log implementation
work as done when none was performed. #1796 **stays `blocked`**, now depending on
#1797, with acceptance criteria and an exact four-item approval rewritten from
the design. This is the same #1795 → #1794 handling one ticket earlier.

**Four of #1796's own premises are corrected by measurement**, each against this
record's convenience:

1. **Four escape routes, not two.** #1796 names `operator[]` and `getItem()`. It
   misses `at()`, which returns a `const std::any&` **into live map storage**
   where `const_cast` + assignment is well-formed, fully defined C++ that rewrote
   the stored value with the counter unmoved; and it misses `setItem`/`Add`'s
   non-`const` `void*` value parameter, a type hole on the input side.
2. **Rehash does *not* dangle a retained alias.** `std::unordered_map` is
   node-based, and the address of a stored value was **unchanged across 8,000
   insertions**. The hazard is `Remove`, `Clear`, copy assignment, move
   assignment and destruction — **nine ASan `heap-use-after-free` reports across
   fourteen scenarios**, 0 LSan leaks with detection proved active by a 317-byte
   self-test.
3. **The worst defect is one #1796 never mentions.** `operator[]` on an *absent*
   key performs a **structural insert** without bumping, so a bare read changes
   `Count`. Measured at 4,008 entries, an outstanding enumerator then visited
   **2,045 distinct keys**, reached **6 of its 8** pre-mutation seed keys, threw
   nothing, and produced **no report from ASan, UBSan or LSan**. All sixteen
   reproduced defects are silent under UBSan alone.
4. **The sibling `IDictionary` implementation has its own defects.**
   `ListDictionaryInternal::setItem`'s *replace* branch returns before
   `++version_` where .NET does `version++` first unconditionally, and it accepts
   and **stores a null key** where .NET and this port's `Hashtable` both throw.
   Filed as **new inactive ticket #1798** (`REMED-COLL-LISTDICTINTERNAL-PARITY`,
   P3, `blocked`), not absorbed.

**Selected: owning reads, tracked writes, no public alias into storage.**
`getItem` → `std::any` by value; `operator[]` → a non-copyable
`Hashtable::ValueReference` proxy whose read conversion returns `std::any` **by
value**, plus a new `const` by-value overload; `at()` → by value throwing
`KeyNotFoundException` instead of `std::out_of_range`. Two proxy details are
load-bearing rather than stylistic, and both were found by measurement:
`std::any`'s template converting constructor outranks a *copyable* proxy's own
conversion operator, so `std::any b = h[k];` silently boxes the **proxy** and
throws `std::bad_any_cast` at run time with nothing wrong at compile time; and a
conversion returning `const std::any&` makes `const std::any& r = h[k];` trip
GCC 14's `-Wdangling-reference`, which every module here compiles with `-Werror`.

**Measured:** 12 call sites across 629 translation units, all in tests, with
`operator[]` at **0** sites; **3** units / 5 sites broken by Phase 2, all in the
test suite — fewer than migrating `getItem` alone (6 units), because the sibling
implementer is migrated in the same change; `at()` → by value breaks **0**;
mangled name **byte-identical** and vtable slot unchanged at `0x38` while `this`
moves `%rdi → %rsi` behind a hidden `sret`, reproduced as a stale caller that
**links with zero diagnostics then SEGVs** with 14 UBSan misaligned-address
reports; `sizeof(Hashtable)` **unchanged at 72** — this is *not* a layout break
in #1788/#1789/#1791's sense; reads cost **0 allocations for an `int`**, 1 for an
SSO string, 2 for a large one, 1.2 → 5.4/15.7/27.7 ns.

**The obvious tidy-up is rejected on evidence**: migrating `setItem`/`Add`'s
raw-key value parameter to `const std::any&` makes `Add("literal", v)` store the
entry under the **stringified address of the literal**, because the standard
`const char*` → `const void*` conversion beats the user-defined
`const char*` → `std::string` one — and it compiles clean under `-Werror`.

**Alternative A′ (`const std::any*`) is the documented fallback**, measured as
**byte-identical machine code** to today. It leaves the alias-lifetime class
entirely open and must never be recorded as a remediation. A shared proxy with
#1791 is explicitly rejected on four measured incompatibilities; recommended
order is **#1796 before #1791**, and the migrations must not be merged.

Validation, all unchanged as expected for a design-only ticket: 41 modules / 90
edges, validator tests 7/7, catalogue current, database consistent,
`git diff --check` clean, Doxygen 1.9.8 at **1,940** of the 1,942 ceiling,
`scripts/local_ci_check.sh build` at **13,602 tests across 37 executables**.
`check_selective_components.sh` not run (no public header or component metadata
changed); required when #1796 Phase 2 lands. Build directories: `build/`
(reused, `--parallel 3`) and the **shared** `build-probe/` (one compiler process
per probe; `MAX_JOBS = 3` in the two Python sweeps). **No compilation exceeded
three jobs** — the ceiling was lowered from four to three during this ticket at
the user's instruction, and `scripts/local_ci_check.sh` and
`scripts/check_selective_components.sh` were corrected from their hard-coded
`--parallel 4` in the same change.

Tickets #1773, #1788, #1789, #1791 and #1796 remain `blocked` and untouched;
#1798 is newly opened `blocked` and deliberately not begun; #1790, #1792, #1793,
#1794 and #1795 remain `done`. #1793 and #1794 were not reopened. CNA and
mobile-eggbert were not inspected, searched, configured, built, or modified.

### Completed ListDictionaryInternal setter design: ticket #1799

Design ticket #1799 (`REMED-COLL-LISTDICT-SETITEM-DESIGN`, P3, size M,
`design`) is **done**. No production or test source changed. Durable record:
`docs/ListDictionaryInternalSetterDesign.md`.

**Ticket #1798 was not reused.** Its row is an *implementation* row — it performs
the corrections and is blocked on approval to do so, not on a decision about what
they are — so recording it as a completed design would log implementation work as
done when none was performed. #1798 **stays `blocked`**, now depending on #1799,
with its acceptance criteria and an exact three-item approval rewritten from the
design. Same #1795 → #1794 and #1797 → #1796 handling one and two tickets
earlier.

**Selected:** a private `ValidatedKey` boundary that every raw-key operation must
construct before the single `findNode()` locator will look at storage —
structurally unskippable, unlike `Hashtable`'s `toKey()` convention — plus one
upsert path in `setItem` that bumps on insert, on replace **and** on an
equal-value replace with the bump placed *after* the mutation (a **strong**
exception guarantee .NET's bump-first shape cannot offer), plus deletion of the
one `const_cast` that made the key view's `CopyTo` disagree with every other key
surface. **No signature, return type, parameter type or data member changes.**

**Four of #1798's own premises are corrected by measurement**, each against this
record's convenience:

1. **Six defects, not two.** #1798 names the `setItem` replace bypass and the
   accepted null key. It misses that the **key view's `CopyTo` launders away the
   caller's `const`** — `MemberCollection::copyToCore` boxes
   `const_cast<void*>(n.key)` where all four other key surfaces box
   `const void*`, so `std::any_cast<const void*>` on a `CopyTo` slot throws
   `std::bad_any_cast`, and a write through the `void*` it *does* hand out was
   reproduced as an **AddressSanitizer SEGV on read-only storage** — and misses
   that `Add`-on-duplicate and `Remove`-of-an-absent-key both diverge from .NET
   in the **opposite** direction from the setter.
2. **"Match .NET's unconditional `version++`" is the wrong instruction.** .NET
   `ListDictionaryInternal` bumps first and unconditionally, before it even
   searches, so a throwing `Add` and a no-op `Remove` both invalidate every
   outstanding enumerator; **.NET's own `Hashtable` does neither**, and .NET's
   two implementations disagree on three of ten version rows. Copying the former
   literally would introduce two new false-positive
   `InvalidOperationException`s and would contradict a currently *passing*
   assertion (`CollectionVersionCounterTests.cpp`'s `ListDictionaryAdapter` sets
   `kHasNoOpMutation = true` for an absent-key `Remove`). The selected rule is
   **advance on effective mutation**, which matches .NET on every row where
   .NET's two implementations agree and takes the `Hashtable` rule where they
   disagree.
3. **The null-key rationale is not SR-AUD-363's.** On `Hashtable`, `nullptr`
   stringified to `"0"` and *aliased* the ordinary string key `"0"`. Here keys
   are compared by raw address and **no valid object has the null address**, so
   a stored null key aliases nothing — measured. The defect is purely that the
   **two implementations of one interface disagree**, on all five raw-key entry
   points.
4. **The stale-object hazard does not crash the way #1794's and #1796's did.**
   No signature changes and every affected body is `inline` in a header, so an
   unrebuilt consumer **silently keeps the defect** — and the outcome is
   **link-order and optimisation-level dependent**: at `-O0` with the stale
   object first on the link line, a correctly *rebuilt* translation unit
   silently reverted to the old bodies, and `-flto -Wodr` diagnoses nothing.

**Measured, against the committed headers unless stated:** the ten-row version
table across the port's two `IDictionary` implementations and both .NET
references, showing **three** divergences on `ListDictionaryInternal` (replace,
equal replace, and — in the other direction — throwing `Add` and absent `Remove`)
and **one previously unrecorded divergence on `Hashtable`**; **four** enumerator
kinds silently valid after a value replacement — the dictionary enumerator, the
key view, the value view and the same through an `IDictionary&` — one of which
**enumerated the post-mutation value**, with **0 ASan and 0 UBSan reports**; six
null-key rows on which the two implementations disagree, with a null key proved
**not** to alias any real key and "absent" still distinguishable from "present
with a null value"; the key view boxing `const void*` on `Current` and `void*` on
`CopyTo`, with `std::bad_any_cast` on one and an **ASan SEGV on a write to
`.rodata`** through the other, while the same object reached through `Current`
cannot be written at all; LeakSanitizer **proved active** by a 350-byte
deliberate-leak self-test with **0 leaks** in every real scenario; **53 of 53**
`ListDictionaryInternal` mangled names byte-identical, the **19-entry vtable
identical** with `getItem` at offset 72 and `setItem` at 80, and `sizeof`
unchanged at **40 / 72 / 24 / 24** with `ValidatedKey` at 8 bytes emitting **no
symbol at all** at `-O2`; the stale-object probe at `-O0`, `-O2` and
`-flto -Wodr`; **0 added allocations** with `setItem` replace moving 1.30–1.66 ns
to 1.53–1.60 ns; and the selected design passing **33/33** contract assertions on
a compile-validated shim.

**Rejected on evidence, not argument:** literal .NET `ListDictionaryInternal`
parity; a `const std::any&` key parameter (a public source *and* ABI break on
`IDictionary` and both implementations that collides with the measured
`Add("literal", v)` address-key corruption of #1797 §13.4 while fixing no
versioning defect); scattered null checks at five entry points; split
insert/replace helpers (two places for the version rule to drift apart — the
exact mechanism of the present defect); normalising every key surface to `void*`
(would reintroduce the `const`-laundering #1793 removed, on four more surfaces);
comparing old and new values to skip an equal-value bump; **a shared proxy or
shared upsert abstraction with #1791 or #1796** (this type hands out no alias and
needs no proxy at all); and a **negative consumer fixture**, because nothing in
this design fails at compile time so one could not fail — stated explicitly so it
is not mistaken for an omission.

**Approval #1798 needs, exactly three items, per action** (design §36), none of
which carries over from #1771, #1780, #1783, #1793, #1794 or #1796: (1) a null
key becomes `ArgumentNullException("key")` on five entry points that currently
succeed; (2) a value replacement — including an equal-value one — advances the
counter, turning a currently-silent enumeration into
`InvalidOperationException`, and includes the two deliberate deviations from .NET
in §15; (3) the key view's `CopyTo` boxes `const void*`, so `any_cast<void*>`
keeps compiling and starts throwing at run time — 3 assertion lines in 2 files,
separately declinable. Plus a required acknowledgement: **a full consumer rebuild
is mandatory.**

**Validation (all unchanged, as expected for a design-only ticket):** 41 modules
/ 90 edges, validator tests 7/7, catalogue current, database consistent,
`git diff --check` clean, Doxygen 1.9.8 at **1,940** of the 1,942 ceiling,
`scripts/local_ci_check.sh build` at **13,657 tests across 37 executables** with
zero warnings and zero errors, `Collections_Core` at **2,371**.
`check_selective_components.sh` **not run** (no public header or component
metadata changed); required when #1798 lands. Build directories: `build/` (reused
incrementally, `--parallel 3`), the **shared** `build-probe/` (one compiler
process per probe, `1799_` file prefix), and `build-tmp/` as `TMPDIR`. **No new
build directory was created and no compilation exceeded three jobs.**

**Three inactive follow-up tickets were opened and deliberately not begun:**
#1800 (`REMED-COLL-VERSION-SEAM-ODR`) for the pre-existing
`CollectionVersionAccess` IFNDR that #1796 reported and no ticket recorded;
#1801 (`REMED-TOOLING-NEGATIVE-FIXTURE-CI`) for the negative-fixture per-site
checker that exists only under the gitignored `build-probe/`; and #1802
(`REMED-COLL-HASHTABLE-REMOVE-VERSION`) for the `Hashtable` absent-key `Remove`
over-bump this ticket measured.

No new `SR-AUD-*` identifier: the audit numbering is frozen at 364 and all six
defects were found during remediation. **The defect is not marked remediated.**
Tickets #1773, #1788, #1789, #1791 and #1798 remain `blocked` and untouched;
#1790, #1792, #1793, #1794, #1795, #1796 and #1797 remain `done` and none was
reopened. CNA and mobile-eggbert were not inspected, searched, configured, built,
or modified. No push, merge, rebase, tag, or publication occurred.

No repair ticket is active.

### Completed ListDictionaryInternal setter remediation: ticket #1798

Implementation ticket #1798 (`REMED-COLL-LISTDICTINTERNAL-PARITY`, P3, size M,
`defect`) is **done**, under the three explicit per-action approvals design
ticket #1799 required. Durable record:
`docs/ListDictionaryInternalSetterDesign.md` §37. #1799 remains `done` and is
not reopened.

**Six measured defects on `System::Collections::ListDictionaryInternal` — the
second production implementer of `IDictionary` — are closed**, and every one was
reproduced again against the committed headers before a line was edited:

1. **`setItem`'s replace branch returned before `++version_`** (version `3 → 3`
   while the stored value changed), so **four** outstanding enumerator kinds
   walked to the end after a replacement with no diagnostic — the dictionary
   enumerator, the key view, the value view, and the same through an
   `IDictionary&` — and the **value view enumerated the post-mutation value**,
   with **0 ASan and 0 UBSan reports**. `setItem` is now one upsert: validate,
   locate, then replace-and-bump or insert-and-bump, with the bump **after** the
   mutation succeeds — a **strong** exception guarantee .NET's bump-first shape
   cannot offer. An **equal-value replacement bumps too**; the value is never
   compared, because equality of a `void*` is address equality and .NET compares
   neither.
2. **All five raw-key entry points accepted `nullptr`** and `setItem` **stored**
   it. A private **`ValidatedKey`** now throws
   `ArgumentNullException("key")` — message
   `Value cannot be null. (Parameter 'key')`, HResult `0x80004003` — and the
   single `findNode()` locator accepts **nothing else**, so validation is
   **structurally unskippable** rather than conventional. That is the whole
   reason the design chose it over a `Hashtable`-style `toKey()` helper, and a
   new negative fixture now **compiles that claim**: 6 of 6 sites rejected.
3. **The key view's `CopyTo` laundered away the caller's `const`** —
   `const_cast<void*>(n.key)` where all four other key surfaces box
   `const void*`, so one view had two incompatible element types and writing
   through the writable pointer the library manufactured for a key the caller
   declared `const` was an **AddressSanitizer SEGV on read-only storage**. The
   `const_cast` is deleted and its superseded rationale comment rewritten.

**Two deliberate deviations from .NET `ListDictionaryInternal`, approved and now
asserted as contract**: a throwing duplicate `Add` and a `Remove` of an absent
key do **not** bump. .NET bumps first and unconditionally on both, which would
manufacture two new false-positive `InvalidOperationException`s out of calls that
changed nothing and would contradict a currently *passing* assertion
(`CollectionVersionCounterTests.cpp`'s `kHasNoOpMutation = true`). **.NET's own
`Hashtable` does neither**, so "match .NET" was never a specification here. The
rule taken is **advance on effective mutation**, which `MutationCounter.hpp`
already documented and which both of this port's `IDictionary` implementations
now follow. `Clear` keeps its unconditional bump, matching .NET.

**Four of the design's own figures are corrected by measurement** (§37.1), each
against this record's convenience:

1. §11 said **0 existing assertions change** for the null-key row. It was
   **one** — and ticket #1796 had put it there deliberately, its comment saying
   "so #1798 has a test to flip". It was **flipped, not deleted**. Corrected
   source break: **4 assertion lines in 3 files**, not 3 in 2.
2. §22's stale-object table **understated the hazard at `-O2`**. Re-run on the
   real headers, `-O2` is **also link-order dependent**: with a stale object
   first on the link line the *rebuilt* translation unit reverts to the
   defective bodies too. The bad link order is dangerous at **both**
   optimisation levels, not only `-O0`. `-flto -Wodr` still diagnoses nothing.
3. §21.1's "7 new symbols" measured as 10 lines, two of them local string
   constants and one `_M_unhook` (from `erase` replacing `remove_if`). The
   substantive claim is unchanged: **none is a `ListDictionaryInternal` symbol**.
4. §24's **+0.2 ns per replace is not resolvable above noise** on the production
   header (1.78–2.01 ns before, 1.82–2.13 ns after, faster in two of three
   runs). Its load-bearing half holds exactly: **0 allocations added**.

**Deviation from design §28, stated so it is not mistaken for scope creep:** §28
proposed **no** negative fixture, correctly, *for the representation change* —
that one fails at run time, not compile time, and is pinned in the permanent
suite and the positive fixture instead. But §28 did not consider the design's
**other** compile-time claim, that validation is structurally unskippable.
`test/consumer/collections_dictionary_setter_negative.cpp` asserts it, because
without it the difference between the selected design and rejected alternative A
is a comment. **CI coverage, exactly:** its per-site checker lives under the
**gitignored** `build-probe/`, so the committed fixture is compiled by **no
tracked CI job** — pre-existing inactive ticket #1801, which applies equally to
the three earlier negative fixtures and which #1798 **neither widens nor
closes**. Both *positive* fixtures are compiled `-Werror` **and run** by
`check_selective_components.sh Collections.Core`.

**Interaction with inactive ticket #1800, recorded and not fixed:** the new
suite adds a **third** `CollectionVersionAccess` specialisation, spelled
**token-for-token** as the two existing `SR1794_SEAM_BODY` ones. Identical
specialisations across translation units are well-formed; the IFNDR is the
**divergence** with `CollectionVersionCounterTests.cpp`'s `SR1787_SEAM_BODY`.
That is pre-existing, is **not introduced, not widened and not fixed** here, and
**#1798 does not claim to close it.**

**No signature, return type, parameter type or data member changed** — measured
on the real headers, not the shim: **53 of 53** mangled names byte-identical,
the **19-entry vtable identical** with `getItem` at 72 and `setItem` at 80,
`this` still in `%rdi` with **no `sret`**, `sizeof` unchanged at **40 / 72 / 24
/ 24**, and `ValidatedKey`/`findNode` emitting **no symbol at all** at `-O2`.
**A full consumer rebuild is nevertheless mandatory**, and README.md says so:
every affected body is `inline` in a header, so a stale object links with **zero
diagnostics** and then **silently keeps the defect** — no crash, unlike #1794's
and #1796's breaks.

**Validation:** a **fresh `cmake --fresh` configuration and clean-first
rebuild** of `build/` — **631 objects, 0 predating the configure**, all 36 test
executables relinked, 0 warnings, 0 errors — then **13,723 tests across 37
executables** from that rebuilt tree (floor was 13,657), `Collections_Core`
**2,437** (was 2,371, **+66**). ASan + UBSan + LSan clean on the entire
`Collections.Core` suite and on both consumer fixtures, with LeakSanitizer
**proved active** by a 350-byte self-test; the §8.3 SEGV is now **unreachable**
because the cast that yielded the writable pointer throws first. The design's own
33-assertion contract probe re-pointed from the shim to the **production** header:
**33/33**. 41 modules / 90 edges, validator 7/7, catalogue current, database
consistent, `git diff --check` clean, Doxygen **1,940** of the 1,942 ceiling,
full `check_selective_components.sh` matrix passing.

**Build directories:** `build/` (fresh configure + clean-first, `--parallel 3`),
the **new** `build-asan/` from the closed CLAUDE.md set (`--parallel 3`,
`ccache`), the **shared** `build-probe/` and `build-consumer/` under a `1798_`
file prefix, and `build-tmp/` as `TMPDIR`. **No build-directory name outside the
closed set was invented and no compilation exceeded three jobs.**
`check_selective_components.sh` needed `TMPDIR` redirected into `build-tmp/` so
its `mktemp -d` matrix root stayed out of `/tmp`; `build-asan/` needed
`-Wno-maybe-uninitialized` for a GCC 14 `-O1` false positive inside
`<bits/std_function.h>` in a Text.RegularExpressions translation unit, unrelated
to this ticket.

**Not claimed closed:** address-based key comparison; `MoveNext`/`Reset` after
the collection is destroyed; a view or enumerator outliving its dictionary; the
silent stale-object hazard; `ValidatedKey` being unskippable within the class
but not across the codebase; the cosmetic duplicate-`Add` message divergence
(§9.5, still not required); and the real blast radius in CNA and mobile-eggbert,
**unmeasured by instruction**.

No new `SR-AUD-*` identifier: the audit numbering is frozen at 364 and all six
defects were found during remediation. `Hashtable` was **not** modified — its
absent-key `Remove` over-bump stays inactive ticket #1802, and closing it is
what would make the two implementations agree on all ten version rows. Tickets
#1800, #1801 and #1802 remain `blocked` and unbegun; #1773, #1788, #1789 and
#1791 remain `blocked` and untouched; #1790 and #1792–#1799 remain `done` and
none was reopened. CNA and mobile-eggbert were not inspected, searched,
configured, built, or modified. No push, merge, rebase, tag, or publication
occurred.

No repair ticket is active.

### Completed Hashtable Remove versioning remediation: ticket #1802

Implementation ticket #1802 (`REMED-COLL-HASHTABLE-REMOVE-VERSION`, P3, size S,
`defect`) is **done**, under the explicit per-action user approval its row
required. Durable record: `docs/HashtableValueAccessSafetyDesign.md` §35. #1796
and #1799 remain `done` and neither is reopened.

All three `System::Collections::Hashtable::Remove` overloads were
`_map.erase(key); ++version_;`, so the fail-fast mutation counter advanced
**whether or not the key was present**. Reproduced against the committed headers
before any edit: **24 defects over 43 checks**, **0 over the same 43** after.
Removing an absent key moved the counter `3 → 4` and threw
`InvalidOperationException` out of **four** enumerator kinds — the
`IDictionaryEnumerator`, the key view, the value view, and the same through an
`IDictionary&`; a full walk after one absent `Remove` yielded **0 of 3** entries
and `Reset()` threw too. `Count` and contents were correct on every row, so this
is a **false positive** — the exact opposite direction of error from #1798's,
which missed a mutation that really happened.

All three overloads now route through one private
`removeKey(const std::string&)` helper that is
`if (_map.erase(key) != 0) ++version_;`.
`std::unordered_map::erase(const key_type&)` already returns the number of
elements removed, so the correction adds **no second lookup, no `Contains`
pre-check, no second key conversion, no allocation and no lock**; the bump
follows the erase, giving a strong exception guarantee. This matches .NET
`Hashtable.Remove`, which calls `UpdateVersion()` only inside the branch that
found and cleared a bucket (`Hashtable.cs:999`), and completes the
"advance on effective mutation" rule
`docs/ListDictionaryInternalSetterDesign.md` §9.3 selected for the interface.
**With #1798 and #1802 both closed the port's two `IDictionary` implementations
agree on all ten version rows of that design's §6.1.**

`Clear()` was deliberately **not** changed and its deviation is now decided
rather than implicit: it bumps unconditionally, including on an already-empty
table, where .NET `Hashtable.Clear` early-returns under a
`_count == 0 && _occupancy == 0` guard whose `_occupancy` half has no
`std::unordered_map` analogue. The unconditional bump also errs in the
memory-safe direction and matches .NET `ListDictionaryInternal.Clear` and the
port's own sibling. It is asserted on both implementations in the permanent
suite and in the consumer fixture.

**Not an ABI break, but a silent stale-object hazard.** `sizeof(Hashtable)`
unchanged at 72, the 19-entry vtable byte-identical with `Remove` still at slot
`0x70`, `this` still in `%rdi` with no `sret`, undefined-symbol list identical.
Every affected body is `inline` in a header, so a stale object links with zero
diagnostics and silently keeps the old false positive — link-order dependent at
`-O0`, per-translation-unit at `-O2`, with `-flto -Wodr` diagnosing nothing.
**A full consumer rebuild is mandatory**, and `README.md` says so.

**Validation:** fresh `cmake --fresh` configuration and clean-first rebuild of
`build/` (**632 objects, 0 predating the configure**, 36 executables relinked, 0
warnings, 0 errors), then **13,790 tests across 37 executables** from that tree
(floor was 13,723), `Collections_Core` **2,504** (was 2,437, **+67**). A later comment-only edit (two doc-comments changed from `§9.3` to `section 9.3` so the two headers stay pure ASCII) triggered one incremental build that recompiled **11 translation units and relinked 1 executable** — the **complete** dependent set, since exactly eleven `.d` files in the tree name `Hashtable.hpp` or `IDictionary.hpp` — leaving **0** objects predating the fresh configuration; the gate below ran from that tree. ASan +
UBSan + LSan clean on the whole `Collections.Core` suite, a focused scenario
probe and both consumer fixtures, with LeakSanitizer proved active by a 350-byte
self-test reported as 383 bytes in 2 allocations. 41 modules / 90 edges,
validator 7/7, catalogue current, database consistent, `git diff --check` clean,
Doxygen **1,940** of the 1,942 ceiling, full `check_selective_components.sh`
matrix passing. Allocation counts identical on every `Remove` path; the one
measured slowdown is on the throwing null-key path and is proved by disassembly
and by a bare-throw control to be code-layout, not added work.

No new `SR-AUD-*` identifier: the numbering is frozen at 364 and the defect was
found during remediation, by #1799's probe. `ListDictionaryInternal` was **not**
modified. #1800 and #1801 remain `blocked` and unbegun — the new suite adds a
fourth `CollectionVersionAccess` specialisation spelled token-for-token as the
three existing `SR1794_SEAM_BODY` ones, so the count of *divergent* bodies is
still two, and no negative fixture was added because nothing here changes at
compile time. #1773, #1788, #1789 and #1791 remain `blocked` and untouched;
#1790 and #1792–#1799 remain `done`. CNA and mobile-eggbert were not inspected,
searched, configured, built, or modified. No push, merge, rebase, tag, or
publication occurred.

No repair ticket is active.

### Completed test-seam ODR remediation: ticket #1800

Ticket #1800 (`REMED-COLL-VERSION-SEAM-ODR`, P3, size S, `defect`) is **done**.
Durable record: `docs/CollectionVersionTestSeamDesign.md`. **No production
source, signature, symbol or object layout changed** — no file under any
`modules/*/include` was touched — so nothing in this section is a consumer
concern.

Five translation units of the one `SharpRuntimeTests_Collections_Core` program
each defined `SharpRuntime::Testing::CollectionVersionAccess` themselves, in two
divergent families (`SR1787_SEAM_BODY` with `positionVersion`, `SR1794_SEAM_BODY`
without), so three specialisations had two token-different definitions in one
program — ill-formed, **no diagnostic required**.

- **Three divergences, not the two the row named.** The **partial**
  specialisation `<detail::BasicMutationCounter<V>>` diverged as well (`read` +
  `write` against `read` alone) and is the one both collection-level bodies
  delegate to. Measured by preprocessing each unit with the build's own flags and
  hashing token sequences, not by grep.
- **The consequence was measured, not assumed.** At `-O0`, which this repository
  builds, swapping two object files on the link line changed the answer a unit
  that had spelled the correct body itself received; at `-O1` and `-O2` the two
  units disagree inside one process. `ld`, `-flto -Wodr`, ASan with
  `detect_odr_violation=2`, and UBSan each reported nothing.
- **Repair:** one authoritative header,
  `modules/collections/tests/support/CollectionVersionSeam.hpp`, holding the
  counter-level seam and all fifteen collections behind one macro; the five
  suites include it. The **richer** body became canonical, so #1787's
  near-boundary matrix keeps every capability it had.
- **Permanent guard:** `scripts/check_version_seam_odr.py` (four rules; it
  *discovers* seams rather than hard-coding them, and covers #1786's
  `SortedSetVersionAccess` too) plus `test/check_version_seam_odr_test.py` with
  12 fixtures, both wired into `scripts/local_ci_check.sh`. It exits 1 against
  the committed pre-fix sources and against an injected hypothetical suite, and 0
  against the repository. A second body inside a unit that includes the header is
  already a hard compile error, so the checker only has to cover the unit that
  does not include it.
- **One cost, reported:** four suites gained thirteen header includes, +0.38 to
  +0.42 s of front-end time each (+31 %), +1.6 s against a 336 s clean-first
  rebuild. Splitting the header would recover it; it was not done, because
  deciding which of two headers a new collection belongs in is the decision that
  produced two bodies in the first place.

Validated from a fresh configure and clean-first rebuild at three jobs: 13,790
across 37, `Collections.Core` 2,504, zero warnings and errors, 632 objects and
37 executables all post-marker, every seam COMDAT byte-identical, the post-fix
link-order probe agreeing at `-O0`/`-O1`/`-O2` in both orders, ASan/UBSan/LSan
2,504 with no diagnostic, the full selective matrix plus an explicit isolated
`Collections.Core` selective build, 41 modules / 90 edges, validator 7/7, seam
checker 12/12, catalogue current, database consistent, `git diff --check` clean,
Doxygen 1.9.8 at 1,940 of the 1,942 ceiling. TSan is not relevant and was not
run: no thread, no shared mutable state and no atomic is introduced.

No new `SR-AUD-*` identifier: the numbering is frozen at 364 and the defect was
found during remediation, by #1796. **#1801 remains `blocked` and is not closed**
— it asks for a tracked per-site checker for the six negative consumer fixtures,
and #1800's checker compiles nothing and shares none of that infrastructure.
#1773, #1788, #1789 and #1791 remain `blocked` and untouched; #1790, #1792–#1799
and #1802 remain `done` and none was reopened. CNA and mobile-eggbert were not
inspected, searched, configured, built, or modified. No push, merge, rebase,
tag, or publication occurred. (**#1801 closed the same day, immediately after
#1800**, and the verified count was **seven** fixtures rather than six — see the
next section; this paragraph is left as #1800's own accurate record.)

### Completed negative-fixture CI remediation: ticket #1801

Ticket #1801 (`REMED-TOOLING-NEGATIVE-FIXTURE-CI`, P3, size S, `tooling`, area
*Developer experience*) is **done**. Durable record:
[`docs/NegativeConsumerFixtureValidation.md`](docs/NegativeConsumerFixtureValidation.md).
It is infrastructure only: **no production source, signature, symbol, layout,
vtable, exception contract or collection semantic changed**, and nothing under
`modules/*/include` or `modules/*/src` was touched.

Seven committed `test/consumer/*_negative.cpp` files existed to prove that
outlawed spellings are rejected by the compiler, and **no tracked job compiled
any of them**; per-site logic existed for two of the seven, under the gitignored
`build-probe/`. The independently verified inventory is **7 fixtures / 37
sites**, not the six the ticket row named — the row predates
`collections_dictionary_setter_negative.cpp` (#1798) — and three fixtures had no
per-site checker at all, while
`collections_object_model_readonlydictionary_negative.cpp` named a
`scripts/check_readonlydict_empty_negative.sh` that has never existed in any
commit.

The false pass was reproduced before anything was built: a temporary copy of the
Hashtable fixture with one marked site made legal still failed at nine other
lines, so a whole-file check reported PASS while one of eleven claims had become
false; the retained gitignored checker caught it at 10/11 and nothing tracked
did.

The selected convention is a numbered preprocessor guard per site
(`#if SHARP_RUNTIME_NEGATIVE_SITE == N`) with an inline `// NEGATIVE(<id>):
<fragment>` marker and `//     | <alternative>` continuations, chosen over
runner-generated variants, an external manifest, one file per expression, and
Clang `-verify` comments. The tracked file is compiled as-is with a `-D`, so
nothing is generated; the all-sites-off baseline must compile with **zero
diagnostics**, which is both the soundness argument for per-site attribution and
the reason no CMake change was needed.

`scripts/check_negative_consumer_fixtures.py` compiles 44 translation units
(7 baselines + 37 sites) with `-std=c++23 -Wall -Wextra -Wpedantic -Werror
-fsyntax-only`, include directories derived from the repository's own CMake
component metadata, `LC_ALL=C` for deterministic diagnostic wording, and a hard
three-job ceiling that refuses a higher request. It runs from
`scripts/local_ci_check.sh` before the configure step.
`test/check_negative_consumer_fixtures_test.py` is 37 cases in 2.1 s, including
the permanent regression proof on a real tracked fixture, and
`build-probe/1801_mutation_campaign.py` is 7/7 with exactly one problem reported
per mutated fixture.

Validated from a fresh configure and clean-first rebuild at three jobs: **7
fixtures / 37 sites / 37 rejected** in 12.5 s at peak 3 jobs, checker fixtures
37/37, mutation campaign 7/7, zero warnings and errors (346 s, 632 objects none
predating the configure), **13,790 tests across 37 executables**,
`Collections.Core` **2,504**, the full ten-component selective matrix with its
three forbidden fixtures still rejected, 41 modules / 90 edges, validator 7/7,
seam checker 12/12, catalogue current, database consistent, `git diff --check`
clean, Doxygen 1.9.8 at **1,940** of the 1,942 ceiling. Sanitizers are not
applicable to a Python checker and compile-only validation, and none was built.

No new `SR-AUD-*` identifier. One residual gap is recorded rather than absorbed:
`SortedSetVersionAccess` has no consumer-side fixture, which is new inactive
ticket **#1803** (`REMED-TOOLING-SORTEDSET-SEAM-NEGATIVE-FIXTURE`, `blocked`,
not begun). #1773, #1788, #1789 and #1791 remain `blocked` and untouched; #1790,
#1792–#1800 and #1802 remain `done` and none was reopened. CNA and
mobile-eggbert were not inspected, searched, configured, built, or modified. No
push, merge, rebase, tag, or publication occurred.

No repair ticket is active.

### P2 — Consumer-driven API breadth

1. **Extend `ImmutableList<T>::Builder` only from a concrete consumer need.**
   Core construction, mutation, query, and snapshot behavior is implemented.
   Advanced query, sorting, and copy overloads remain separate bounded work;
   preserve the documented vector-backed snapshot semantics.

2. **Extend `BinaryReader` only from a concrete consumer need.**
   `ReadChar`, `ReadChars`, `Read(char[])`, `ReadDecimal`, and seekable
   `PeekChar` are implemented. Any further encoding or buffering breadth must
   preserve decoder state, supplementary UTF-8 handling, and truncated-input
   behavior.

3. **Review other documented partial surfaces by demand.**
   Examples include debugger/process breadth and richer XML reader/writer
   functionality.
   A documented partial API is not automatically higher priority than a
   consumer-visible bug.

### P2 — Developer experience

4. **Reduce the Doxygen warning backlog incrementally.**
   The reproducible Doxygen 1.9.8 baseline is 1,942 warnings. Require touched
   public APIs not to regress it; avoid a mass comment-only rewrite.

5. **Decide whether distribution support is wanted.**
    The repository currently supports `add_subdirectory`; it has no installed
    package/export configuration and no standalone sample application. Add
    install rules, package config, or a sample only after the desired consumer
    workflow is selected.

### Requires explicit user direction

- Adding Windows, macOS, or Emscripten jobs to the repository CI matrix.
- Introducing a new third-party dependency.
- Broad public-header renames or compatibility-breaking refactors.
- Expanding permanent out-of-scope areas such as reflection, TLS/X.509, or
  symmetric/asymmetric encryption.

## Definition of done for future work

A task is complete only when all applicable items hold:

1. The expected behavior is verified against the actual .NET source under
   `/rv/tmp/runtime/src/libraries/`, not memory or an old audit statement.
2. The change has one clear physical module owner and declares only necessary
   public/private/test dependency edges.
3. A regression test demonstrates any corrected behavior; existing tests are
   not weakened to hide a failure.
4. `scripts/local_ci_check.sh build` passes with zero warnings/errors and the
   test count does not decrease without a documented reason.
5. Boundary/catalogue checks pass, and selective fixtures are updated when a
   component surface or dependency changes.
6. Concurrency, lifetime, or low-level memory changes receive the relevant
   sanitizer pass.
7. `README.md`, `NEXT.md`, component documentation, and `plan.sqlite3` are
   updated when their stated facts change.
8. The focused change is committed and pushed only according to the branch
   policy in `CLAUDE.md`.

---

## Post-audit remediation batch — ticket #1796, `Hashtable` value-access escapes closed (2026-07-28)

Implementation ticket **#1796** (`REMED-COLL-HASHTABLE-WRITE-ESCAPES`, P3, size
M, category `defect`, area Collections) is **`done`** on branch
`feature/remediation-coll-hashtable-value-access`. It implements design ticket
**#1797** exactly; the durable record is
`docs/HashtableValueAccessSafetyDesign.md`, whose new §34 is the
implementation-complete section. **No new `SR-AUD-*` identifier** — the audit
numbering stays frozen at 364, and every defect closed here was found during
remediation.

**The user granted design §32's four-item approval explicitly and per action**,
in this ticket's own instruction: (1) the public source break, (2) the one silent
semantic change, (3) the silent ABI break requiring a full consumer rebuild, and
(4) the changed exception type on `at()`.

**All four escape routes are closed** — the two #1796 was named after and the two
#1797 found:

| Member | Was | Now |
|---|---|---|
| `IDictionary::getItem(const void*) const` | `void*` into live storage, from a `const` member | **`std::any` by value** |
| `Hashtable::operator[](const std::string&)` | `std::any&`, and a bare read **inserted** | **`ValueReference` proxy** — tracked write, owning read, no insert on read |
| `Hashtable::operator[](const std::string&) const` | did not exist | **`std::any` by value** |
| `Hashtable::at(const std::string&) const` | `const std::any&`, `std::out_of_range` | **`std::any` by value, `KeyNotFoundException`** |
| `Hashtable::setItem(const std::string&, const std::any&)` | did not exist | **new typed tracked setter** |
| `ListDictionaryInternal::getItem` | `void*` | **`std::any`**, boxing the same caller pointer — mechanical only |
| `setItem`/`Add` raw-key `void*` *value* parameter | — | **unchanged, deliberately** (design §13.4) |

**Two corrections to #1797's own record, both against convenience.** The Phase 2
source break is **3 translation units and seven source lines, not five** —
#1797's "5 sites" counted distinct compiler *diagnostics*, and
`ListDictionaryInternalTests.cpp`'s three `int*`-shaped `getItem` comparisons
share one GoogleTest template instantiation, so two of them produced no
diagnostic of their own yet still had to be edited. And **zero `test/consumer/`
fixtures needed migration, not three**: all five pre-existing `Collections.Core`
fixtures compile and run unmodified. Everything else in #1797 reproduced exactly.

**Post-fix evidence.** The nine AddressSanitizer `heap-use-after-free` reports
across fourteen lifetime scenarios are **0**; UBSan **0**; LeakSanitizer **0**
with detection proved active by a 318-byte / 2-allocation deliberate-leak
self-test. Rerun in #1797's exact experiment shape — 8 seed keys, one outstanding
enumerator, 4,000 missing-key reads through `operator[]` — `Count` goes
**8 → 8** where it went **8 → 4,008**, and the enumerator walks **8 of 8**
distinct keys with 0 duplicates where it walked **2,045** and reached **6 of 8**
seeds while throwing nothing and producing no sanitizer report.

**The ABI break was reproduced end to end against the real production
declarations**, with the old headers extracted from git rather than approximated:
mangled name **byte-identical**, vtable slot **unchanged at `0x38`**, no symbol
added or removed, `this` moving `%rdi → %rsi` behind a hidden `sret`. A stale
caller **links with `exit=0` and then segfaults with `exit=139`**, preceded by 14
UBSan misaligned-address diagnostics naming the caller's key pointer used as
`this`. `sizeof(Hashtable)` is **unchanged at 72** and
`sizeof(ListDictionaryInternal)` at **40** — not an object-layout break;
`ValueReference` is 40 bytes and is never stored by the collection. `README.md`
carries the mandatory-full-rebuild breaking-change entry.

Permanent coverage:
`modules/collections/tests/System/Collections/HashtableValueAccessSafetyTests.cpp`,
**55 tests**, parameterised over both `IDictionary` implementations wherever the
assertion is about the interface, clean under ASan + UBSan + LSan. Consumer
fixtures `test/consumer/collections_hashtable_value_access.cpp` (compiled **and
run** against `Collections.Core` alone under `-Wall -Wextra -Wpedantic -Werror`)
and `..._negative.cpp` (**11 of 11** marked alias spellings rejected, verified
per-site by `build-probe/1796_check_negative.py` rather than by the file merely
failing to compile).

Validation: `scripts/local_ci_check.sh build` at **13,657 tests across 37
executables**, zero warnings and zero errors, from a tree **reconfigured from
scratch (`cmake --fresh`) and rebuilt with `--clean-first`** for the silent ABI
break — **626 translation units recompiled, 37 executables relinked, and zero
object files on disk predating the fresh configuration**.
`SharpRuntimeTests_Collections_Core` at **2,371** (was 2,316; +55, exactly the
new suite). 41 physical modules / 90 dependency edges, validator tests 7/7,
catalogue current, `scripts/db_consistency_check.py` clean, `git diff --check`
clean, Doxygen 1.9.8 at **1,940** of the 1,942 ceiling (unchanged), and
`scripts/check_selective_components.sh` **run in full** with a repository-local
`TMPDIR` because public headers changed. `scripts/__pycache__` absent; every
Python tool run with `PYTHONDONTWRITEBYTECODE=1`.

**No new build directory was created.** `CLAUDE.md` rule 10 closes the name set,
so the mandatory clean build reconfigured `build/` itself rather than adding a
`build-abi-1796/` — the per-ticket habit #1794's `build-abi-1794` exemplified and
#1797 ended. Directories used: `build/`, the shared `build-probe/` (this ticket's
artefacts under a `1796_` file prefix), `build-consumer/`, and `build-tmp/` as
`TMPDIR`. **No compilation exceeded three jobs.**

**Still open and explicitly not claimed closed:** `setItem`/`Add`'s raw-key
`void*` *value* parameter (deliberate, design §13.4, with the
`Add("literal", v)` address-key corruption that is the reason); accessor use
after the *collection* is destroyed; a `ValueReference` outliving its table (the
port-wide borrow rule — documented on the class, not enforced);
`const std::any& r = h[k];` still compiling and now meaning a snapshot (the one
silent meaning change, documented in `README.md` with the instruction not to
write it). A **pre-existing, unrelated** finding was observed and **recorded
rather than fixed**: `CollectionVersionAccess<Hashtable>` and
`CollectionVersionAccess<ListDictionaryInternal>` are explicitly specialised with
*different* bodies in two translation units of one binary
(`CollectionVersionCounterTests.cpp`'s `SR1787_SEAM_BODY` has
`positionVersion`; `DictionaryEnumeratorKeyValueSafetyTests.cpp`'s
`SR1794_SEAM_BODY` does not), which is IFNDR. It predates this ticket, is benign
in practice, and #1796 deliberately did not make it worse — the new suite spells
its specialisation token-for-token identically to the `SR1794` one. Fixing it is
outside #1796's approval.

Tickets #1773, #1788, #1789, #1791 and #1798 remain `blocked` and untouched;
**#1791 was not implemented and no shared List/Hashtable proxy was introduced**,
so #1797 §24's four measured incompatibilities and its recommended
**#1796-before-#1791** order stand. #1790, #1792, #1793, #1794, #1795 and #1797
remain `done`, and none of them was reopened. CNA and mobile-eggbert were not
inspected, searched, configured, built, or modified, so the source-break figures
here are *this repository only*. No push, merge, rebase, tag, or publication
occurred.

## Completed tracked List indexer mutation: ticket #1791

Ticket #1791 (`REMED-COLL-LIST-INDEXER-VERSION-IMPLEMENT`, P2, size L, `defect`,
area `Collections`) implemented the architecture design ticket #1790 selected,
under the **exact four-part approval written verbatim in
`docs/ListIndexerVersioningDesign.md` §28**, granted by the user and scoped to
#1791 only. The implementation record is §§29-39 of that document, appended below
#1790's, which is preserved unedited. No new `SR-AUD-*` identifier was issued.

Real .NET's `List<T>` index setter advances `_version` unconditionally, so
`list[i] = value` fails an in-progress enumeration fast; this port's `operator[]`
returned a plain `T&`, which no C++ mechanism can intercept, so an indexed write
was invisible to the fail-fast guard and a retained reference was a reproduced
use-after-free. The non-const indexer of `IList<T>`, `List<T>`,
`ObjectModel::Collection<T>` and `ObjectModel::ReadOnlyCollection<T>` now returns
`System::Collections::detail::ElementReference<T>`, a 16-byte prvalue proxy that
reads as `const T&` and routes every write through the mutation counter;
`getItem`/`setItem` were added as pure virtuals on `IList<T>` and implemented by
all four implementers. `list[i] = v` still compiles and now invalidates.

Three corrections to the design are recorded in §30.3: the mutable
`List<T>::ToVector()` was **removed with no public replacement** rather than
merely re-documented, because the approval forbids any ordinary public API
returning a mutable `std::vector<T>&`; `begin()`/`end()` were **kept** as the
documented STL-interop residual, so **the ticket claims the last *ordinary*
untracked write path is closed, not the last one**; and a constrained forwarding
`operator=(U&&)` was added after measuring one heap allocation per
`stringList[i] = "literal"` write with only the `T`-typed overloads.

Measured: `sizeof(List<T>)` **40 → 40**, `sizeof(Collection<T>)` **32 → 40**,
`sizeof(ReadOnlyCollection<T>)` **24 → 24**, `IList<T>` vtable **14 → 16** slots,
4 symbols removed and 18 added, source break **1 site in 1 of 631 translation
units** (the hand-written implementer, as #1790 predicted against 625), all 61
measured indexer call sites still compiling. Runtime cost is at the measurement
noise floor and nothing allocates; the real cost is that reference-based in-place
member access is gone and copy-modify-set copies the element twice.

Three residual hazards are stated rather than concealed: `begin()`/`end()` still
yield an untracked mutable `T&`; a *retained* proxy still aliases a slot across
reallocation; and **a stale object file links with no diagnostic, does not crash,
reads correct values, and silently loses mutation tracking**, measured at `-O0`
and `-O2` in both link orders — which is why the full consumer rebuild is
mandatory. `Collection<T>` also gained a fail-fast enumerator, having
version-checked nothing before, not even `Add()`.

Validation from a fresh configure plus clean-first rebuild at **three jobs** (633
objects, 0 predating the marker, 37 of 38 executables relinked, 0 warnings, 0
errors): `Collections.Core` **2,554**, full repository **13,840 across 37
executables**, negative consumer fixtures **8 / 51** all rejected plus 37/37
self-test, version-seam ODR **2 seams / 18 specialisations** plus 12/12
self-test, module graph **41 / 90**, Doxygen **1,940** of the 1,942 ceiling, the
full ten-component selective matrix and the new positive fixture passed,
ASan/UBSan/LSan `Collections.Core` 2,554 with zero reports and LSan proved active,
`git diff --check` clean, local CI gate passed. TSan was not run, for the reason
design §19 gave.

Tickets #1773, #1788, #1789 and #1803 remain `blocked` and untouched; **no shared
List/Hashtable proxy was introduced**, so #1797 §24's four measured
incompatibilities stand. #1790 and #1792–#1802 remain `done` and none was
reopened. CNA and mobile-eggbert were not inspected, searched, configured, built,
or modified, so the source-break figures here are *this repository only*. No
push, merge, rebase, tag, or publication occurred.

### Completed LinkedList mutation-counter widening: ticket #1788

Ticket **#1788** (`REMED-COLL-LINKEDLIST-VERSION-WIDEN`, P3, size S, `defect`,
area `Collections`) widened `System::Collections::Generic::LinkedList<T>`'s
private mutation counter — and, in the same change, its `Enumerator`'s snapshot —
from 32 to 64 bits. It was opened `blocked` by ticket #1787 and began only after
the user granted the exact approval `docs/CollectionVersionCounterSweep.md` §8.1
had asked for. **No new `SR-AUD-*` identifier**; the numbering stays frozen at
364. The implementation record is **§19** of that document, appended below
#1787's, which is preserved unedited.

**The defect, reproduced before anything changed.** #1787 removed the
signed-overflow undefined behaviour from every counter but could not widen this
one without growing a public object, so it left a 2^32 enumerator-snapshot ABA:
after 2^32 effective mutations the counter returned to a value an outstanding
`Enumerator` had captured and the equality guard silently accepted it. Because
that enumerator holds a raw `data_t*` into `shared_ptr`-owned node storage, the
consequence was a potential **use-after-free**, not merely a wrong answer, and at
~10^8 mutations/second the horizon was about **43 seconds** of hot mutation.
`build-probe/1788_prefix_defects.log` reads `guard-fired=0` three times
(`LinkedList<int>`, `LinkedList<std::string>`, and `Reset()`),
`defects-observed=3`; the identical source post-fix reads `guard-fired=1` and
`defects-observed=0`. UBSan reported **0** runtime errors on **both** sides,
confirming #1787 had already closed the UB and that this ticket closed only the
remaining logical horizon.

**Measured, not assumed.** `sizeof(LinkedList<T>)` **40 → 48** for every `T`,
`alignof` unchanged at 8, `head_`/`tail_`/`count_` offsets unchanged;
`sizeof(Enumerator)` **unchanged at 40**, because its wider snapshot landed in
padding it already had, with every other enumerator member keeping its offset;
`sizeof(LinkedListNode<T>)`, both iterators and `LinkedListNodeData<T>`
unchanged; and **0 `LinkedList` symbols added, removed or renamed** — 796 on both
sides with byte-identical name lists. The only symbol delta anywhere is five
weak inline members of the counter class swapping from the `unsigned int`
instantiation to the `unsigned long` one.

**The break is binary-only and silent, which was reproduced rather than
asserted.** No public signature, return type, parameter or `const` qualification
changed, and every in-repository call site compiles unmodified. But an object
file compiled against the old header links with a new one **with zero
diagnostics in both link orders** — and then, depending on which won the COMDAT
race, either takes an AddressSanitizer heap-buffer-overflow and a SEGV, or
silently corrupts the member following an embedded `LinkedList<T>` **with no
sanitizer report at all**, or silently loses mutation invalidation
(`guard-fired=0`); in the other order everything appears to work. A complete
consumer rebuild is therefore mandatory, and `README.md` says the linker will not
warn you.

**A weakness in #1787's own pin was found and fixed, not concealed.** Flipping
`LinkedListAdapter::kNarrowCounter` back to `true` as a mutation check failed
only *one* test. #1787's narrow branch positioned the counter at
`static_cast<Value>(snapshot)` rather than at `snapshot + 2^32` — identical for a
32-bit field, but true for a counter of any width, so it pinned nothing about the
residual its comment described. It now spells the full distance and asserts the
truncation itself, which makes it load-bearing for `BitArray` and for #1789.

Validation from a fresh configure plus a clean-first rebuild at **three jobs**
(634 objects — 630 C++, 4 C — **0 predating the fresh-configure marker**, 37 of
38 executables relinked, 0 warnings, 0 errors; the exception is the
`EXCLUDE_FROM_ALL` `build/SharpRuntimeTests`, an 85 MB historical binary outside
the gate that is now definitively stale): `Collections.Core` **2,594** (was
2,554), full repository **13,880 across 37 executables** (was 13,840), negative
consumer fixtures **8 / 51** all rejected plus 37/37 self-test (none added),
version-seam ODR **2 seams / 18 specialisations** plus 12/12 self-test (none
added — the new suite includes the one authoritative seam file), module graph
**41 / 90** unchanged, Doxygen **1,941** of the 1,942 ceiling with the single new
warning attributed to the one new `README.md` link into `docs/`, the full
ten-component selective matrix plus the new `Collections.Core` fixture passed,
ASan/UBSan/LSan `Collections.Core` **2,594 with zero reports** and LSan proved
active by a bounded self-test (336 bytes in 7 allocations, exit 1), a 200,000-node
teardown at the boundary clean under ASan, `git diff --check` clean, local CI gate
passed. **TSan was not run**: no atomic, no `mutable` cache, no hidden `const`
write, and `LinkedList<T>` claims no thread safety before or after.

Tickets #1773, #1789 and #1803 remain `blocked` and untouched — in particular
`BitArray` keeps its 2^32 residual, deliberately, because closing it grows the
**public** `BitArray::Enumerator` and that is #1789's separate approval. #1790,
#1791 and #1792–#1802 remain `done` and none was reopened. CNA and
mobile-eggbert were not inspected, searched, configured, built or modified, and
no claim is made about whether they use `LinkedList<T>`. No push, merge, rebase,
tag, or publication occurred.

### Completed BitArray mutation-counter widening: ticket #1789

Ticket #1789 (`REMED-COLL-BITARRAY-VERSION-WIDEN`, P3, size XS, `defect`, area
`Collections`) closed the **second and last** of the two residuals ticket #1787
had to leave open, after the user granted the **exact object-size approval**
[`docs/CollectionVersionCounterSweep.md`](docs/CollectionVersionCounterSweep.md)
§8.2 asked for, scoped to #1789 only. The implementation record is **§20** of that
document; §§1–19 are #1787's and #1788's and are preserved unedited, so the record
does not pretend `BitArray` was always 64-bit. **No new `SR-AUD-*` identifier**;
the numbering stays frozen at 364.

`BitArray::version_` moved from the 32-bit `detail::NarrowMutationCounter` to the
64-bit `detail::MutationCounter`, and `BitArray::Enumerator::version_` from
`NarrowMutationVersion` to `MutationVersion` **in the same change**. Both
together, deliberately: widening the container alone would turn the guard's
comparison into a silent truncation and leave the 2^32 alias in place while the
code claimed otherwise — the failure mode §8.2 identified and refused. Nine
increment sites (`Set`, `SetAll`, `Not`, `And`, `Or`, `Xor`, `LeftShift`,
`RightShift`, `setLengthProperty`) plus the implicitly declared copy/move
assignment, and three read/compare sites, are all unchanged in spelling; the
production diff is two field declarations plus documentation. `BitArray` has no
`Clear()`, no `Add`, and a `const`-only `operator[]`, so `Set` is its sole indexed
write path.

**The defect was reproduced before any production change.** Pre-fix,
`build-probe/1789_prefix_defects.log` shows `truncated-onto-snapshot=1` and
`guard-fired=0` for `MoveNext`, for `Reset()`, and at seven laps of 2^32 —
`defects-observed=3`. The identical source post-fix reads `guard-fired=1` and
`defects-observed=0`, and the entire diff of the two logs is the counter width,
those three outcomes, and one sentinel probe reaching a larger maximum; **every
mutation-delta line and every ordinary-invalidation line is byte-identical**.
UBSan reported **0** runtime errors on both sides: `BitArray` is the one
collection whose counter was already unsigned before #1787 (`std::uint32_t`,
diverging from .NET's signed `int` at `BitArray.cs:44`), so it never had the
signed-overflow UB and this ticket closed only the remaining *logical* ABA
horizon. Unlike `LinkedList<T>`'s, the consequence was a **wrong answer rather
than a use-after-free** — the enumerator holds an index bounds-checked against the
current length on every step — which is why this was P3. At ~10^8
mutations/second 2^32 is about **43 seconds**.

Measured, not estimated: `sizeof(BitArray::Enumerator)` **32 → 40**, `alignof`
unchanged at 8, `arr_` keeping offset 8 while the snapshot at 16 widens and
`index_`/`current_`/`state_` each move by 8 — nine bytes are needed after an
eight-byte snapshot where eight are available, in any member order, exactly as
§8.2 predicted; `sizeof(BitArray)` **unchanged at 48**, because the wider counter
landed in the four bytes of tail padding the container already had, so
`PublishedObjectSizesAreUnchanged` still asserts 48 and is still telling the
truth; **0 `BitArray` symbols added, removed or renamed** (64 on each side,
byte-identical name lists), the only symbol delta anywhere being the counter
class's seven weak inline members swapping from the `<unsigned int>` to the
`<unsigned long>` instantiation. No public signature changed and every
in-repository caller compiles unmodified.

**The break is binary-only and silent, and that was measured.**
`BitArray::Enumerator` is a **public** nested class, so a consumer may name one
and store it by value. An object file compiled against the old header links with
a new one producing **no diagnostic in any of eight configurations** (`-O0`/`-O2`
× both link orders × with and without ASan+UBSan). Then, depending on which
definition won the COMDAT race, it either silently corrupts the member following
an embedded enumerator — a sentinel went from `0xFEEDFACECAFEBEED` to
`0xFEEDFACE00000002`, with **no AddressSanitizer report at all**, because the
bytes are inside the same allocation — or, at `-O2`, silently reports **zero
elements for an eight-bit array**, or aborts on a `new-delete-type-mismatch`
("allocated 32 bytes, deallocated 40") under ASan. At `-O0` one of the two link
orders looks entirely healthy. Notably the **fail-fast guard keeps firing in every
configuration**, so a consumer cannot use that as evidence it rebuilt. A complete
consumer rebuild is mandatory and `README.md` now says so in those terms.

**The adapter flip was mutation-checked.** Putting
`BitArrayAdapter::kNarrowCounter` back to `true` and rebuilding fails **two**
tests — `TheCounterHasTheWidthItsLayoutPermits` *and*
`NoStaleSnapshotBecomesValidAcrossTheOld2Pow32Distance`. Only the first would have
failed before #1788 corrected that assertion to spell the full `snapshot + 2^32`
distance (§19.11), so that correction is what made this flip load-bearing rather
than cosmetic.

**One performance figure is disclosed rather than waved through.** The
`RightShift(1)` benchmark row moved +88 ns/op (~8%) and reproduced across fourteen
paired runs — two non-overlapping ranges, so not noise. It is **not** the counter:
`BitArray::RightShift`'s generated code is instruction-for-instruction identical
on both sides (130 lines of `objdump` output each), the only codegen difference
anywhere being 32- to 64-bit `mov`s inside `BasicMutationCounter::operator++`; and
recompiling **both** sides with `-falign-loops=32 -falign-functions=64` inverts
the sign, making the post side 197 ns *faster*. It is `-O2` code alignment. Every
other row straddles zero and **allocation counts are identical in every row**.

New permanent suite `BitArrayVersionWideningTests.cpp` (**+43** cases, all
boundary positioning through #1800's one authoritative seam, which already carried
a `BitArray` specialisation, so no new specialisation body was written) and a new
tracked consumer fixture `test/consumer/collections_bitarray_version.cpp`,
compiled with `-Wall -Wextra -Wpedantic -Werror` and run. All 21
`BitArrayTests.cpp` cases and the `Batch18`/`Batch18b` gap-fills pass
**unmodified**.

Validation from `cmake --fresh` plus a clean-first rebuild at **three jobs** (635
objects, **0** predating the fresh-configure marker, 37 of 38 executables
relinked, 0 warnings, 0 errors — the exception being the `EXCLUDE_FROM_ALL`
`build/SharpRuntimeTests`, an 85 MB historical binary from 2026-07-24 outside the
gate, left untouched and still stale): `Collections.Core` **2,637** (was 2,594);
full repository **13,923 across 37 executables** (was 13,880); negative consumer
fixtures **8 / 51, every site rejected** plus 37/37 self-test, none added and none
needed since no public signature changed; version-seam ODR **2 seams / 18
specialisations** plus 12/12 self-test, none added; module graph **41 / 90**
unchanged; Doxygen **1,941** of the 1,942 ceiling, **unchanged** — the new
`README.md` entry deliberately refers to the sweep document as a code span rather
than a markdown link, because every `README.md` → `docs/` link costs one
unresolvable `\ref` warning; the full ten-component selective matrix and the new
`Collections.Core` fixture passed; ASan/UBSan/LSan `Collections.Core` **2,637 with
zero reports**, LSan proved active by a bounded self-test reporting 96 bytes in 3
allocations, and a 200,000-bit boundary-positioned walk clean; `git diff --check`
clean; the local CI gate passed. **TSan was not run** — no atomic, no `mutable`
cache, no hidden `const` write, and no thread-safety claim is made for `BitArray`
before or after; `getIsSynchronizedProperty()` still returns `false`.

**With this ticket, no collection in this repository retains a 2^32
enumerator-snapshot ABA horizon** — every one is 2^64, and
`detail::NarrowMutationCounter` has no user left (it is kept as the historical
record and as the second instantiation the counter tests pin).

**CNA and mobile-eggbert were not inspected, searched, configured, built or
modified**, no claim is made about whether they use `BitArray`, and **#1773
remains `blocked`**. #1803 remains `blocked` and untouched.

### Completed SortedSet seam consumer guard: ticket #1803

Ticket #1803 (`REMED-TOOLING-SORTEDSET-SEAM-NEGATIVE-FIXTURE`, P3, size XS,
`tooling`, area *Developer experience*) is **done**. Durable record:
[`docs/NegativeConsumerFixtureValidation.md`](docs/NegativeConsumerFixtureValidation.md)
**§18**, appended below #1801's §§1-17, which are preserved unedited — in
particular §16.4 item 4, which is this ticket's own charge sheet and still reads
"`SortedSetVersionAccess` has no consumer-side fixture". **No new `SR-AUD-*`
identifier**; the numbering stays frozen at 364.

**This is test/tooling work only.** No production source, signature, symbol,
object layout, vtable, exception contract or collection semantic changed; not one
file under any `modules/*/include` or `modules/*/src` was touched, and CMake
metadata, the component graph and every include directory are unchanged. #1800
and #1801 stay `done` and neither was reopened; `scripts/check_version_seam_odr.py`,
`scripts/check_negative_consumer_fixtures.py`,
`modules/collections/tests/support/CollectionVersionSeam.hpp` and
`test/consumer/CMakeLists.txt` are unmodified.

**What was missing.** `SharpRuntime::Testing::SortedSetVersionAccess<T>` is
declared, never defined, and befriended by `SortedSet<T>`, so #1786's regressions
can position the shared 64-bit counter and read #1784's atomic Count cache. #1800
pinned that it has exactly one definition site — a test translation unit,
`SortedSetVersionOverflowTests.cpp` — by reading source text. What no tracked job
did was **compile a consumer that tries to use it**. `CollectionVersionAccess` had
that half, from `test/consumer/collections_mutation_version_negative.cpp`;
`SortedSetVersionAccess` did not. Nothing was broken: this was a coverage
asymmetry, and it stayed one.

**The intended restriction, now written down and pinned:** an ordinary consumer —
compiling against a component's declared public include surface, with no flag that
disables access control, including nothing under `modules/*/tests`, and not
authoring a specialisation in a namespace it does not own — can neither name a
*complete* `SortedSetVersionAccess<T>` nor reach, by any other route, the state
that seam exists to reach.

**Inventory first, fixture second.** `build-probe/1803_threat_probe.py` compiled
**29** candidate consumer expressions, one per translation unit, against the
resolved `Collections.Core` surface. Three are accepted and 26 rejected. Two of
the three acceptances are intended and are deliberately **not** sites — naming the
incomplete type, and declaring a pointer to it, obtain nothing, which is what a
forward declaration is for. The third is §18.5 below.

**`test/consumer/collections_sorted_set_version_negative.cpp`, 15 sites**, not the
two the ticket's row estimated: five prove the seam is an **incomplete type**
(`version`, `positionVersion`, `cachedTag`, an object definition, the `::Set`
member type), nine prove there is no second route to the same storage (`state_`,
the nested `State`, `bumpVersion()`, `cachedCount_`, `cachedCountVersion_`,
`countCacheTag()`, `kMaxCacheableVersion`, `kCountNotCached`, and
`Iterator::version_` — each `is private within this context`), and one proves the
defining translation unit is not reachable through any public include path. A
sibling fixture rather than two more sites in #1787's file, so `--list` output,
blame and diagnostics stay attributable to one ticket each. **One correction to
the row itself**: its acceptance criteria asks for
`SortedSetVersionAccess<SortedSet<int>>`; the seam is parameterised by the
**element** type, so the contract spelling is `SortedSetVersionAccess<int>`. The
row's spelling is also rejected and was measured, but pinning it would have pinned
a misspelling.

**Two measurements are disclosed rather than filed under noise.**

1. **#1800's checker exits 0 on a real seam exposure.** Both checkers were run
   against identical mirror repositories (`build-probe/1803_gap_probe.py`). Give
   the seam's **primary template** a body in `SortedSet.hpp` and
   `check_version_seam_odr.py` says `OK`, silently reporting 1 seam and 17
   definitions instead of 2 and 18 — because its discovery rule is "declared and
   **not defined**", so a defined seam stops being a seam and rule 1 never fires;
   its vacuity guard fires only at **zero** seams. Making `SortedSet<T>::state_`
   public likewise exits 0. #1803's fixture fails on both, naming five sites and
   one site respectively. Nothing is wrong in the repository today and #1800 is
   **not reopened** — the two checks are complementary by design, which is the
   measurement rather than the argument. Strengthening the vacuity guard is
   inactive ticket **#1804** (`REMED-TOOLING-SEAM-DISCOVERY-VACUITY`, `blocked`).
2. **One restriction cannot be expressed, and it is not SortedSet's.** A consumer
   that reopens `namespace SharpRuntime::Testing` and writes its own explicit
   specialisation *does* obtain the access the friend declaration grants, and
   compiles clean under `-Werror` against the public headers alone — for
   `SortedSetVersionAccess<int>` and, measured identically, for
   `CollectionVersionAccess<List<int>>`. That is well-formed ISO C++; a
   `friend class X;` is open to whoever writes `X`, and no seam design in C++
   avoids it. It is unsupported and is recorded in §18.5 instead of being assumed
   away. `SortedSet.hpp`'s own doc-comment — nothing a consumer *links against*
   can observe or call the seam — remains literally true.

**Load-bearing, proved.** `build-probe/1803_mutation_campaign.py` applies **ten**
temporary header mutations, each shadowed in a mirror root whose modules are
per-file symlinks, and requires the tracked checker to fail naming **exactly** the
expected site set. **10/10 caught, 0 failures**, covering all fifteen sites; the
unmutated mirror exits 0. No tracked file was modified at any point. Two earlier
campaign runs failed because the mutation itself broke the header, and the checker
correctly reported a **baseline** failure rather than a site result — rule 7 doing
its job on the campaign is the best available evidence that "the build broke"
cannot be mistaken for "the seam was exposed".

Validation, incremental by CLAUDE.md rule 12 (no `modules/` file, no
`CMakeLists.txt` and no component metadata changed, so no object in `build/` can
be stale and a clean-first rebuild would have written a full tree of objects to
re-derive an unchanged answer): `scripts/local_ci_check.sh build` **passed**, 0
warnings, 0 errors, and it **executes the new fixture automatically** — its
negative-fixture phase now reads *9 fixtures, 66 sites, every site rejected*, 75
compiler invocations, peak 3 jobs. Full repository **13,923 tests across 37
executables**, unchanged; `Collections.Core` **2,637**, unchanged; negative
consumer fixtures **9 / 66, every site rejected** (was 8 / 51) plus **37/37**
self-test, unchanged because the checker itself was not modified; version-seam ODR
**2 seams / 18 specialisations** plus **12/12** self-test; module graph **41 /
90**; component catalogue current; database consistency clean; the full
ten-component selective matrix passed with its 3 forbidden fixtures rejected;
Doxygen **1,941** of the 1,942 ceiling, unchanged; `git diff --check` clean.

**Sanitizers are not applicable and none was built** — the deliverable is one
compile-only fixture and documentation, with no production code, no new runtime
code, no new allocation, no new thread and no new CTest case; ASan, UBSan, LSan
and TSan cannot observe a compile-rejection contract, and #1784's and #1786's
existing `SortedSet` sanitizer coverage stands unre-measured. **No compilation
exceeded three jobs**, in any script, including inside the tracked checker, which
refuses a higher request. Build directories used: `build/` (gate), `build-probe/`
(this ticket's probes, mirrors and logs, all `1803_` prefixed), `build-tmp/`
(repository-local `TMPDIR`); **no new build directory was created**. The stale
`EXCLUDE_FROM_ALL` `build/SharpRuntimeTests` binary was neither executed, trusted,
nor deleted.

**CNA and mobile-eggbert were not inspected, searched, configured, built or
modified**, and **#1773 remains `blocked`**. #1788, #1789, #1790, #1791 and
#1792–#1802 all remain `done` and none was reopened. The one new inactive row is
**#1804**.

### Completed MemoryStream raw-buffer validation: ticket #1805

Ticket #1805 (`REMED-IO-MEMORYSTREAM-NULL-BUFFER`, P1, size S, `remediation`,
area *IO*) is **done** and **SR-AUD-341 is now `remediated`**. It is the first
ticket of the post-audit remediation phase to leave `Collections`: NEXT.md's
recommended dependency order names it, with SR-AUD-338, as one of the two
self-contained ASan/UBSan-backed public-input repairs to take after the
collection safety contracts were settled. **No new `SR-AUD-*` identifier** — the
audit numbering stays frozen at 364; the index now records **11 remediated** and
**353 confirmed** of 364.

**Selection note.** The ticket queue in `plan.sqlite3` was **empty** at the start
of this ticket: every row was `done` except #1773 and #1804, both `blocked` and
both correctly so. The remediation backlog is not empty, though — it lives in
`audit/AUDIT_FINDINGS_INDEX.md`, and converting the next roadmap item into a
ticket is how every remediation ticket since #1767 has begun. #1805 was created
that way, not discovered.

**What was wrong.** `MemoryStream(const bytecs* buffer, intcs size, bool writable)`
initialized `data_(buffer, buffer + size)` in its member-initializer list, so the
copy ran ahead of every check. Reproduced before any production change, one
process per input so a crash in one case could not hide another
(`build-probe/1805_prefix_defects.cpp`, log `build-probe/1805_prefix_defects.log`):

| Input | Pre-fix | Post-fix |
|---|---|---|
| `(nullptr, 1)` | UBSan *load of null pointer of type `const unsigned char`*, then **ASan SEGV on address 0x0**, exit 1 | `ArgumentNullException` — *Value cannot be null. (Parameter 'buffer')* |
| `(nullptr, 0)` | constructed, `length=0` | **byte-identical** |
| `(data, -1)` | `std::length_error` — *cannot create std::vector larger than max_size()* | `ArgumentOutOfRangeException` — *Non-negative number required. (Parameter 'size')* |
| `(data, 3)` | constructed, `length=3` | **byte-identical** |

**A second defect in the same constructor is disclosed rather than filed under
noise.** The finding text named only the null dereference. A negative `size` was
also unvalidated, and it did not merely produce a wrong answer: it formed
`buffer + size` — out-of-bounds pointer arithmetic, undefined in its own right —
and then constructed a vector from a reversed range, which escaped as a raw
`std::length_error`. A standard-library exception was crossing a public API whose
entire purpose is to mirror .NET's argument diagnostics. The same change closes
it, and a permanent test catches `std::length_error` *first* so the assertion
fails if it ever comes back.

**The repair.** `data_` is now initialized from a file-local
`validatedBufferCopy(buffer, size)` that validates and then copies, so nothing
invalid reaches pointer arithmetic or the vector range constructor. Null takes
precedence over a bad size, matching .NET's own ordering, in which
`ArgumentNullException.ThrowIfNull(buffer)` precedes the count check — so
`(nullptr, -1)` reports the null. An anonymous-namespace helper in the `.cpp` was
chosen over a member function so that **no header, signature, object layout,
vtable or exported symbol changed**; the sibling `UnmanagedMemoryStream.cpp` in
the same module already validates that way.

**One input is deliberately still accepted, and that decision is load-bearing.** A
null pointer paired with a size of **zero** remains valid. This port's parameter
is a pointer/length pair, not .NET's `byte[]` object: `(nullptr, 0)` is the
ordinary spelling of an empty range, `std::vector<bytecs>().data()` is permitted
to return null and does on this toolchain, and `BinaryData::ToStream()` reaches
this constructor exactly that way for empty content. The pre-fix probe's case 2
shows the input was already well defined and already produced a correct empty
stream, so rejecting it would have been a **regression, not a repair** — which is
precisely the correction ticket #1774 had to make after #1771 over-rejected the
same shape on `ICollection::CopyTo`. `UnmanagedMemoryStream` diverges from both
and rejects null unconditionally, because it *retains* the caller's pointer and
.NET's own `UnmanagedMemoryStream` rejects it unconditionally too; that
divergence is now stated in the header doc-comment rather than left to be
inferred.

**Tests: +14 permanent regressions.** Thirteen in
`modules/io/tests/System/IO/StreamTests.cpp` — null/positive (the audited input),
null/large, the parameter name in the message, null-before-negative precedence,
null/zero, null/zero still writable afterwards, `std::vector::data()` on an empty
vector, negative, the explicit no-`std::length_error` assertion, `INT32_MIN`, zero
size with a non-null source, the unchanged valid path, and the source-lifetime/copy
independence the audit report itself listed as missing. One in
`modules/io/tests/System/BinaryDataTests.cpp` pinning `BinaryData::Empty().ToStream()`,
the in-repository caller that makes the accepted `(nullptr, 0)` rule load-bearing:
delete the rule and that test fails.

**Validation.** `SharpRuntimeTests_IO` **541/541** (was 527), and the same 541
under **AddressSanitizer + UndefinedBehaviorSanitizer + LeakSanitizer with zero
reports** (`build-asan/1805_io_asan.log`). LeakSanitizer was **proved active**
rather than assumed: `build-probe/1805_lsan_selftest.cpp` is reported as a
4,096-byte definite leak. The first version of that self-test leaked from a
pointer still live on the stack at exit, which LSan correctly classifies as *still
reachable* and does not report — it proved nothing, and was replaced rather than
believed. Repository gate `scripts/local_ci_check.sh build`: **0 warnings, 0
errors**, **13,937 tests across 37 executables** (was 13,923), including the six
local-server `Net.Http` cases, which were network-permitted in this run. Module
graph **41 / 90**; catalogue current; database consistent; version-seam ODR
**2 seams / 18 specialisations** plus 12/12 self-test; negative consumer fixtures
**9 / 66, every site rejected** plus 37/37 self-test; the ten-component selective
matrix passed with its 3 forbidden fixtures rejected; Doxygen **1,941** of the
1,942 ceiling, **unchanged** — the header gained two `@throws` lines, which
Doxygen resolves; `git diff --check` clean.

**No consumer fixture was added**, deliberately. The existing ones exist for
contracts a consumer can only be *shown* through the public headers — ABI shape,
seam reachability, compile rejection. This ticket changes no signature and outlaws
no spelling; the GoogleTest suite exercises the identical public constructor, and
a fixture would have rebuilt the whole `IO` component to re-assert it.

**Source and ABI consequences: none.** No public signature, object layout, vtable,
inheritance or exported symbol changed, so **no consumer rebuild is required on
this ticket's account**. One behavioural note belongs in a consumer's release
notes: a caller that was catching `std::length_error` to detect a negative size no
longer catches it. That spelling was an accident of the vector range constructor,
never a contract, and no in-repository caller relied on it.

**Not closed by this ticket, and said so in the audit report:** the second bullet
of that report's "Missing assertions and diagnostics" — the absent near-limit
capacity/position diagnostic — needs a multi-gigabyte allocation to exercise and
is not part of SR-AUD-341's crash contract.

Build directories used: `build/` (gate), `build-asan/` (the pre-existing
sanitizer tree, which gained the `SharpRuntimeTests_IO` target it did not have),
`build-probe/` (this ticket's probes and logs, all `1805_` prefixed),
`build-tmp/` (repository-local `TMPDIR` for the `mktemp`-based gate, Doxygen and
selective-matrix scripts); **no new build directory was created**. **No
compilation exceeded three jobs.**

**CNA and mobile-eggbert were not inspected, searched, configured, built or
modified**, and **#1773 remains `blocked`**. #1804 remains `blocked` and
untouched. No previously `done` ticket was reopened and no finding was reopened.

### Completed text-wrapper null-stream validation: ticket #1806

Ticket #1806 (`REMED-IO-TEXT-WRAPPER-NULL-STREAM`, P1, size S, `remediation`,
area *IO*) is **done** and **SR-AUD-338 is now `remediated`**. It is the second
of the two self-contained ASan/UBSan-backed public-input repairs NEXT.md's
recommended dependency order names, taken directly after #1805. **No new
`SR-AUD-*` identifier**; the numbering stays frozen at 364, and the index now
records **12 remediated** and **352 confirmed** of 364.

**The finding named one dereference. There were five.** Measured before any
production change, one process per case so a crash could not hide another
(`build-probe/1806_prefix_defects.cpp`, logs `1806_prefix_defects.log` and
`1806_postfix_defects.log`). Pre-fix, on a null base stream:

| Call | Pre-fix | Post-fix |
|---|---|---|
| `StreamReader` `Read` / `Peek` | `-1` | `ArgumentNullException` at construction |
| `StreamReader` `ReadLine` / `ReadToEnd` | `""` | same |
| `StreamWriter::Write(std::string)` | UBSan *member access within null pointer of type `struct Stream`*, ASan **SEGV on 0x0** | same |
| `StreamWriter::Write(const char*)` | same | same |
| `StreamWriter::Flush()` | same | same |
| `StreamWriter::Close()`, `leaveOpen=false` | same | same |
| `~StreamWriter()`, `leaveOpen=false` | same | same |
| `BinaryReader(nullptr)` / `BinaryWriter(nullptr)` | `ArgumentNullException` **already** | unchanged |

The destructor case is the sharpest and is **not** in the finding text: with the
**default** `leaveOpen = false`, merely constructing a `StreamWriter` over a null
stream and letting it leave scope was fatal, because the destructor closed a
stream it did not have. No call on the object was required.

**The reader's half is not a crash, and its guards were removed rather than
kept.** `Read()`/`Peek()` returning `-1` and `ReadLine()`/`ReadToEnd()` returning
`""` are exactly what an **empty document** returns, so a programming error was
silently laundered into ordinary, plausible data — a caller could not tell "there
was no stream" from "there was nothing in it". Both constructors now throw
`ArgumentNullException("stream")`, matching .NET, whose every `Stream`-taking
constructor of both types opens with `ArgumentNullException.ThrowIfNull(stream)`,
and matching the sibling `BinaryReader`/`BinaryWriter` **in the same module**,
whose already-correct behaviour the audit called out as making the divergence
especially hazardous. With that check in place `stream_` is non-null for the
lifetime of every `StreamReader` — the only other constructor assigns a freshly
allocated `FileStream`, and nothing else writes the member — so the
`stream_ == nullptr` tests in `Peek()`, `Read()`, `Close()` and the destructor are
**gone**, rather than left behind as unreachable code implying a state that can no
longer exist.

**Tests: +11 permanent regressions** in `IOStreamTests.cpp` — reader null, reader
null with `leaveOpen=true`, the reader's parameter name, the same three for the
writer, a cross-type assertion that all four `Stream*`-wrapping types in this
module now answer the identical input identically, the reader's ordinary read
paths re-pinned after its guards were removed, the empty-stream `-1`/`""` meanings
re-pinned so they keep their one remaining legitimate sense, the writer's ordinary
write path, and a check that a rejected construction leaves a neighbouring live
stream untouched — throwing from the constructor body means `~StreamWriter()`
never runs, so the failure cannot close or delete anything.

**Validation.** `SharpRuntimeTests_IO` **552/552** (was 541), and the same 552
under **ASan + UBSan + LSan with zero reports** (`build-asan/1806_io_asan.log`);
LeakSanitizer's activity was established by #1805's self-test earlier in this same
session. Repository gate `scripts/local_ci_check.sh build`: **0 warnings, 0
errors**, **13,948 tests across 37 executables** (was 13,937), including the six
local-server `Net.Http` cases. Module graph **41 / 90**; catalogue current;
database consistent; seam ODR **2 / 18** plus 12/12; negative consumer fixtures
**9 / 66** plus 37/37; the ten-component selective matrix passed with its 3
forbidden fixtures rejected; Doxygen **1,941** of the 1,942 ceiling, unchanged;
`git diff --check` clean.

**Source and ABI consequences: none.** No public signature, object layout, vtable
or exported symbol changed. One behavioural note belongs in a consumer's release
notes: code that constructed either wrapper over a null stream and relied on the
reader's silent end-of-stream now receives `ArgumentNullException` at
construction. No in-repository caller did so, and no test asserted the old
behaviour.

**Explicitly not done here.** **SR-AUD-337** — the `leaveOpen` disposal contract,
which shares these two exact files — is untouched and stays `confirmed`. Rolling
it in would have merged two unrelated contracts into one ticket.

**Two separable defects were found while doing this work and are recorded as
inactive `todo` tickets rather than folded in or concealed:**

- **#1809** (P2) — `TextWriter::Write(const char*)` forms `std::string(value)` and
  `StreamWriter::Write(const char*)` calls `std::strlen(value)`; both are
  undefined for a null pointer, and the same shape reaches `WriteLine(const char*)`
  and every `TextWriter` subclass. It is kept separate because the answer is a
  contract decision, not a guard: .NET's `TextWriter.Write(string?)` treats null as
  a **no-op**, so a throwing guard would diverge from .NET while a silent one would
  match it. The audit did not record this; that is stated plainly rather than
  backfilled into a finding.
- **#1808** (P2) — neither wrapper validates `CanRead`/`CanWrite`, where
  `StreamReader.cs:147` and `StreamWriter.cs:103` throw
  `ArgumentException(SR.Argument_StreamNotReadable / _StreamNotWritable)`. Kept
  separate because rejecting a stream that exists but is *unsuitable* is a
  different contract from rejecting one that does not exist, and because it can
  reject calls that work today — its acceptance criteria require an inventory
  before any implementation.

Build directories used: `build/` (gate), `build-asan/`, `build-probe/` (all
`1806_` prefixed), `build-tmp/` (repository-local `TMPDIR`); **no new build
directory was created** and **no compilation exceeded three jobs**. The ticket's
probe binaries were deleted once their logs were transcribed.

**CNA and mobile-eggbert were not inspected, searched, configured, built or
modified**, and **#1773 remains `blocked`**. #1804 remains `blocked` and
untouched. No previously `done` ticket and no finding was reopened.

### Completed AggregateException null-inner validation: ticket #1807

Ticket #1807 (`REMED-CORE-AGGREGATEEXCEPTION-NULL-INNER`, P1, size S,
`remediation`, area *Core*) is **done** and **SR-AUD-097 is now `remediated`** —
the third of NEXT.md's eight immediate public-input crash tickets to land, after
SR-AUD-089 (#1776), SR-AUD-341 (#1805) and SR-AUD-338 (#1806). **No new
`SR-AUD-*` identifier**; the numbering stays frozen at 364, and the index now
records **13 remediated** and **351 confirmed** of 364.

**The finding named one crash path. There were three, and two silent ones.**
`std::rethrow_exception` is undefined for a null argument and three members call
it, so a single accepted null armed all three at once. Measured before any
production change, one process per case
(`build-probe/1807_prefix_defects.cpp`, logs `1807_prefix_defects.log` and
`1807_postfix_defects.log`):

| Input / call | Pre-fix | Post-fix |
|---|---|---|
| `AggregateException(vector{null})`, `{null}`, `{valid, null}` | **ASan SEGV** in `std::rethrow_exception`, address `0xffffffffffffff80` | `ArgumentException` — *An element of innerExceptions was null.* |
| `AggregateException("m", vector{null})` then `Flatten()` | same SEGV via `collectLeaves` | rejected at construction |
| …then `GetBaseException()` | same SEGV | rejected at construction |
| …then `Handle(always-true)` | **completed**, `predicate-saw-null=1` | rejected at construction |
| `AggregateException("m", exception_ptr())` then `Unwrap()` | **completed**, `unwrapped-null=1` | `ArgumentNullException` — *(Parameter 'innerException')* |
| the two valid constructions | `One or more errors occurred. (boom)`, `count=1 message='outer'` | **byte-identical** |

The two `completed` rows are the ones worth naming. They did **not** crash: the
message-plus-collection and message-plus-single constructors never built a
message from their inner exceptions, so they stored the null quietly, and then
`Handle()` handed it to the caller's predicate and `Unwrap()` returned it. The
crash happened afterwards, in consumer code, at a `std::rethrow_exception` the
consumer wrote, with nothing left to indicate where the null had entered.

The trap address is `0xffffffffffffff80`, not `0x0` — that is what a null
`std::exception_ptr` decodes to inside libstdc++ — and it is written down so a
future reader matching this signature is not sent looking for an ordinary null
dereference.

**The two exception types are deliberately different, and a test pins them
apart.** .NET's private `AggregateException(string?, Exception[], bool)` core
constructor, through which every public collection overload funnels, throws
`ArgumentException(SR.AggregateException_ctor_InnerExceptionNull)` for a null
**element**, while `AggregateException.cs:59` opens
`AggregateException(string?, Exception)` with
`ArgumentNullException.ThrowIfNull(innerException)` for a null **argument**. Since
`ArgumentNullException` derives from `ArgumentException`, a test catching only the
base type would still pass if the two were collapsed onto one type, so one
regression asserts the collection case is **not** caught as
`ArgumentNullException`.

**Tests: +10 permanent regressions** in `ExceptionRemainingTests.cpp` — null in a
vector, null in an initializer list, null after a valid entry, the exact .NET
message text, null through the message-plus-vector constructor, null through the
message-plus-single constructor, its parameter name, the type split, an assertion
that no constructed aggregate (flat, nested, or either flattened) can hold a null
inner, and the unchanged valid paths including the empty-collection default
message.

**Validation.** `SharpRuntimeTests_Core_Base` **4,982/4,982** (was 4,972), and the
same 4,982 under **ASan + UBSan + LSan with zero reports**
(`build-asan/1807_core_base_asan.log`). Repository gate
`scripts/local_ci_check.sh build`: **0 warnings, 0 errors**, **13,958 tests across
37 executables** (was 13,948), including the six local-server `Net.Http` cases.
Module graph **41 / 90**; catalogue current; database consistent; seam ODR
**2 / 18** plus 12/12; negative consumer fixtures **9 / 66** plus 37/37; the
ten-component selective matrix passed with its 3 forbidden fixtures rejected;
Doxygen **1,941** of the 1,942 ceiling, unchanged; `git diff --check` clean.

**Source and ABI consequences: none.** `AggregateException` is header-only and
gains two private static helpers; no public signature, object layout, vtable or
exported symbol changed. Behavioural note: code that constructed an aggregate over
a null `std::exception_ptr` now receives an argument exception at construction
instead of crashing later. No in-repository caller did so —
`CancellationTokenSource` and `Parallel`, the only two producers, both push
`std::current_exception()` from inside a `catch (...)`, where it is never null.

**Explicitly not done here.** **SR-AUD-098** (causal diagnostics and flatten
ordering) and **SR-AUD-099** (`Handle` accepts an empty predicate) share this file
and stay `confirmed`. SR-AUD-099 belongs to **CCF-011**, which the remediation
roadmap requires be taken as a scoped family rather than one file at a time, and
SR-AUD-098 is a different contract entirely. The audit report's remaining
missing-assertion list — first-inner identity, custom-message aggregation, `Handle`
message and order preservation, predicate exceptions, nested/direct leaf order,
the `GetBaseException` value-API adaptation, and the HResult assertion — belongs
to those two findings and is not claimed as closed.

Build directories used: `build/` (gate), `build-asan/` (which gained the
`SharpRuntimeTests_Core_Base` target it did not have; 5m23s at three jobs),
`build-probe/` (all `1807_` prefixed), `build-tmp/` (repository-local `TMPDIR`);
**no new build directory was created** and **no compilation exceeded three jobs**.
The probe binaries were deleted once their logs were transcribed.

**CNA and mobile-eggbert were not inspected, searched, configured, built or
modified**, and **#1773 remains `blocked`**. #1804 remains `blocked`; #1808 and
#1809, opened by #1806, remain `todo` and unbegun. No previously `done` ticket and
no finding was reopened.

### Completed interpolated-handler pointer validation: ticket #1810

Ticket #1810 (`REMED-CORE-INTERPOLATED-HANDLER-NULL-DEST`, P1, size S,
`remediation`, area *Core*) is **done** and **SR-AUD-132 is now `remediated`** —
the fourth of NEXT.md's eight immediate public-input crash tickets. **No new
`SR-AUD-*` identifier**; the numbering stays frozen at 364, and the index now
records **14 remediated** and **350 confirmed** of 364.

**This one is a write.** `TryWriteInterpolatedStringHandler(nullptr, 1)` passed
the capacity check in `appendRaw` — `1 >= 1` — and reached
`std::memcpy(dest_ + pos_, ...)`, an AddressSanitizer-confirmed **write to the
zero page**, with UBSan reporting *null pointer passed as argument 1, which is
declared to never be null*. That is the most severe shape among the remaining
public-input findings, which is why it was taken ahead of them. Measured before
any production change, one process per case
(`build-probe/1810_prefix_defects.cpp`, logs `1810_prefix_defects.log` and
`1810_postfix_defects.log`):

| Input | Pre-fix | Post-fix |
|---|---|---|
| `handler(nullptr, 1).AppendLiteral("x")`, both constructors | **ASan SEGV on 0x0** in `memcpy`, exit 1 | `ArgumentNullException` *(Parameter 'destination')* |
| `AppendLiteral((const char*)nullptr)` | **ASan SEGV** in `strlen`, exit 1 | `ArgumentNullException` *(Parameter 'value')* |
| `handler(nullptr, 0).AppendLiteral("x")` | refused, `success=0` | **byte-identical** |
| fill the buffer then append one more | refused, `written=8 success=0` | **byte-identical** |
| ordinary use | `written=4 success=1 string='x=42'` | **byte-identical** |

**What .NET gets for free, this port must check.** The .NET counterpart takes a
`Span<char>`, which cannot represent a nonempty null destination at all, so there
is no .NET validation to copy — the check restores by validation what the .NET
type gets from its parameter type. A null paired with a capacity of **zero** stays
valid, per the rule tickets #1774 and #1805 settled for the same pointer/length
shape; the probe shows it already behaved correctly.

**The null-literal policy is decided here, not inherited.** The finding's closing
sentence asked for exactly that. In .NET the handler is compiler-generated and
`AppendLiteral` receives only literal text, so no .NET behaviour applies.
`AppendLiteral(const char*)` throws rather than treating null as empty: the
`std::string` overload cannot be null — `""` is already how an empty literal is
spelled — and the `bool` result already means "did it fit", so succeeding silently
would give that result a second meaning and hide the caller's bug. One regression
asserts `""` and `nullptr` behave differently, so a later change cannot quietly
collapse them.

**Two further defects in the same members are closed by the same change**, both
taken from the audit report's own "Other missing assertions" list rather than
found anew:

- the capacity test was `pos_ + len > destLen_`, a `size_t` sum that can **wrap**
  and let an oversized append pass the very check meant to stop it. It is now
  `len > destLen_ - pos_`, which cannot wrap because `pos_ <= destLen_` is an
  invariant of the class;
- `std::memcpy` is undefined for a null pointer even at zero length, and so is
  `std::string(nullptr, 0)` in `getString()`. Both are reachable — `dest_` is null
  for a zero-capacity handler and `data` is null for an empty `std::string` — so
  `appendRaw` returns early at `len == 0` and `getString()` guards the null.

**Tests: +12 permanent regressions** in
`TryWriteInterpolatedStringHandlerTests.cpp`, including one that pins the
four-argument constructor's `shouldAppend` out-parameter as deliberately
**unwritten** on rejection: an exception reports a destination that does not
exist, where `shouldAppend = false` reports one that exists and is too small, and
conflating them would make the failure indistinguishable from an ordinary
short-buffer result.

**Validation.** `SharpRuntimeTests_Core_Base` **4,994/4,994** (was 4,982), and the
same 4,994 under **ASan + UBSan + LSan with zero reports**
(`build-asan/1810_core_base_asan.log`). Repository gate: **0 warnings, 0 errors**,
**13,970 tests across 37 executables** (was 13,958). Module graph **41 / 90**;
catalogue current; database consistent; the ten-component selective matrix passed;
Doxygen **1,941** of the 1,942 ceiling, unchanged; `git diff --check` clean.

**Source and ABI consequences: none.** Header-only, one new private static helper;
no public signature, object layout, vtable or exported symbol changed. Neither
rejected input had any in-repository caller.

**Explicitly not done here.** **SR-AUD-133** — `AppendFormatted` discards its
format string and substitutes hardcoded C++ spellings, so `true` renders as `1`,
`255` with `"X2"` as `255`, and `3.14` as `3.140000` — shares this file and stays
`confirmed`. It asks for format/provider-aware formatting *or* an explicit
renaming of the surface to a documented primitive formatter, which is a design
decision about what this type is, not a safety repair. The report's observation
that the class is an ordinary C++ object rather than a compiler-generated
`ref struct`, with nothing preventing it from being copied or escaping, belongs
with it.

Build directories used: `build/` (gate), `build-asan/`, `build-probe/` (all
`1810_` prefixed), `build-tmp/` (repository-local `TMPDIR`); **no new build
directory was created** and **no compilation exceeded three jobs**. The probe
binaries were deleted once their logs were transcribed.

**CNA and mobile-eggbert were not inspected, searched, configured, built or
modified**, and **#1773 remains `blocked`**. #1804 remains `blocked`; #1808 and
#1809 remain `todo` and unbegun. No previously `done` ticket and no finding was
reopened.

### Completed compression-stream null-inner validation: ticket #1811

Ticket #1811 (`REMED-IO-COMPRESSION-NULL-INNER-STREAM`, P1, size S,
`remediation`, area *IO*) is **done** and **SR-AUD-257 is now `remediated`** —
the fifth of NEXT.md's eight immediate public-input crash tickets. **No new
`SR-AUD-*` identifier**; the numbering stays frozen at 364, and the index now
records **15 remediated** and **349 confirmed** of 364.

`DeflateStream`, `GZipStream` and `ZLibStream` all take a raw `Stream*` and none
validated it. Measured before any production change, one process per case, across
all three types symmetrically rather than only the one the finding named
(`build-probe/1811_prefix_defects.cpp`, logs `1811_prefix_defects.log` and
`1811_postfix_defects.log`):

| Cases | Input | Pre-fix | Post-fix |
|---|---|---|---|
| 1–3 | `T(nullptr, Compress, true)` then a 256 KiB incompressible `Write` | **ASan SEGV on 0x0**, all three types | `ArgumentNullException` at construction |
| 4–6 | `T(nullptr, Decompress, true)` then `Read` | **ASan SEGV on 0x0**, all three types | same |
| 7–12 | `T(nullptr, Compress, false)` then `Close()`, and destruction alone | completed | same |
| 13 | the valid path over a real `MemoryStream` | `length=6` | **byte-identical** |

The finding named `Write`. The `Decompress`-mode `Read` path crashes identically
and is the second half of the same defect; it is recorded rather than absorbed.

**The payload shape is why the check belongs at construction.** A small
compressible write does not reproduce this at all — zlib absorbs it into the
64 KiB deflate buffer and never touches the inner stream. That is what the
finding's phrase "a sufficiently large incompressible Write" is about, and it is
the argument against fixing this on the write path: a write-path guard would
still leave a constructed object whose inner stream does not exist. The check
also sits **before** `deflateInit2`/`inflateInit2`, which allocate state only
`deflateEnd`/`inflateEnd` release, so a rejected construction allocates no
compressor state.

**Cases 7–12 did not crash, and the reason is recorded so it is not "tidied
away" later.** `Close()` tests `if (produced > 0 && inner_)` and
`if (!leaveOpen_ && inner_)`. Those look exactly like the guards ticket #1806
removed from `StreamReader` as unreachable — but here they are **reachable**:
`Close()` itself assigns `inner_ = nullptr` after closing a non-`leaveOpen` inner
stream, so a null `inner_` is a genuine post-close state in these three classes.
They stay, and a comment in each constructor plus a note on the ticket row says
why. Two tickets in the same session reached opposite conclusions about
identical-looking code, for a concrete reason, and that is worth writing down.

**Tests: +9 permanent regressions** in `CompressionTests.cpp` — both compression
modes rejected for each of the three types, the owning (`leaveOpen = false`) shape
whose destructor and `Close()` also touch the inner stream, the parameter name,
and a large incompressible round-trip that exercises the exact flush path the
crash came from.

**Validation.** `SharpRuntimeTests_IO_Compression` **31/31** (was 22), and the
same 31 under **ASan + UBSan + LSan with zero reports**
(`build-asan/1811_io_compression_asan.log`). Repository gate: **0 warnings, 0
errors**, **13,979 tests across 37 executables** (was 13,970). Module graph
**41 / 90**; catalogue current; database consistent; the ten-component selective
matrix passed, including the `IO.Compression` and `IO.Compression.Zip` consumer
fixtures that build this component in isolation; Doxygen **1,941** of the 1,942
ceiling, unchanged; `git diff --check` clean.

**Source and ABI consequences: none.** No public signature, object layout, vtable
or exported symbol changed. Behavioural note: constructing any of the three over a
null stream now throws instead of deferring a crash to first use. No
in-repository caller did so.

**Explicitly not done here.** **SR-AUD-258** — invalid `CompressionMode` values
silently accepted (a native `(CompressionMode)42` constructs, creates a deflater,
and reports both `CanRead` and `CanWrite` false, where .NET throws
`ArgumentException`), and post-`Close` operations returning silently — shares
these files and stays `confirmed`.

Build directories used: `build/` (gate), `build-asan/`, `build-probe/` (all
`1811_` prefixed), `build-tmp/` (repository-local `TMPDIR`); **no new build
directory was created** and **no compilation exceeded three jobs**. The probe
binaries were deleted once their logs were transcribed.

**CNA and mobile-eggbert were not inspected, searched, configured, built or
modified**, and **#1773 remains `blocked`**. #1804 remains `blocked`; #1808 and
#1809 remain `todo` and unbegun. No previously `done` ticket and no finding was
reopened.

## Session summary — 2026-07-29, five public-input crash remediations

Tickets **#1805, #1806, #1807, #1810 and #1811** are `done`, each remediating a
high-severity public-input crash finding: SR-AUD-341, SR-AUD-338, SR-AUD-097,
SR-AUD-132 and SR-AUD-257 respectively. Two new inactive `todo` tickets, **#1808**
and **#1809**, were opened from defects found during #1806 and were not folded
into it. The audit identifier range stays **frozen at 364**; the findings index
now records **15 remediated and 349 confirmed** of 364, up from 10 and 354.

The ticket queue was **empty** when the session began — every row `done` except
the correctly `blocked` #1773 and #1804. The backlog lives in
`audit/AUDIT_FINDINGS_INDEX.md`, and each ticket was created by converting the
next item from `NEXT.md`'s recommended dependency order, which is how every
remediation ticket since #1767 has begun.

In **all five**, the defect proved larger than the finding described: five
`StreamWriter` dereferences rather than one, three `AggregateException` crash
paths plus two silent hand-offs to the caller, a `Read` half of SR-AUD-257 that
was never named, a `std::length_error` leak alongside SR-AUD-341's null read, and
a `size_t` capacity wrap alongside SR-AUD-132's zero-page write. Every extra
defect is disclosed in the owning audit report rather than absorbed silently.

Baselines after the session: **13,979 tests across 37 executables** (from 13,923),
0 warnings, 0 errors, 41 modules / 90 edges, Doxygen **1,941** of the 1,942
ceiling unchanged throughout, negative consumer fixtures 9 / 66 with every site
rejected, version-seam ODR 2 seams / 18 specialisations.

Full detail — per-ticket measurements, scope boundaries, environment notes and
the recommended next ticket — is in `NEXT.md`'s "CONTEXT-REFRESH handoff" section
and in the per-ticket sections above.

### Completed ZipArchive null-stream validation: ticket #1812

Ticket #1812 (`REMED-IO-ZIP-NULL-STREAM`, P1, size S, `remediation`, area *IO*)
is **done** and **SR-AUD-242 is now `remediated`** — the sixth of NEXT.md's eight
immediate public-input crash tickets. **No new `SR-AUD-*` identifier**; the
numbering stays frozen at 364, and the index now records **16 remediated** and
**348 confirmed** of 364.

`ZipArchive`'s public `Stream*` constructor stored the pointer with no null
check. Measured before any production change, one process per case
(`build-probe/1812_prefix_defects.cpp`, logs `1812_prefix_defects.log` and
`1812_postfix_defects.log`):

| Case | Input | Pre-fix | Post-fix |
|---|---|---|---|
| 1 | `ZipArchive(nullptr, Read)` | **ASan SEGV on 0x0**, exit 1 | `ArgumentNullException` |
| 2 | `ZipArchive(nullptr, Update)` | **ASan SEGV on 0x0**, exit 1 | same |
| 3 | `ZipArchive(nullptr, Create)` | constructed | same |
| 4 | `ZipArchive(nullptr, Create)` + `CreateEntry` + write + `Dispose()` | completed — **and delivered nothing** | same |
| 5 | `ZipArchive(nullptr, Create)` destruction alone | completed | same |
| 6 | `ZipArchive(nullptr, (ZipArchiveMode)42)` | constructed | rejected on the null, before the mode |
| 7 | the valid Create path over a real `MemoryStream` | `length=146` | **byte-identical** |
| 8 | the valid Read path over case 7's archive | `entries=1 name=payload.txt length=5` | **byte-identical** |

**The two halves of this finding are not symmetric, and that asymmetry is the
argument for an unconditional guard.** Read and Update dereference the pointer
inside the constructor itself, so they fail loudly and immediately. Create never
crashes: it stores the null pointer and then every call the caller makes
succeeds — `CreateEntry`, the entry write stream, `Dispose()`. The finalized
archive lands in `state_->memBuf`, and `Dispose()`'s write-back is gated on
`state_->stream != nullptr`, so the archive is discarded in silence. Case 4 wrote
a complete one-entry archive and delivered it nowhere, with no diagnostic of any
kind. Silent data loss is the worse of the two failure modes, so the check covers
every mode rather than only the ones that segfault.

The check sits **first**, before any state is populated: nothing opens a reader
and nothing fills a buffer on the rejected path, and `state_` is a `shared_ptr`
that unwinds on its own. This matches .NET, whose `Stream`-taking `ZipArchive`
constructors all funnel into `ZipArchive(Stream, ZipArchiveMode, bool,
Encoding?)` and open with `ArgumentNullException.ThrowIfNull(stream)`.

**The path-based constructor overload is deliberately unchanged** and a
regression pins that: an unopenable path still raises
`System::IO::InvalidDataException` from the reader, not the new guard.

**One separate defect was found and deliberately not folded in.** Probe case 9 —
added *after* the fix, because a null stream no longer reaches the mode at all —
constructs `ZipArchive(&realStream, (ZipArchiveMode)42)` successfully, where
.NET's `ValidateMode` throws `ArgumentOutOfRangeException(nameof(mode))`. It is a
different public contract, carries **no `SR-AUD-*` identifier** (the audit
recorded invalid mode values only as a missing-test note, never as a finding),
and is now inactive ticket **#1813**, which is explicitly told to inventory the
sibling enum surfaces — including SR-AUD-258's `CompressionMode` half and
`ZipFile::Open`'s own `ZipArchiveMode` parameter — before writing a guard.

**Tests: +8 permanent regressions** in the ZIP integration fixture
(`tests/integration/System/IO/Compression/CompressionTests.cpp`) — all three
modes, the defaulted-mode overload, the parameter name, repeatability of the
rejected construction, an unaffected valid Create/Read round-trip, and the
path-based overload.

**Validation.** The ZIP fixture is **44/44** (was 36), and the same 44 under
**ASan + UBSan + LSan with zero reports** (`build-asan/1812_zip_asan.log`, which
required building `SharpRuntimeIntegrationTests` in `build-asan/` for the first
time, at three jobs). Repository gate: **0 warnings, 0 errors**, **13,987 tests
across 37 executables** (was 13,979). Module graph **41 / 90**; catalogue
current; database consistent; the ten-component selective matrix passed,
including the `IO.Compression.Zip` fixture that builds this component in
isolation; Doxygen **1,941** of the 1,942 ceiling, unchanged; `git diff --check`
clean.

**Source and ABI consequences: none.** No public signature, object layout, vtable
or exported symbol changed. Behavioural note: constructing a `ZipArchive` over a
null stream now throws instead of crashing (Read/Update) or silently discarding
output (Create). No in-repository caller did so.

Build directories used: `build/` (gate), `build-asan/`, `build-probe/` (all
`1812_` prefixed), `build-tmp/` (repository-local `TMPDIR`); **no new build
directory was created** and **no compilation exceeded three jobs**.

### Completed HttpContent JSON null-content validation: ticket #1814

Ticket #1814 (`REMED-NET-HTTP-JSON-NULL-CONTENT`, P1, size S, `remediation`,
area *Net*) is **done** and **SR-AUD-236 is now `remediated`** — the seventh of
NEXT.md's eight immediate public-input crash tickets. **No new `SR-AUD-*`
identifier**; the numbering stays frozen at 364, and the index now records
**17 remediated** and **347 confirmed** of 364.

`HttpContentJsonExtensions::ReadFromJson` dereferenced its
`std::shared_ptr<HttpContent>` with no validity check, and `ReadFromJsonAsync`
delegated to it from inside the task body. Measured before any production change,
one process per case (`build-probe/1814_prefix_defects.cpp`, logs
`1814_prefix_defects.log` and `1814_postfix_defects.log`):

| Case | Input | Pre-fix | Post-fix |
|---|---|---|---|
| 1 | `ReadFromJson(empty)` | UBSan *member access within null pointer* at `HttpContentJsonExtensions.hpp:32`, then **ASan SEGV on 0x0** | `ArgumentNullException` |
| 2 | `ReadFromJsonAsync(empty)` then `getResultProperty()` | same pair, on worker thread **T1** | same, thrown by the call itself |
| 3 | `ReadFromJsonAsync(empty)` and **never awaited** | same pair, on worker thread **T1** — the process still died | same |
| 4 | the valid synchronous path | `answer=42` | **byte-identical** |
| 5 | the valid asynchronous path | `answer=7` | **byte-identical** |

**Case 3 is why the async guard sits before the task, not inside it.** The finding
describes `ReadFromJsonAsync` as turning null input into "a deferred task crash".
Measured, it is worse than deferred: the dereference happened on the `std::async`
worker thread, so a caller that started the task and walked away still lost the
whole process to a SEGV on thread T1. A guard placed only inside `ReadFromJson`
would have converted that into an exception stored on a task the caller never
observes — quieter, still wrong.

.NET prevents exactly this by code layout, and this port now copies it: the public
`ReadFromJsonAsync` overloads in `HttpContentJsonExtensions.cs` are **not** `async`
methods. Each runs `ArgumentNullException.ThrowIfNull(content)` and only then calls
the separate `ReadFromJsonAsyncCore`, so a null argument throws at the call site. A
named regression pins that placement, not just the exception type.

**One component-metadata consequence — the first in this remediation series.**
`Net.Http.Json` is an `INTERFACE` (header-only) component, so the guard lives in a
public header, which must include `System/ArgumentNullException.hpp`.
`Net.Http.Json` previously reached `Core.Base` only transitively through
`Net.Http`, and the boundary validator correctly rejected the undeclared public
edge. `modules/net-http-json/CMakeLists.txt` now declares `Core.Base` in
`PUBLIC_DEPENDENCIES` and `docs/ComponentCatalog.md` was regenerated. **The
production graph moves from 90 to 91 direct edges**; the module count is unchanged
at 41. No consumer include path, target name or link line changes as a result, and
the ten-component selective matrix still passes.

**Tests: +7 permanent regressions** in `HttpContentJsonExtensionsTests.cpp` — both
entry points rejected, the parameter name on both, the synchronous-throw placement
of the async guard, repeatability, content whose body is the JSON literal `null`
(an empty `shared_ptr` and a document that parses to null are different things and
must stay different), and empty content still reaching the parser rather than
being absorbed by the new guard.

**Validation.** `SharpRuntimeTests_Net_Http_Json` **15/15** (was 8), and the same
15 under **ASan + UBSan + LSan with zero reports**
(`build-asan/1814_net_http_json_asan.log`). Repository gate: **0 warnings, 0
errors**, **13,994 tests across 37 executables** (was 13,987). Module graph **41 /
91**; catalogue regenerated and current; database consistent; the ten-component
selective matrix passed; Doxygen **1,941** of the 1,942 ceiling, unchanged;
`git diff --check` clean.

**Source and ABI consequences: none.** No public signature, object layout, vtable
or exported symbol changed. Behavioural note: passing an empty `shared_ptr` to
either method now throws instead of crashing the process. No in-repository caller
did so.

**Deliberately out of scope.** `HttpClientJsonExtensions`'s
`GetFromJsonAsync`/`DeleteFromJsonAsync` do guard their response content, but map a
null content body to the JSON literal `"null"`
(`content ? content->ReadAsString() : "null"`) rather than to a diagnostic. That is
a different contract on a different type, carries no `SR-AUD-*` identifier, and was
left exactly as found.

Build directories used: `build/` (gate), `build-asan/`, `build-probe/` (all
`1814_` prefixed), `build-tmp/` (repository-local `TMPDIR`); **no new build
directory was created** and **no compilation exceeded three jobs**.

### Completed Base64 family plan and in-place write order: tickets #1815 and #1816

With the eight immediate public-input crash findings closed, the roadmap's item 3
applies: cross-cutting causes get a **scoped family plan** first, not a
file-by-file sweep. **CCF-013** was taken first because it is the smallest
coherent cause and the only one of the five that is a *correctness* defect rather
than a parity difference — the current output is simply wrong.

**Ticket #1815** (`REMED-BUFFERS-BASE64-FAMILY-PLAN`, P2, size S, design-only) is
**done** and recorded in
[`docs/Base64FamilyPlan.md`](docs/Base64FamilyPlan.md). No production source
changed under it. It establishes, from evidence rather than memory:

- CCF-013 has exactly **one** member finding, SR-AUD-078, spanning **two** public
  headers, and the cause explicitly requires one repair covering both;
- four adjacent `confirmed` findings — SR-AUD-079, SR-AUD-080, SR-AUD-081,
  SR-AUD-082 — live in the *same two headers* and share the same shape, but are
  **not** CCF-013 members and are not renamed into it;
- the pre-existing in-place encode tests covered `dataLength` **2 and 3 only**,
  which are precisely the two shapes that *cannot* exhibit the defect (length 3
  has no remainder, length 2 has no full pack) — that is why the suite was green;
- #1817, #1818 and #1819 all edit the same final-quantum branch of one
  `decodeCore` and are therefore **sequenced**, while #1820 touches the Base64Url
  decode table instead and is deliberately **unordered** against them;
- **none** of the six tickets needs the public-signature/layout approval category,
  though #1817 and #1818 do narrow the accepted input set, which each must state
  in its own record.

**Ticket #1816** (`REMED-BUFFERS-BASE64-INPLACE-ORDER`, P1, size S) is **done**
and **SR-AUD-078 is now `remediated`**, which **closes CCF-013**. **No new
`SR-AUD-*` identifier**; the numbering stays frozen at 364, and the index now
records **18 remediated** and **346 confirmed** of 364.

`Base64::EncodeToUtf8InPlace` and `Base64Url::TryEncodeToUtf8InPlace` both encoded
the full 3-byte packs backwards and only then read the trailing one/two-byte
remainder. Encoding pack `i` reads source `3i..3i+2` and writes output
`4i..4i+3`, and `4i >= 3i`, so a pack can only overwrite source bytes belonging to
packs *after* it — which makes a last-to-first walk correct for every full pack
but **not** for the remainder, which is the last pack of all and was handled after
the loop. The remainder is now encoded **first**. That is .NET's own order:
`Base64Helper/Base64EncoderHelper.cs`'s shared
`EncodeToUtf8InPlace<TBase64Encoder>` encodes the leftover pack before its
backwards loop, under the comment *"encode last pack to avoid conditional in the
main loop"*.

**Measured before any production change** (`build-probe/1816_prefix_defects.cpp`,
logs `1816_prefix_defects.log` and `1816_postfix_defects.log`) — every
`dataLength` from 0 to 24, both types, each in-place result compared against *the
same type's own out-of-place encoder*, with a sentinel byte immediately past the
encoded output:

| | Pre-fix | Post-fix |
|---|---|---|
| Cases wrong | **28 of 50** | **0 of 50** |
| Lengths affected, per type | 4, 5, 7, 8, 10, 11, 13, 14, 16, 17, 19, 20, 22, 23 | none |
| Status returned | `Done` / `true` in **all** 50 | unchanged |
| Sentinel past the output | never touched | never touched |

**The scope was larger than the finding's "4/5-byte source lengths"** — it is
every length with both a full pack and a remainder, which the finding's own text
does say and the sweep makes concrete. **The sentinel result is the other half of
the story**: this was silent corruption *inside* the declared output, never an
overrun, which is why no sanitizer had ever flagged it and why only a
differential sweep could find it.

**Tests: +8 permanent regressions**, four per header — the audit's own 4-byte
reproduction (`'A','B','C',0` → `QUJDAA==` / `QUJDAA`, not `QUJDRA==` /
`QUJDRA`), the 5-byte case, a **7-byte** case proving the defect was never limited
to 4 and 5, and the 0..24 sweep asserting equality with the out-of-place encoder
and an untouched sentinel at every length.

**Validation.** `SharpRuntimeTests_Buffers` **473/473** (was 465), and the same
473 under **ASan + UBSan + LSan with zero reports**
(`build-asan/1816_buffers_asan.log`). Repository gate: **0 warnings, 0 errors**,
**14,002 tests across 37 executables** (was 13,994). Module graph **41 / 91**;
catalogue current; database consistent; the ten-component selective matrix
passed; Doxygen **1,941** of the 1,942 ceiling, unchanged; `git diff --check`
clean.

**Source and ABI consequences: none.** Both functions are `static` members of
header-only classes; no signature, layout or exported symbol changed.
**Behavioural note for consumers: any output previously produced in place for a
length with both a full pack and a remainder was wrong and is now correct**, so a
consumer that stored or transmitted such output stored corrupted data. No
in-repository caller uses these APIs outside their tests.

**Left open on purpose.** SR-AUD-079, SR-AUD-080, SR-AUD-081 and SR-AUD-082 stay
`confirmed` and are tickets **#1817–#1820**, ordered by the plan. #1815 also
opened **#1821** for a defect found while planning and not folded in: .NET's
helper short-circuits an empty buffer to `Done` *before* its length check, while
this port returns `DestinationTooSmall`/`false` for an empty buffer with a
positive `dataLength` (`build-probe/1815_empty_buffer_probe.log`). #1821 is framed
as a **decision**, not a foregone fix — reporting `Done` for a request to encode
five bytes into a zero-byte buffer is arguably the worse contract.

Build directories used: `build/` (gate), `build-asan/`, `build-probe/` (all
`1815_`/`1816_` prefixed), `build-tmp/` (repository-local `TMPDIR`); **no new
build directory was created** and **no compilation exceeded three jobs**.

### Completed Base64 canonical final-quantum rule: ticket #1817

Ticket #1817 (`REMED-BUFFERS-BASE64-CANONICAL-FINAL-BITS`, P2, size S,
`remediation`, area *Buffers*) is **done** and **SR-AUD-079 is now `remediated`**.
It is the second ticket of the Base64 family plan
([`docs/Base64FamilyPlan.md`](docs/Base64FamilyPlan.md), ticket #1815) and the
first of its three sequenced `decodeCore` tickets. **No new `SR-AUD-*`
identifier**; the numbering stays frozen at 364, and the index now records
**19 remediated** and **345 confirmed** of 364.

Neither header required the unused low bits of the final quantum to be zero:

- a quantum carrying **one** byte (`XX==` padded, `XX` unpadded) uses only the top
  two bits of the second sextet, so its **low four bits** must be zero;
- a quantum carrying **two** bytes (`XXX=` padded, `XXX` unpadded) uses only the
  top four bits of the third sextet, so its **low two bits** must be zero.

Measured before and after (`build-probe/1817_defects.cpp`, with the pre-fix log
built against stashed headers so the two runs use the same source):

| Input | Type | Pre-fix | Post-fix |
|---|---|---|---|
| `AB==` | Base64 | `Done`, 1 byte, `IsValid` **true** | `InvalidData`, 0 bytes, `IsValid` false |
| `AAB=` | Base64 | `Done`, 2 bytes, `IsValid` **true** | `InvalidData`, 0 bytes, `IsValid` false |
| `AB` | Base64Url | `Done`, 1 byte, `IsValid` **true** | `InvalidData`, 0 bytes, `IsValid` false |
| `AAB` | Base64Url | `Done`, 2 bytes, `IsValid` **true** | `InvalidData`, 0 bytes, `IsValid` false |
| 12 canonical spellings, both types | — | accepted | **unchanged** |

**The validator had to change in the same ticket, not a later one.** Decoder and
validator agreed before and after; a validator more permissive than its own decoder
is the worse outcome, because it tells a caller an input is safe to decode when it
is not. Base64Url's `validateCore` only *counted* symbols and never kept their
values, so it now retains the trailing sextets in order to apply the rule at all.

**The canonical check runs before the destination-size check**, deliberately:
canonicity is a property of the input alone and must not depend on how much room
the caller provided. Canonical input is unaffected either way, so no existing
`DestinationTooSmall` outcome changes.

**This narrows the accepted input set**, in the direction of .NET parity. Input
that used to decode successfully is now `InvalidData`. All 104 pre-existing
`Base64*` tests still pass unmodified, so nothing in this repository relied on the
old acceptance.

**Tests: +12 permanent regressions**, six per header — the noncanonical one- and
two-byte quanta rejected by the decoder, `IsValid` and its `decodedLength` overload
agreeing with the decoder, the `char` overloads inheriting the rule through the
shared core, six canonical spellings still decoding to the same bytes, and a 0..24
round trip proving everything this repository's own encoder produces is still
accepted.

**Validation.** `SharpRuntimeTests_Buffers` **485/485** (was 473), and the same 485
under **ASan + UBSan + LSan with zero reports**
(`build-asan/1817_buffers_asan.log`). Repository gate: **0 warnings, 0 errors**,
**14,014 tests across 37 executables** (was 14,002). Module graph **41 / 91**;
catalogue current; database consistent; the ten-component selective matrix passed;
Doxygen **1,941** of the 1,942 ceiling, unchanged; `git diff --check` clean.

**Source and ABI consequences: none.** No signature, layout or exported symbol
changed; only the accepted input set did.

**Still open in this family**, in the plan's order: **#1818** (SR-AUD-080, padding
accepted while `isFinalBlock` is false), **#1819** (SR-AUD-081, trailing whitespace
wrongly consumed), **#1820** (SR-AUD-082, Base64Url rejects optional final
padding), and **#1821** (the empty-buffer status divergence, no `SR-AUD-*`).

Build directories used: `build/` (gate), `build-asan/`, `build-probe/` (all
`1817_` prefixed), `build-tmp/` (repository-local `TMPDIR`); **no new build
directory was created** and **no compilation exceeded three jobs**.

### Completed Base64 non-final padding rule: ticket #1818

Ticket #1818 (`REMED-BUFFERS-BASE64-NONFINAL-PADDING`, P2, size S, area
*Buffers*) is **done** and **SR-AUD-080 is `remediated`**. It is the third ticket
of the Base64 family plan ([`docs/Base64FamilyPlan.md`](docs/Base64FamilyPlan.md),
ticket #1815) and the second of its three sequenced `decodeCore` tickets. **No new
`SR-AUD-*` identifier**; numbering stays frozen at 364, and the index now records
**20 remediated** and **344 confirmed** of 364.

`Base64::decodeCore` consulted `isFinalBlock` only *after* an incomplete unpadded
group, so a **complete padded** group decoded to `Done` regardless of the flag —
telling a chunked caller that a terminal quantum was ordinary intermediate data.

Current .NET rejects padding in a non-final call along two independent paths:
`Base64DecoderHelper.DecodeFrom` sets `skipLastChunk = isFinalBlock ? 4 : 0`, so
with the flag clear the whole source runs through the four-element loop where `'='`
is unmapped and the padding-aware tail is unreachable; and
`DecodeWithWhiteSpaceBlockwise` forces its per-block `localIsFinalBlock` back to
false whenever the caller's flag is false. The repair is that one rule, applied at
the **first** padding character — which is what keeps `bytesConsumed`/`bytesWritten`
on the last completed quantum boundary and stops a too-small destination from
masking the rejection.

**The finding understated its surface.** It named one input; six of the seven
non-final shapes probed were wrong (`build-probe/1818_defects.cpp`, logs
`1818_prefix_defects.log` / `1818_postfix_defects.log`): the bare padded quantum, a
padded quantum after a complete one, the single-`=` spelling, padding in a
non-terminal position, and padding split by whitespace.

**Two residual divergences are recorded, not fixed** — both in the cursor reported
*alongside* `InvalidData`, neither changing a status or a decoded byte. They are
inactive ticket **#1822**, with no `SR-AUD-*` identifier.

**This narrows the accepted input set.** Every `isFinalBlock == true` outcome is
byte-for-byte unchanged, `IsValid` is unaffected (it has no `isFinalBlock`
parameter and *is* the final-block decoder's validator), and unpadded incomplete
quanta keep `NeedMoreData`.

**Tests: +7 permanent regressions.** `SharpRuntimeTests_Buffers` **492/492** (was
485), and the same 492 under **ASan + UBSan + LSan with zero reports**
(`build-asan/1818_buffers_asan.log`). Repository gate: **0 warnings, 0 errors**,
**14,021 tests across 37 executables** (was 14,014). Module graph **41 / 91**;
catalogue current; database consistent; the ten-component selective matrix passed;
Doxygen **1,941** of the 1,942 ceiling, unchanged; `git diff --check` clean.

**Source and ABI consequences: none.**

**Still open in this family**, in the plan's order: **#1819** (SR-AUD-081, trailing
whitespace wrongly consumed), **#1820** (SR-AUD-082, Base64Url rejects optional
final padding), **#1821** (the empty-buffer status divergence) and **#1822** (the
`InvalidData` cursor), the last two with no `SR-AUD-*`.

Build directories used: `build/` (gate), `build-asan/`, `build-probe/` (all
`1818_` prefixed), `build-tmp/` (repository-local `TMPDIR`); **no new build
directory was created** and **no compilation exceeded three jobs**.

### SR-AUD-081 is a false positive: ticket #1819

Ticket **#1819** (`REMED-BUFFERS-BASE64-PADDED-WHITESPACE-CURSOR`, P2, size XS,
area *Buffers*) is **done, classified FALSE POSITIVE**. **No production source
changed.** It is the fourth ticket of the Base64 family plan
([`docs/Base64FamilyPlan.md`](docs/Base64FamilyPlan.md), ticket #1815); see that
plan's new §8.

**The finding's premise is inverted.** SR-AUD-081 states that *"the current .NET
Base64 test base specifies that whitespace after end/padding is not included in
consumed bytes"*. That test base specifies the opposite, in three places: its member
data is named `BasicDecodingWithExtraWhitespaceShouldBeCountedInConsumedBytes_MemberData`
and yields `{ "AQ==" + whitespace(i), 4 + i, 1 }`; its second half yields
`{ s+s+s+s, s.Length * 4, 12 }` for seven whitespace placements including a trailing
one; and `DecodingWithWhiteSpaceSplitFinalQuantumAndIsFinalBlockFalse` asserts
`bytesConsumed == base64Data.Length` for `"AQ\r\nQ=\r\n"`, whitespace after the
padding included.

For the finding's own `"QQ== \n"`, .NET reports **6** consumed — exactly what this
port reports. Traced through `Base64DecoderHelper.DecodeFrom`: `SrcLength` rounds the
source to 4, the `if (srcLength != source.Length)` guard sends the call to
`InvalidDataExit`, and `InvalidDataFallback` then finds the remainder to be all
whitespace, executes `bytesConsumed += source.Length` and returns `Done`.

**Measured**: `build-probe/1819_defects.cpp` (log `build-probe/1819_defects.log`)
replays .NET's own vectors with .NET's own expected values on both the UTF-8 and the
`char` overload. **27 of 27 whitespace-consumption vectors match.** The same run
independently re-confirmed ticket #1818 against .NET's *own* tests
(`"AAA="` → `InvalidData`, 0, 0; `"AAAA"` → `Done`, 4, 3; `"AQ\r\nQ="` →
`InvalidData`, 0, 0) — a stronger check than the traced expectations #1818 closed on.

**SR-AUD-081 stays `confirmed`** because the findings-index vocabulary has no
false-positive value — the same treatment SR-AUD-362 received under #1779 — and now
carries a Correction note in the index row and in the owning report. It must not be
read as an open defect. The index counts are unchanged at **20 remediated / 344
confirmed of 364**.

**Tests: +4 permanent regressions** pinning the .NET-verified behaviour so the
inverted premise cannot be re-applied later in good faith — the `"AQ==" + i` shape,
the seven whitespace placements repeated four times, the whitespace-split final
quantum with and without trailing whitespace, and the `char` overload.
`SharpRuntimeTests_Buffers` **496/496** (was 492); repository gate **0 warnings, 0
errors, 14,025 tests across 37 executables** (was 14,021).

**What the run *did* find** is ticket **#1822**, upgraded from two traced instances
to four **.NET-test-pinned** ones, from P3 to **P2**, and from `InvalidData` only to
`DestinationTooSmall` as well: on a non-`Done` return .NET advances `bytesConsumed`
past whitespace to the first non-whitespace character at or after the last completed
quantum boundary, and this port stops at the boundary. One case sits outside that
rule and needs its own decision — `"QQ==QUJD"` with `isFinalBlock` true, where
`DecodeWithWhiteSpaceBlockwise` reverts its counters to `0,0`. No `SR-AUD-*`
identifier; numbering stays frozen at 364.

### Completed Base64Url optional-padding acceptance: ticket #1820

Ticket **#1820** (`REMED-BUFFERS-BASE64URL-OPTIONAL-PADDING`, P2, size S, area
*Buffers*) is **done** and **SR-AUD-082 is now `remediated`** — the last `confirmed`
finding in `Base64Url.hpp`, and the fifth and final repair ticket of the Base64 family
plan ([`docs/Base64FamilyPlan.md`](docs/Base64FamilyPlan.md), ticket #1815). No new
`SR-AUD-*` identifier; numbering stays frozen at 364, and the index now records
**21 remediated** and **343 confirmed** of 364.

Base64Url's decode table mapped `'='` and `'%'` to `-1` and `decodeCore` rejected
either immediately, so `IsValid` rejected them too and `YQ==` / `YQ%%` were
unreadable. Base64Url omits padding on **output**, and this port still does — but
.NET's decoder and validator deliberately **accept** it:
`Base64UrlDecoderByte.IsValidPadding` is `padChar is EncodingPad or UrlEncodingPad`
and `Base64UrlByteValidatable.IsEncodingPad` is the same test. The port's header
documented only how it encodes and never claimed a stricter decode adaptation.

**The finding predicted a table change; the table did not need one.** Like .NET,
padding is now recognised by a test on the **raw character**, so kDecTable stays the
pure sextet alphabet and a padding character can never be mistaken for a value. Not
changing the table is the more faithful port.

**The grammar** is `Base64UrlByteValidatable.ValidateAndDecodeLength`: with
`remainder` symbols in the trailing incomplete quantum and `padCount` final pads,
padding is valid only when `remainder != 0` and `remainder + padCount <= 4`, at most
two pads, `remainder == 1` never decodable. Two symbols admit one **or** two pads,
three symbols admit exactly one, a complete quantum admits none; whitespace may sit
before, between and after the pads. Padding is also `InvalidData` when `isFinalBlock`
is false — #1818's rule, which .NET's own `DecodingInvalidBytesPadding` asserts here.
`validateCore` gained the same branch in the same change: a validator **stricter**
than its own decoder is as harmful as a more permissive one, because it declares a
decodable input unusable.

**Measured against 62 vectors, every one taken from a named current-.NET test rather
than traced** (`build-probe/1820_defects.cpp`, logs `1820_prefix_defects.log` and
`1820_postfix_defects.log`): **18 of 62 differed before, 0 of 62 after**, with the two
overloads and the two validators agreeing on every line throughout. All 18 were
rejections that should have been acceptances. **Nothing .NET rejects was accepted
before this change, and nothing is now** — padding before the last quantum, more than
two pads, a pad after a complete quantum or a one-symbol remainder, three symbols plus
two pads, data after padding, and noncanonical final bits under padding are all still
`InvalidData`, with the exact cursor .NET reports.

**This is a widening change**: it only adds accepted input, which is why the family
plan deliberately left it unordered against the narrowing tickets #1817–#1819.
Unpadded input of every remainder size decodes exactly as before and the encoder still
emits no padding, both confirmed by a 0..24 round trip.

**One correction to the ticket's own note**: it suggested rewording
`invalidDataMessage()` because it mentions padding despite a Base64Url surface. That
would be a **divergence** — .NET's `Base64Url` throws `FormatException` with
`SR.Format_BadBase64Char`, verbatim the string this port already uses. The message is
left alone.

**Tests: +8 permanent regressions.** `SharpRuntimeTests_Buffers` **504/504** (was
496), and the same 504 under **ASan + UBSan + LSan with zero reports**
(`build-asan/1820_buffers_asan.log`); the probe is clean under the same three
(`build-probe/1820_asan.log`). Repository gate: **0 warnings, 0 errors, 14,033 tests
across 37 executables** (was 14,025). Module graph **41 / 91**.

**Source and ABI consequences: none.**

**The Base64 family is now closed except for two decisions**: **#1821** (the
empty-buffer status divergence in the in-place encoders) and **#1822** (the cursor
reported alongside a non-`Done` status), neither of which carries an `SR-AUD-*`
identifier.

### Completed non-Done decode cursor alignment: ticket #1822

Ticket **#1822** (`REMED-BUFFERS-BASE64-INVALIDDATA-CURSOR`, P2, size S, area
*Buffers*) is **done**. It carries **no `SR-AUD-*` identifier** by design — the audit
numbering stays frozen at 364 and the index counts are unchanged at **21 remediated /
343 confirmed**. It was opened inactive by #1818 with two *traced* instances and
upgraded by #1819 to four **.NET-test-pinned** ones, from P3 to P2, and from
`InvalidData` alone to `DestinationTooSmall` as well.

**The defect.** On a non-`Done` return both `decodeCore`s reported the boundary of the
last **completed** quantum. Current .NET reports the first **non-whitespace** character
at or after that boundary, because it reaches its `InvalidData` and
`DestinationTooSmall` exits through `InvalidDataFallback`, which skips the failing
region's leading whitespace and adds it to `bytesConsumed` before re-entering the
decoder.

**Pinned by .NET's own tests**, not by tracing:
`DecodingWithValidDataBeforeWhiteSpaceSplitFinalQuantum` asserts `bytesConsumed` of
**9, 10 and 15** and then that slicing the input at that cursor leaves exactly
`"AQ\r\nQ="`; `DecodingWithEmbeddedWhiteSpaceIntoSmallDestination_TrailingWhiteSpacesAreConsumed`
asserts `input[consumed] == 'j'` — index **44** of a 48-byte input with a 6-byte
destination — which is what makes this a rule about *every* non-`Done` status rather
than about `InvalidData`.

**The fix** is one `failCursor` per header, updated whenever a non-whitespace character
is read with no quantum pending, applied through a small `fail()` helper at every
`InvalidData`/`DestinationTooSmall` return. Base64Url gets it too, because .NET's
`DecodeFrom` and `InvalidDataFallback` are **one generic helper** shared by both
decoders.

**`NeedMoreData` is deliberately excluded.** .NET returns it from `NeedMoreDataExit`,
which the fallback never runs for, so its cursor stays on the quantum boundary —
`"QUJD QQ"` with `isFinalBlock` false is **4**, not 5. Applying the rule there would
have introduced a new divergence while fixing an old one.

**Measured** over 41 vectors across both types (`build-probe/1822_defects.cpp`, logs
`1822_prefix_defects.log` and `1822_postfix_defects.log`): **9 differed before, 0
after**, with the two overloads agreeing on every line. Re-running #1819's and #1820's
probes against the new rule gives **0 of 27** and **0 of 62** differences, so no
previously verified cursor moved.

**One case is a deliberate deviation**, decided explicitly as the ticket required.
.NET's `DecodeWithWhiteSpaceBlockwise` *reverts* its block counters when non-whitespace
follows a block's padding, reporting `0,0` for `"QQ==QUJD"` while having already
written the byte into the caller's destination. This port keeps reporting what it
actually wrote. The .NET behaviour is pinned by none of its own tests, and reporting
fewer bytes written than were physically written is the worse contract for a caller
that inspects the buffer. Two vectors pin the deviation so that it stays deliberate,
including the invariant `bytesWritten <= bytesConsumed`.

**Tests: +8 permanent regressions** — the three `DecodingWithValidDataBefore…` cursors
plus the remainder-slice assertion, the `DestinationTooSmall` cursor with its decoded
bytes and its untouched sentinel, five failing shapes on both overloads, three
`NeedMoreData` shapes keeping the boundary, and the deviation, for Base64; and the
`InvalidData`, `NeedMoreData` and `DestinationTooSmall` cursors for Base64Url.
`SharpRuntimeTests_Buffers` **512/512** (was 504), and the same 512 under **ASan +
UBSan + LSan with zero reports** (`build-asan/1822_buffers_asan.log`); the probe is
clean under the same three (`build-probe/1822_asan.log`). Repository gate: **0
warnings, 0 errors, 14,041 tests across 37 executables** (was 14,033). Module graph
**41 / 91**.

**No status and no decoded byte changes for any input; every `Done` cursor is
unchanged. Source and ABI consequences: none.**

### Completed in-place encoder empty-buffer decision: ticket #1821

Ticket **#1821** (`REMED-BUFFERS-BASE64-EMPTY-BUFFER-STATUS`, P3, size XS, area
*Buffers*) is **done**, and with it the entire Base64 family. It carries **no
`SR-AUD-*` identifier** by design; the index counts are unchanged at **21 remediated /
343 confirmed** of 364.

.NET's `Base64Helper.EncodeToUtf8InPlace` — the **one** helper its `Base64` and
`Base64Url` in-place encoders share — opens with
`if (buffer.IsEmpty) { bytesWritten = 0; return OperationStatus.Done; }` **before**
`GetMaxEncodedLength(dataLength)` and before the destination-size check. This port had
no such branch.

**Ticket #1815 recorded four diverging shapes; there are eight**, and the four it
missed are the more consequential ones (`build-probe/1821_defects.cpp`, logs
`1821_prefix_defects.log` and `1821_postfix_defects.log`):

| buffer | `dataLength` | Before | .NET / after |
|---|---|---|---|
| empty | 0 | `Done`, 0 | unchanged |
| empty | 1, 5 | `DestinationTooSmall` / `false` | `Done` / `true`, 0 |
| empty | −1, −1000 | **throws `ArgumentOutOfRangeException`** | `Done` / `true`, 0 |
| empty | 1610612734 | **throws `ArgumentOutOfRangeException`** | `Done` / `true`, 0 |

**The decision, and the reasoning against it.** #1815 framed this as a genuine
question: reporting `Done` for a request to encode five bytes into a zero-byte buffer
is arguably the worse contract. It was decided in favour of .NET's behaviour because
(1) `buffer` **is** the source, so a caller cannot act on `DestinationTooSmall` by
supplying a larger destination — that status names a remedy that does not exist, so it
is a different answer, not a more informative one; (2) the input is
self-contradictory, claiming `dataLength` bytes of raw data at the start of a
zero-length buffer, so there is no *correct* answer to give, only a *portable* one; and
(3) the **ordering** matters as much as the branch — without it, an empty buffer with a
negative or over-large `dataLength` **threw** where .NET returns `Done`, and an
exception where the reference implementation succeeds is a harder divergence for ported
code than a wrong status. That half of the divergence had not been recorded before this
ticket measured it.

**A non-empty buffer is untouched**: correct encodes, `DestinationTooSmall`,
`dataLength` 0, and the `ArgumentOutOfRangeException` on a negative or over-large
`dataLength` all behave exactly as before.

**Tests: +5 permanent regressions** across both types — the empty buffer succeeding for
six `dataLength` values with the byte past the span untouched, the short-circuit
running before validation for three invalid `dataLength` values, and the non-empty
buffer's four outcomes pinned unchanged. `SharpRuntimeTests_Buffers` **517/517** (was
512), and the same 517 under **ASan + UBSan + LSan with zero reports**
(`build-asan/1821_buffers_asan.log`); the probe is clean under the same three
(`build-probe/1821_asan.log`). Repository gate: **0 warnings, 0 errors, 14,046 tests
across 37 executables** (was 14,041). Module graph **41 / 91**.

**Source and ABI consequences: none.**

### Autonomous batch handoff, 2026-07-29 (Base64 family closure)

Five tickets completed on `feature/remediation-batch-base64-followup`: **#1818**
(SR-AUD-080, non-final padding), **#1819** (SR-AUD-081, **false positive**), **#1820**
(SR-AUD-082, optional Base64Url padding), **#1822** (the non-`Done` decode cursor, no
`SR-AUD-*`), and **#1821** (the empty-buffer in-place encode decision, no `SR-AUD-*`).

The Base64 family plan ([`docs/Base64FamilyPlan.md`](docs/Base64FamilyPlan.md), ticket
#1815) is **fully executed** and `Base64.hpp`/`Base64Url.hpp` carry no `confirmed`
`SR-AUD-*` finding.

Baselines at batch end: **14,046 tests across 37 executables**, 0 warnings, 0 errors;
findings index **21 remediated / 343 confirmed of 364**; module graph **41 / 91**;
Doxygen **1,941** of the 1,942 ceiling; **9 negative fixtures / 66 sites**; **2 version
seams / 18 specialisations**; selective-component matrix passed.

Four incorrect premises were corrected by appending, never by rewriting: SR-AUD-080
understated its surface, SR-AUD-081's premise is inverted, SR-AUD-082 predicted the
wrong repair and asked for a message change that would have been a divergence, and this
repository's own family plan was wrong in its §2 and §5.

Ready queue: **#1808**, **#1809** (same two headers — plan them together) and **#1813**,
all compatible and needing no approval. Blocked: **#1773** (CNA / mobile-eggbert
migration, untouched) and **#1804**. Next recommended family after those: **CCF-004**,
which needs a #1815-quality plan first.

Full detail, including build directories, the three-job parallelism record and the
`build-asan`/`build-probe` disk accounting, is in `NEXT.md` under
"Autonomous batch handoff, 2026-07-29 (Base64 family closure)".

### Completed text-wrapper input contract plan: ticket #1823

**`P2: scope the text-wrapper input contract before implementing #1808 or #1809`**
(`REMED-IO-TEXT-WRAPPER-CONTRACT-PLAN`, P2, size M, design-only) is recorded in
[`docs/TextWrapperInputContractPlan.md`](docs/TextWrapperInputContractPlan.md). No
production source changed under it. It carries **no `SR-AUD-*` identifier**; the audit
numbering stays frozen at 364, and `SR-AUD-337`/`SR-AUD-338` keep the statuses they had.

Tickets #1808 and #1809 name the same headers and were opened inactive by #1806 with an
explicit instruction to inventory before implementing. This ticket did that inventory
and measured every affected path *before* any production change, with one process per
case under ASan + UBSan + LSan (`build-probe/1823_prefix_defects.cpp`, log
`build-probe/1823_prefix_defects.log`).

**Three of the four premises in the two tickets were understated, and are corrected by
appending rather than by rewriting:**

1. **#1808 said the writer failure "surfaces later as a `NotSupportedException` from
   the first `Write`, or not at all if nothing is ever written."** For `FileStream` it
   surfaces **never, even when data is written**: a `StreamWriter` over a
   `FileAccess::Read` `FileStream` accepts `Write` and `Flush` with no exception, and
   the file is unchanged afterwards (cases 5, 15). `FileStream::Write` checks neither
   `canWrite_` nor the stream state, so `std::fstream::write` sets `badbit` and the
   bytes are dropped in silence. That is data loss, not a late diagnostic, and it is
   now ticket **#1825**.
2. **#1808 assumed one contract for both directions.** The two halves have opposite
   compatibility, and the cause is one line: `Stream::getCanWriteProperty()` **defaults
   to `false`** where .NET's `Stream.CanWrite` is abstract. A `CanRead` guard rejects
   only streams that positively declare themselves unreadable; a `CanWrite` guard also
   rejects every custom stream that implements `Write()` and never overrode the
   property — case 8 writes `"hello"` successfully through exactly such a stream today.
   #1808 is therefore rescoped to the reader half and the writer half becomes blocked
   ticket **#1824**.
3. **#1809 listed two failure modes; there are three, and it named the mildest two.**
   `Console::Write(nullptr)` sets `badbit` on `std::cout` **permanently**, so every
   subsequent console write in the process silently produces nothing (cases 26, 27) —
   no crash, no exception, no message. `StreamWriter` gives an ASan `SEGV` in `strlen`
   (cases 22, 23) and `TextWriter`/`StringWriter` a libstdc++ `std::logic_error` (cases
   20, 21, 24, 25), which is also the wrong hierarchy: `std::`, not `System::`.

**Selected contracts.** Null `const char*` follows .NET's null-**string** rule exactly —
`Write` is a no-op, `WriteLine` writes only the line terminator, nothing ever throws
(`TextWriter.cs:277-283`, `502-509`) — across `TextWriter`, `StreamWriter` and
`System::Console`. `StreamReader` rejects `!getCanReadProperty()` with
`ArgumentException("Stream was not readable.")`, byte-identical to `BinaryReader.cpp:25`
and to `StreamReader.cs:147`, after the null check and before member initialisation.

**Ticket split**: #1809 (compatible) → #1808 (compatible, reader half) → #1825
(compatible, `FileStream` access flags) → #1824 (**blocked on approval**) with #1826
opened inactive for `MemoryStream::getCanReadProperty()` ignoring `isOpen_`.

The family also has **no asynchronous path at all** — `grep -rn "Async"` over all six
text-wrapper headers returns nothing — no synchronized wrapper, no null writer, and no
adapters beyond the four subclasses, all of which is stated in §1.1 rather than assumed.

### Completed null `const char*` text contract: ticket #1809

**`P2: a null const char* is undefined behaviour across the TextWriter Write family`**
(`REMED-IO-TEXTWRITER-NULL-CSTRING`, P2, size S) is complete. It carries **no
`SR-AUD-*` identifier**; the audit did not record it, which is stated plainly rather
than backfilled, and the numbering stays frozen at 364.

Design ticket #1823 decided the contract for the whole family first, because #1809 was
a contract decision and not a guard. Its measurement found **three** structurally
different current failures where the ticket named two, and the unnamed one is the worst:

| Surface | Before | After |
|---|---|---|
| `TextWriter`/`StringWriter` `Write`/`WriteLine(const char*)` | libstdc++ `std::logic_error` — a `std::` exception, not a `System::` one, so `catch (const System::Exception&)` missed it | no-op / line terminator only |
| `StreamWriter::Write`/`WriteLine(const char*)` | AddressSanitizer **SEGV on `0x0`** inside `strlen` | no-op / line terminator only |
| `Console::Write`/`WriteLine(const char*)` | `badbit` on `std::cout` set **permanently**, silently disabling every later console write in the process | no-op / line terminator only, `std::cout` still `good()` |

**The contract is .NET's own rule for a null *string*.** These `const char*` overloads
have no .NET counterpart — they exist only so a string literal binds to the string
overload instead of `Write(bool)` (`TextWriter.hpp:30-37`) — so they take the behaviour
.NET gives the thing they spell: `TextWriter.cs:277-283` makes `Write(string?)` a
no-op, `TextWriter.cs:502-509` makes `WriteLine(string?)` write only the line
terminator, and neither ever throws. A guard that threw `ArgumentNullException` would
have been a divergence, exactly as the ticket's own description warned.

`StreamWriter` needed its own test: it is the one override of the base overload in this
repository, so fixing only `TextWriter` would have left the crash reachable by virtual
dispatch — including through `TextWriter::WriteLine(const char*)`, which forwards to
the virtual `Write`.

Closure evidence: **14 permanent regressions** (10 in `IOStreamTests.cpp`, 4 in
`Batch10ConsoleTests.cpp`) covering null, empty and ordinary input on each of the five
surfaces, the terminator-only `WriteLine` rule, a cross-type assertion that
`StringWriter` and `StreamWriter` answer null identically, an inertness check that a
null write between two ordinary writes leaves them contiguous, and three `Console`
cases asserting `std::cout.good()` afterwards — the sticky-`badbit` regression stated
directly. `SharpRuntimeTests_IO` **562/562** (was 552), `SharpRuntimeTests_Console`
**127/127** (was 123), both clean under ASan + UBSan + LSan
(`build-asan/1809_io_asan.log`, `build-asan/1809_console_asan.log`; activation proven
in `build-asan/1809_asan_activation.log`). Repository gate: 0 warnings, 0 errors,
**14,060 tests across 37 executables**.

Source and ABI consequences: none. No public signature, virtual, vtable, object layout
or exported symbol changed. `TextWriter`'s two overloads are `inline` in a public
header, so an unrebuilt consumer keeps the old inlined body — a stale-inline
consideration, not an ABI break, and benign here because the old body only ever failed
on the input the new one handles.

### Completed StreamReader direction validation: ticket #1808

**`P2: StreamReader does not validate CanRead on the base stream`**
(`REMED-IO-TEXT-WRAPPER-STREAM-DIRECTION`, P2, size S) is complete **for the reader
half**. No `SR-AUD-*` identifier; `SR-AUD-337` stays `confirmed`, `SR-AUD-338` stays
`remediated`, numbering frozen at 364.

`StreamReader(Stream*, bool)` now rejects a stream that exists but declares itself
unreadable, with `ArgumentException("Stream was not readable.")` — message only, no
`paramName` — after the null check. That is `StreamReader.cs:145-148` /
`Argument_StreamNotReadable` exactly, and byte-identical to `BinaryReader.cpp:25`,
which already did it.

**The defect is SR-AUD-338's laundering one level further out.** Measured on a
`FileStream(path, FileMode::Append)` (`FileAccess::Write` only, so `CanRead` is
false): `Read()` returned `-1` and `ReadToEnd()` returned `""`, so a stream that can
never be read was indistinguishable from an empty document
(`build-probe/1823_prefix_defects.log` cases 6 and 7).

**Rescoped to one half, with the reason measured rather than assumed.** #1808 was
opened covering both directions and required an inventory first, so that "the check
cannot reject a stream that is in fact usable". The inventory (#1823,
`docs/TextWrapperInputContractPlan.md` §5) found the two directions have **opposite**
compatibility, from one line — `Stream::getCanWriteProperty()` defaults to `false`
and `getCanReadProperty()` to `true`, where .NET makes both abstract. A `CanRead`
guard rejects only self-declared-unreadable streams; a `CanWrite` guard would also
reject every custom stream that implements `Write()` and never overrode the property,
one of which writes `"hello"` successfully today. The writer half is therefore blocked
ticket **#1824**, awaiting the approval a mandatory downstream migration needs.

Closure evidence: 10 permanent regressions, `SharpRuntimeTests_IO` **572/572** (was
562), clean under ASan + UBSan + LSan (`build-asan/1808_io_asan.log`). Repository
gate: 0 warnings, 0 errors, **14,070 tests across 37 executables**. No public
signature, virtual, vtable, layout or symbol changed.

Two further defects exposed by the same measurement are their own tickets rather than
folded in: **#1825** (`FileStream::Write` silently discards data written to a
read-only handle) and **#1826** (`MemoryStream::getCanReadProperty()` ignores
`isOpen_`).

### Completed FileStream access-flag validation: ticket #1825

**`P1: FileStream::Write silently discards data written to a read-only handle`**
(`REMED-IO-FILESTREAM-ACCESS-FLAGS`, P1, size S) is complete. It carries **no
`SR-AUD-*` identifier**; the audit did not record this defect, which is stated plainly
rather than backfilled, and the numbering stays frozen at 364.

`FileStream::Read`, `Write` and `WriteByte` inspected only `file_.is_open()`, never
`canRead_`/`canWrite_`. This was **data loss, not a late diagnostic**: an
`std::fstream` opened without `std::ios::out` accepts `write()`, sets `badbit` and
returns, and nothing inspected either the flag or the stream state, so the bytes were
dropped with no diagnostic anywhere. All three operations now test the flag **after**
the existing `is_open()` check and **before** the buffer/offset/count validation,
throwing `NotSupportedException` with .NET's `NotSupported_UnreadableStream` /
`_UnwritableStream` messages — the order and the wording of
`Strategies/OSFileStreamStrategy.cs:208-217` and `232-241`, and the same messages
`MemoryStream` and `UnmanagedMemoryStream` already threw for the same condition.
`FileStream` was the last stream in the module that did not.

**A fourth premise corrected, this one in #1825's own text.** The ticket said all three
operations "check only `file_.is_open()`". For `WriteByte` that was too generous: it had
**no validation at all**, so writing a byte to a *closed* `FileStream` was accepted in
silence while the `Write()` sibling beside it already threw
(`build-probe/1825_prefix_defects.log` case 4, a case the ticket did not predict). .NET
has no such gap because `OSFileStreamStrategy.cs:226-227` defines `WriteByte` in terms
of `Write(ReadOnlySpan<byte>)` and inherits both checks.

| Case | Before | After |
|---|---|---|
| `Write` on `FileAccess::Read` | accepted; file still `"seed"` | `NotSupportedException` |
| `WriteByte` on `FileAccess::Read` | accepted; file still `"seed"` | `NotSupportedException` |
| `Read` on `FileMode::Append` | `n=0`, indistinguishable from EOF | `NotSupportedException` |
| `WriteByte` after `Close()` | **accepted in silence** | `ObjectDisposedException` |
| `Write`/`Read` after `Close()` | `ObjectDisposedException` | unchanged |
| the valid read, write and `WriteByte` paths | correct | unchanged, byte-identical |
| closed **and** unwritable | `ObjectDisposedException` | unchanged — pinned by a test |

**Compatible narrowing, needing no approval.** Every newly rejected input already
failed: a write to a read-only handle never reached the file, and a read of a write-only
handle always returned 0. The only change is that the caller is now told.

Closure evidence: 7 permanent regressions, `SharpRuntimeTests_IO` **579/579** (was
572), clean under ASan + UBSan + LSan with activation proven rather than assumed
(`build-probe/1825_postfix_defects.log`, `build-asan/1825_io_asan.log`). Repository
gate: 0 warnings, 0 errors, **14,077 tests across 37 executables**;
`scripts/local_ci_check.sh build` passed. No public signature, virtual, vtable, object
layout or mangled symbol changed.

### Completed ZipArchive mode-range validation: ticket #1813

**`P2: ZipArchive silently accepts an out-of-range ZipArchiveMode value`**
(`REMED-IO-ZIP-INVALID-MODE`, P2, size S) is complete. It carries **no `SR-AUD-*`
identifier**; the audit recorded invalid mode values only as a missing-test note in
`ZipArchive.hpp.audit.md`'s "Other missing assertions" section, never as a finding, and
the numbering stays frozen at 364.

Both constructors tested the mode only with `== Read` / `== Create` / `== Update`, so a
value outside the enumerator set took none of the branches and produced a **zombie
archive**: no reader opened, no stream retained, no entries reported, nothing written
back. `validateZipArchiveMode()` now rejects any such value with
`ArgumentOutOfRangeException("mode")` — `ZipArchive.cs:979`'s
`default: throw new ArgumentOutOfRangeException(nameof(mode))`, verbatim.

**The severity is worse than the ticket said.** #1813 described an archive that "reports
no entries and writes nothing back", which reads as inert. It is not:
`build-probe/1813_prefix_defects.log` **case 9** builds a complete one-entry archive over
a perfectly good `MemoryStream` — `CreateEntry("lost.txt")`, `"DATA"` written to the entry
stream, `Dispose()` — with every step accepted, and the stream receives **0 bytes**. That
is the same silent-data-loss shape ticket #1812 removed from the null-stream path, reached
here with a valid stream. **Case 14** shows why `CreateEntry` does not catch it: that
method rejects only `mode == Read`.

**This defect is invisible to a sanitizer, which is worth recording.** `enum class
ZipArchiveMode` has the implicit fixed underlying type `int`, so holding 42, −1, `INT_MAX`
or `INT_MIN` in it is well-formed C++ with a well-defined value, not undefined behaviour.
UBSan reported nothing for cases 1–5 (measured, not assumed). Only an explicit range check
finds this class of defect — a useful counterexample to the batch's default sanitizer
strategy.

| Case | Before | After |
|---|---|---|
| 1–5 — stream ctor with 42, 3, −1, `INT_MAX`, `INT_MIN` | all construct successfully | `ArgumentOutOfRangeException("mode")` |
| 6 — **path** ctor with 42 | constructs successfully | same |
| 7 — `ZipFile::Open(path, 42)` | constructs successfully | same, inherited |
| 8 — `nullptr` **and** mode 42 | `ArgumentNullException("stream")` | unchanged — order pinned |
| 9 — mode 42 + `CreateEntry` + write + `Dispose` | **accepted; 0 bytes delivered** | cannot start |
| 10, 11, 12 — the valid Create, Read and Update paths | 146 bytes, 1 entry, 2 entries | unchanged, byte-identical |
| 13, 14 — destructor and observable state on mode 42 | zombie archive survives | cannot be constructed |

**Inventory result for the acceptance criterion.** `ZipFile::Open` is a bare forwarder to
the path constructor (`ZipFile.cpp:17`), so it is fixed **transitively** rather than
needing its own guard — and that is pinned by its own test, so a future refactor that
stops forwarding cannot silently lose the check. Validation order is .NET's in both
overloads: after the null-stream check for the `Stream*` ctor (`ZipArchive.cs:135`), and
before the path is stored or the file system touched for the path ctor
(`ZipFile.Create.cs:473-479`, which rejects the range before opening its `FileStream`).

**Explicitly excluded, and now blocked ticket #1827.** `ValidateMode` has a second half
(`ZipArchive.cs:962-975`) that rejects a stream whose *capabilities* contradict the mode.
This port validates none of it, and the guard cannot be added compatibly for the same
one-line reason that blocks #1824: `System::IO::Stream::getCanWriteProperty()` **defaults
to `false`** (`Stream.hpp:62`) where .NET's `Stream.CanWrite` is abstract, so a Create-mode
capability guard would reject every custom stream that implements `Write()` without
overriding the property. #1827 is opened `blocked`, with a note that it and #1824 share a
root cause and may deserve one design covering the `Stream.hpp` default itself.

**Compatible narrowing, needing no approval.** Every newly rejected input already
produced an unusable archive; no in-range value changed behaviour.

Closure evidence: 14 permanent regressions, `ZipArchiveTests` **42/42**,
`SharpRuntimeIntegrationTests` **857/857** (was 843), clean under ASan + UBSan + LSan with
0 reports (`build-probe/1813_postfix_defects.log`,
`build-asan/1813_integration_asan.log`). Repository gate: 0 warnings, 0 errors, **14,091
tests across 37 executables**; `scripts/local_ci_check.sh build` passed. No public
signature, virtual, vtable, object layout or mangled symbol changed.

### Completed MemoryStream disposed-CanRead fix: ticket #1826

**`P3: MemoryStream::getCanReadProperty() ignores the disposed state`**
(`REMED-IO-MEMORYSTREAM-CANREAD-DISPOSED`, P3, size XS) is complete. **No `SR-AUD-*`
identifier**; numbering stays frozen at 364.

`MemoryStream` did not override `getCanReadProperty()` at all, inheriting `Stream`'s base
default of `true`, so it kept claiming to be readable after `Close()`. It now returns
`isOpen_` (`MemoryStream.cs:99`). `getCanWriteProperty()` is deliberately left returning
`writable_` (`MemoryStream.cs:103`); the resulting asymmetry is **.NET's own** and is
documented in the header and pinned by a test citing both line numbers, so it cannot later
be normalised into a divergence.

**The predicted interaction, confirmed.** A `StreamReader` over a closed `MemoryStream` was
accepted at construction and failed only on first read (`prefix` case 5); it now throws
`ArgumentException("Stream was not readable.")` from the constructor, where .NET reports it.
Ticket #1808's guard existed for exactly this and the property had been defeating it.

**Inventory result.** Of nine `Stream` subclasses, `MemoryStream` was the only one
overriding `CanWrite`/`CanSeek` but not `CanRead`. `BufferedStream` folds `closed_` in and
delegates to its inner stream; `UnmanagedMemoryStream` folds `isOpen_` in; `NetworkStream`
folds `fd_ >= 0` in; `FileStream` folds neither, unobservably after #1825.

**Separate defect found → inactive ticket #1828.** The three zlib wrappers answer
`CanRead`/`CanWrite` from `mode_` alone where `DeflateStream.cs:171-195` folds in both the
disposed state and the inner stream's capability (cases 7–9). Case 10 records that #1826
made this **visible** rather than causing it: `CanRead=1 inner-CanRead=1` before (accidentally
consistent), `CanRead=1 inner-CanRead=0` after (a contradiction). #1828 is blocked because
its delegation half meets `Stream.hpp:62`'s default-`false` `getCanWriteProperty()` — the
root cause it now shares with #1824 and #1827.

Closure evidence: 7 permanent regressions, `MemoryStreamTests` 64/64,
`SharpRuntimeTests_IO` **586/586** (was 579), clean under ASan + UBSan + LSan with 0 reports.
Repository gate: 0 warnings, 0 errors, **14,098 tests across 37 executables**;
`scripts/local_ci_check.sh build` passed. No member added, layout unchanged, no vtable slot
added.

### Completed CCF-004 family plan: ticket #1829

**`P1: plan the CCF-004 defined-arithmetic family before implementing any of its eight
findings`** (`REMED-CORE-CCF004-PLAN`, P1, size M, design-only) is recorded in
[`docs/DefinedArithmeticBoundaryPlan.md`](docs/DefinedArithmeticBoundaryPlan.md). No
production source changed under it. **No `SR-AUD-*` identifier**; all eight members keep
status `confirmed` and numbering stays frozen at 364.

All eight members were re-reproduced under UBSan on 2026-07-29 rather than taken from the
audit's wording (`build-probe/1829_ccf004_survey.cpp`, 16 cases, one process each,
`-fno-sanitize-recover`). Every one still reproduces.

**Three survey findings that change the work:**

1. **The members split three ways** — 6 defined-wrap sites whose current result is
   already the intended value and whose repair changes nothing observable; 1
   validate-first ordering fix; and 2 that produce a **wrong answer** today
   (`TimeSpan::TryParse` returns `parsed=1` with `ticks=-7695280436664713216` for a
   positive input, and the `DateOnly` arithmetic). Only the last two need a compatibility
   argument, and it is the one already accepted for #1817/#1818/#1825.
2. **SR-AUD-060 is seven sites, not four** — the overflow cascades into `jdnToDate` at
   `DateOnly.cpp:35/37/39`, so cases 8 and 9 each report four UB operations.
3. **A methodology trap** — the first survey run called SR-AUD-049, SR-AUD-060 and
   SR-AUD-008 already fixed, which is false: a probe linked against `build/` cannot see
   any `.cpp`-side site, and `-O1` constant-folds an inlined header overflow so it emits
   no check. Link against `build-asan/` at `-O0`.

**No new shared infrastructure is needed.** .NET's idiom (`DateOnly.cs:73-81`, `:121-132`)
already exists correctly at `ReadOnlyMemory.hpp:120-131`; each remaining application is a
local one-line change, and a proposed `SafeArithmetic` helper should be rejected.

Implementation split: **#1830–#1837**, all `todo`, all compatible, none requiring
approval. #1836 and #1837 should not be taken first.


### Autonomous batch handoff, 2026-07-29 (text/IO remediation and the CCF-004 family plan)

Six tickets on `feature/remediation-batch-text-io-ccf004`: **#1825** (FileStream access
flags), **#1813** (ZipArchive mode range), **#1826** (MemoryStream disposed CanRead),
**#1829** (CCF-004 family plan, design-only), **#1830** (Index/Range defined arithmetic,
**SR-AUD-057 remediated**) and **#1832** (IntPtr defined wrap, **SR-AUD-025 remediated**).

Baselines, all verified rather than carried forward: repository gate **14,113 tests across
37 executables**, 0 warnings; audit **23 remediated / 341 confirmed / 364**; module graph
41 / 91; canonical Doxygen **1,941** against the 1,942 ceiling; negative fixtures 9 / 66;
version seams 2 / 18.

Five premises were corrected by measurement, including two in the batch's own documents:
SR-AUD-060 is seven sites not four, SR-AUD-057 is two sites not one, and #1829's own first
survey run wrongly reported three of eight members as already fixed. The two methodology
rules that follow — link probes against `build-asan/` at `-O0`, and enumerate sites with
the *recovering* sanitizer build — are recorded in
`docs/DefinedArithmeticBoundaryPlan.md` §3 and §12.

Ready queue: **#1831**, **#1833**, **#1834**, **#1835** (all compatible, no approval), then
the two class C tickets **#1836** and **#1837** last. Blocked: **#1773** (untouched),
**#1804**, and **#1824** / **#1827** / **#1828**, which share one root cause —
`Stream.hpp:62`'s default-`false` `getCanWriteProperty()` — and are better served by one
design covering that line than by four per-type guards.

Full detail, including build directories, the three-job parallelism record and the
`build-probe` disk accounting, is in `NEXT.md` under "CONTEXT-REFRESH handoff — 2026-07-29,
text/IO + CCF-004 batch".

### Autonomous batch handoff, 2026-07-30 (CCF-004 class A/B closure and the Stream capability design)

Seven tickets on `feature/remediation-batch-ccf004-stream-design`: **#1831**
(`tupleHashCombine`, **SR-AUD-062 remediated**), **#1833** (`ReadOnlyMemory::Slice`,
**SR-AUD-049 remediated**), **#1834** (`Int128` MinValue, **SR-AUD-019 remediated**), **#1835**
(`Utf8Parser` Int64 minimum, **SR-AUD-084 remediated**), **#1839** (the shared Stream capability
design, design-only), **#1841** (zlib closed-state capabilities) and **#1840** (Stream capability
documentation).

**CCF-004's class A and class B members are now complete.** Six of the eight are done
(#1830, #1831, #1832, #1833, #1834, #1835); only the two **class C** members remain, #1836
(`TimeSpan`) and #1837 (`DateOnly`), both still **ready** and both now reproduced against a
current sanitizer tree with their evidence recorded as
[`docs/DefinedArithmeticBoundaryPlan.md`](docs/DefinedArithmeticBoundaryPlan.md) §16.

Baselines, all verified rather than carried forward: repository gate **14,145 tests across 37
executables**, 0 warnings; audit **27 remediated / 337 confirmed / 364**; module graph 41 / 91;
canonical Doxygen **1,941** against the 1,942 ceiling; negative fixtures 9 / 66; version seams
2 / 18.

**Seven premises were corrected by measurement, four of them in this repository's own design
documents.** The plan's §2 lists a finding by the site that *reports*, which is right for
enumerating a repair and wrong for enumerating a public surface: `Int128` had four more public
doors and `Utf8Parser` four more overloads than §2 names (§15). UBSan **deduplicates by source
location**, so shapes sharing a line hide each other even in a recovering build — one process per
shape (§13.2). "Largest operand" is **not** the worst case for a shift-then-add step (§13.1).
`SR-AUD-049` really is one site, and recording a count that did **not** move matters as much as
raising one (§14.2). The `Stream` family's premise was a third of the problem: all **three**
capabilities have different undocumented defaults and the two failure directions are **opposite**.
`TimeSpan::Parse` does not throw either, and `DateOnly::AddYears` is a **second**
silent-wrong-answer member where the plan claimed there was only one (§16.1, §16.4).

Ready queue: **#1836** and **#1837** (the two class C CCF-004 members, evidence already
captured), then **#1838** (`SslApplicationProtocol` hash, XS) and **#1842** (`FileStream` closed
capabilities, XS). Blocked: **#1773** (untouched), **#1804**, and **#1824** / **#1827** /
**#1828**, which now share **one** stated approval —
[`docs/StreamCapabilityContractDesign.md`](docs/StreamCapabilityContractDesign.md) §6.2 — whose
in-repository migration cost was **measured** at one line in one test double rather than
estimated.

Full detail, including build directories, the three-job parallelism record and the `build-probe`
disk accounting, is in `NEXT.md` under "CONTEXT-REFRESH handoff — 2026-07-30".

### Autonomous batch handoff, 2026-07-30 (CCF-004 closure and the Stream capability family)

Seven tickets on `feature/remediation-batch-ccf004-stream-capabilities`: **#1836**
(`TimeSpan` defined ticks, class A+C — the previous session's uncommitted work, verified and
committed), **#1837** (`DateOnly` defined arithmetic, class C — **closes CCF-004, 8/8 members**),
**#1842** (`FileStream` closed-state capabilities, the wrapper-guard prerequisite), **#1838**
(`SslApplicationProtocol::GetHashCode` unsigned djb2, a #1831 side-finding), **#1824**
(`StreamWriter` `CanWrite` guard), **#1828** (zlib wrappers' inner-stream delegation) and
**#1827** (`ZipArchive` per-mode capability guard — **closes the Stream-capability family**). The
last three used the single `docs/StreamCapabilityContractDesign.md` §6.2 approval the previous
handoff requested; #1836/#1837 carry the class-C compatibility argument in-ticket per the
defined-arithmetic plan §9, no new approval.

Gate **14,196 tests across 37 executables** (0 warnings, 0 errors), up from 14,145. Audit index
**29 remediated / 335 confirmed of 364** — SR-AUD-008 and SR-AUD-060 flipped to `remediated`; the
other five tickets carry no `SR-AUD-*` and numbering stays frozen at 364. Module graph **41 / 91**,
canonical Doxygen **1,941 / 1,942**, negative fixtures **9 / 66**, version seams **2 / 18** — all
unchanged (no boundary, seam, or fixture surface changed).

Premises corrected by measurement: SR-AUD-060 is **seven** sites with a `jdnToDate` cascade
reachable without an entry-point overflow, and CCF-004 has **two** silent-wrong-answer members
(`DateOnly::AddYears`, not only `TimeSpan::TryParse`); SR-AUD-008 is **six** sites / **five**
public doors; the DateOnly paramName decision adopted .NET's per-method names over the leaked
`year`; and a UBSan sweep enumerates undefined operations, not wrong answers (two members had
wrong answers UBSan is silent about). No new tickets and no new defects; every surface addition
was recorded by appending to the owning finding. The `git` branch was pushed/merged/tagged
**not at all** — everything is local.

Ready queue is **drained of compatible remediation work**: the only open tickets are **#1773**
(blocked on CNA/mobile-eggbert, downstream not inspected) and **#1804** (a P3 tooling false-pass
needing a design decision). Next recommended: a `plan.sqlite3` namespace-review pass, or a #1804
design ticket.

Full detail, including build-directory sizes, the three-job parallelism record and the
`build-probe` accounting, is in `NEXT.md` under "CONTEXT-REFRESH handoff — 2026-07-30, CCF-004
closure + Stream capability family".

### Autonomous batch handoff, 2026-07-30 (CCF-003 close + CCF-005 plan)

Branch `feature/remediation-batch-ccf003-ccf005-plan`, **eight commits**. Follows the
same-day `feature/remediation-batch-1804-namespace-review` batch (#1804 seam-checker
gap, #1843 UInt128 shift UB, and the numeric-wrapper namespace review that opened
#1844–#1848), which raised the gate 14,196 → 14,199.

**CCF-003 (numeric primitive-wrapper boundary) is CLOSED.** Four implementation
tickets landed: **#1846** (SR-AUD-022 — 11 `Clamp` overloads throw `ArgumentException`
on `min>max` instead of reaching `std::clamp` `[alg.clamp]` library UB, reproduced via
`-D_GLIBCXX_ASSERTIONS`), **#1844** (SR-AUD-024 — `SByte`/`Int16` `IsPositive` → `>=0`),
**#1845** (SR-AUD-023 — integral binary `ToString("B"/"b")`, +Int128 premise fix),
**#1847** (SR-AUD-021 integer slice — unknown format → `FormatException`, +128-bit `G/g`
and width-`std::stoi` premise fixes). Plus tooling **#1848** (exec bit). All five
CCF-003 findings (020–024) are now `remediated`; 019 was already closed under CCF-004.
Durable record: `docs/NumericWrapperBoundaryPlan.md` §14.1/§15.

**CCF-005 (conversion/memory-safety) is PLANNED and one fix landed.**
`docs/ConversionBoundaryFamilyPlan.md` is the #1815/Base64-quality plan for the
memory-safety slice (SR-AUD-026/027/041/043/047, three ASan-confirmed OOB), verified
against current source by four parallel read-only agents. Tickets **#1850–#1854**
opened, dependency-ordered. **#1850** (SR-AUD-047 — static `MemoryExtensions::CopyTo`
throws before overflowing a short destination; ASan-confirmed heap write) is done. The
rest are the queue; **#1854** is `needs_user` (dropping `noexcept`/`constexpr` on
`ReadOnlyMemory` ctors + `HashCode::AddBytes` — defense-in-depth after #1852).

Gate **14,233 tests across 37 executables** (0 warnings, 0 errors), up from 14,199.
Audit index **35 remediated / 329 confirmed of 364** (+4 CCF-003, +1 CCF-005). Module
graph **41 / 91**, canonical Doxygen **1,941 / 1,942**, negative fixtures **9 / 66**,
version seams **2 / 18** (self-tests 15) — all unchanged.

Premises corrected by measurement: **Int128 also lacked** binary `ToString` (SR-AUD-023
surface extended to 7 types); the **128-bit types had no explicit `G/g` branch** and an
**unguarded width `std::stoi`** (SR-AUD-021); the **Single/Double `std::stoi` float
slice** of SR-AUD-021 was split to new inactive ticket **#1849** (CCF-006), not falsely
closed; and in CCF-005, SR-AUD-027 spans two files, SR-AUD-043 needs **no** layout
change (.NET keeps the span length signed), and SR-AUD-044 (`std::copy` overlap) was
kept out of #1850. Nothing pushed/merged/tagged; CNA/mobile-eggbert untouched; #1773
stays `blocked`.

Ready queue: **#1851** (SR-AUD-041 BitConverter bounds), **#1852** (SR-AUD-043a Span
ctors), **#1853** (SR-AUD-026/027 Convert) — all P1/P2 compatible; then **#1854**
(needs_user) and the CCF-005 Decimal slice / CCF-006. Full detail, build-directory
sizes, three-job parallelism record and probe accounting: `NEXT.md` under "Autonomous
batch handoff, 2026-07-30 (CCF-003 close + CCF-005 plan)".

## Session summary — 2026-07-30, CCF-011 empty-callable family closed

Branch `feature/remediation-batch-empty-callable`, off
`feature/remediation-batch-floating-fidelity`. CCF-007 had no remaining
*compatible* ready work and `plan.sqlite3` held **no `todo` ticket at all**, so
the batch selected a new family from the audit index: **CCF-011 — "empty
`std::function` values cross public boundaries without an explicit policy"**.
Design ticket **#1866** wrote `docs/EmptyCallableBoundaryPlan.md` (18 sections,
every current-behaviour claim measured by a 60-case probe, every .NET claim cited
to a file and line under `/rv/tmp/runtime`), then four implementation tickets
landed it. **CCF-011 is CLOSED**: all six findings are `remediated`.

- **#1867** — SR-AUD-065 + SR-AUD-099. `Lazy<T>`'s three factory constructors
  throw `ArgumentNullException("valueFactory")` for an empty factory;
  `AggregateException::Handle` throws `ArgumentNullException("predicate")` before
  its loop. +14 tests.
- **#1868** — SR-AUD-058 + SR-AUD-121. `Progress<T>::addProgressChangedHandler`
  and `EventHandler::Add`/`operator+=` treat an empty handler as a **no-op**,
  before the replay hook, matching C# `event += null`; `OnReport`/`Raise` skip
  untruthy handlers. +13 tests.
- **#1869** — SR-AUD-052. All 17 `Array` delegate overloads reject an empty
  callable with .NET's own `paramName`, in .NET's own per-overload order; the 12
  `Predicate<T>` parameters renamed `predicate` → `match`. +13 tests.
- **#1870** — SR-AUD-134. All 11 `Linq` callback overloads reject before the
  sequence is examined. +8 tests.

Gate **14,444 tests across 37 executables** (0 warnings, 0 errors), up from
14,396. Audit index **49 remediated / 315 confirmed of 364** (+6). Module graph
**41 / 91**, canonical Doxygen **1,941 / 1,942**, negative fixtures **9 / 66**,
version seams **2 / 18** — all unchanged, as this family adds none.

Premises corrected by measurement, historical audit text preserved: the silent
region was **size**-dependent, not emptiness-dependent (`Sort`, `BinarySearch`,
`OrderBy`, `OrderByDescending` were silent for a *one-element* input too, because
`std::sort`/`std::stable_sort` never compare below two); `Linq::First(empty, {})`
**masked** the argument error with `InvalidOperationException`;
`AggregateException::Handle` was silent with **no** inner exception;
`EventHandler`'s failure happened **inside `Add`**, not at `Raise`, whenever a
replay hook was set; and `Array::FindLastIndex` validated its range before the
callable where .NET does the opposite — folded into #1869, **no new SR-AUD
identifier** (numbering stays frozen at 364).

Two observable changes are recorded rather than assumed away and neither is
approval-gated: a previously silent normal result now throws (B1), and
`EventHandler::Size()` no longer counts a subscription that could never have been
invoked (B2). No signature, `noexcept`, vtable, layout, grammar, formatted output
or numerical behaviour changed anywhere in the family.

Nothing pushed/merged/rebased/tagged; CNA and mobile-eggbert untouched; #1773
stays `blocked`; #1854/#1858/#1862/#1863/#1865 remain `needs_user` with their
`docs/FloatingValueFidelityPlan.md` §19 decision records unchanged.

Ready queue: **empty** — every open ticket is `blocked` or `needs_user`. The next
batch should either obtain the §19 decisions or plan another compatible family;
the strongest candidates are CCF-014 (`TryRead`/`TryParse` stale output, 2
findings), CCF-016 (derived HResults, 5 findings, mechanical) and CCF-002
(date/time validation). Full detail, build-directory sizes, three-job parallelism
record and probe accounting: `NEXT.md` under "Autonomous batch handoff,
2026-07-30 (CCF-011 empty-callable family, CLOSED)".

## Session summary — 2026-07-30, CCF-014 and CCF-016 both closed

Branch `feature/remediation-batch-ccf014-ccf016`, off
`feature/remediation-batch-empty-callable`. The ready queue was empty, so the
batch took the two families NEXT.md recommended, planning each before touching
code. **Both are now CLOSED.**

**CCF-014 — Try-style failure output.** Design ticket **#1871** wrote
`docs/TryOutputFailureContractPlan.md` (18 sections) from a 26-case probe that
prepopulates every output with a caller sentinel. Implementation **#1872**
remediated SR-AUD-075 and SR-AUD-085 across **11 public entries** — not the two
the cross-cutting record names: `SequenceReader<T>::TryRead`/`TryPeek` assign
`T{}` on their end-of-sequence branch, and all nine `Utf8Parser::TryParse`
overloads route their ten failure exits through one shared private
`fail(value, bytesConsumed)`. +14 tests.

The cause was a language guarantee lost in translation: a C# `out T` parameter is
definitely assigned on every returning path, so the reference's `value = default`
lines are mandatory. Porting `out T` to a C++ `T&` dropped the guarantee while
keeping the `bytesConsumed = 0` half, which is exactly why a *checked* failure
still looked like a stale success.

**CCF-016 — derived exception HResults.** Design ticket **#1873** wrote
`docs/DerivedExceptionHResultPlan.md` (13 sections). Implementation **#1874**
remediated SR-AUD-093/094/095/096/100 across **11 types and 40 constructors** —
38 wrong results before, `wrong=0` after. The cause was structural: `Exception`
initialises `hResult_` and `SystemException`'s body overwrites it, so a type
written as a pure forwarding constructor inherits whatever its nearest base last
wrote. +18 tests.

Gate **14,476 tests across 37 executables** (0 warnings, 0 errors), up from
14,444. Audit index **56 remediated / 307 confirmed of 364** (+7). Module graph
**41 / 91**, canonical Doxygen **1,941 / 1,942**, negative fixtures **9 / 66**,
version seams **2 / 18** — all unchanged.

Premises corrected by measurement, historical audit text preserved: CCF-014's
defect is **grammar-independent** and also strikes **after a successful core
parse**, `bytesConsumed` was never wrong, and the `FormatException` path's
unwritten outputs are **parity with .NET's deliberate `Unsafe.SkipInit`**, not a
defect; CCF-016's five findings cover **eleven** types, `AggregateException` is
**correctly** inheriting `COR_E_EXCEPTION` because .NET assigns none either, and
**SR-AUD-100's message claim is a false positive** — the port's text is
byte-identical to `SR.Arg_DuplicateWaitObjectException`, so no message change was
made and a test pins it verbatim.

Both implementations are **mutation-checked**: reverting `Utf8Parser::fail`'s
value write fails 5 tests, `SequenceReader::TryRead`'s fails 3, and deleting one
`DllNotFoundException` HResult assignment fails 2.

New ticket **#1875** is open and **inactive**: a sweep found 45 of 59 exception
types outside `modules/core/include/System/` with no explicit HResult. None is a
confirmed defect — `AggregateException` proves inheriting can be right — so it
carries the measured evidence, no `SR-AUD-*` identifier, and an explicit
"confirm before starting".

Nothing pushed/merged/rebased/tagged; CNA and mobile-eggbert untouched; #1773
stays `blocked`; #1854/#1858/#1862/#1863/#1865 remain `needs_user` with their
`docs/FloatingValueFidelityPlan.md` §19 decision records unchanged.

Ready queue: **only the inactive #1875**. Recommended next families are CCF-002
(date/time validation), CCF-012 (composite-format grammar), CCF-017 (Attribute
identity) and the CCF-019 remainder (JsonNode / XObject use-after-free, likely
design-first). Full detail, build-directory sizes, three-job parallelism record
and probe accounting: `NEXT.md` under "Autonomous batch handoff, 2026-07-30
(CCF-014 + CCF-016, both CLOSED)".

## Session summary — 2026-07-30, CCF-002 partially remediated

Branch `feature/remediation-batch-ccf002-datetime-validation`, off
`feature/remediation-batch-ccf014-ccf016`. The ready queue held only the
deliberately inactive #1875, so the batch took the family NEXT.md recommended
first, planning it before touching code. **CCF-002 is now PARTIALLY REMEDIATED
— not closed**, and the plan says so in its own completion criteria rather than
letting "three of five classes done" read as closure.

**Design first.** Ticket **#1876** wrote
`docs/DateTimeValidationBoundaryPlan.md` (20 sections) from
`build-probe/1876_datetime_validation_probe.cpp`: 84 non-sanitised cases plus
five one-shape-per-process UBSan runs. It inventoried **five** component
constructors, **six** wrappers that add no validation of their own, and
**eight** parse entries across **four** types — where the cross-cutting record
names three findings and six files. It also pulled in SR-AUD-061
(`DateOnly::TryParse`), which is the same `std::sscanf`-prefix omission in the
same module and is not listed under CCF-002.

**The line between compatible and gated** was drawn exactly where #1857→#1858
and #1864→#1865 drew it: a change to the accepted **range of component values**
is compatible; a change to the accepted **textual grammar** is approval-gated.

**#1877 — SR-AUD-006, remediated (+32).** `DateTime::dateToTicks` validated
year/month/day and then multiplied hour/minute/second/millisecond straight into
the tick sum. That single omission produced silent normalisation
(`DateTime(2024,1,1,-1,0,0)` returned a date in the *previous year*), a breach
of the class's own documented `[0, MaxTicks]` invariant
(`DateTime(9999,12,31,24,0,0)` stored `MaxTicks + 1`), and — not in the audit
record — **undefined behaviour**: `hour * TicksPerHour` overflows `int64` for
`|hour| > 256204778`, UBSan-confirmed from a plain constructor *and* from
`DateTime::TryParse("2024-06-15 2000000000:00:00")`, which returned **`true`**
with a negative tick count. The repair validates all four components before any
arithmetic, in .NET's order, using the messages and `paramName`s that
`TimeOnly::validateHms` — 45 lines away — already carried. `DateTimeOffset`'s
offset-first validation order was restored in the same commit, because adding
the hour check alone would have moved two doubly-invalid cases off the exception
.NET reports.

**#1878 — SR-AUD-007a, remediated (+6).** One unchecked `sscanf` line carried
three defects: an impossible minute field absorbed into the `TimeSpan` before the
±14 h guard could see it (`"+02:75"` meant +03:15), a negative field inverting
the sign the caller wrote (`"+-05:00"` meant *minus* five hours), and a large
hour field making **`TryParse` itself throw** `OverflowException` from outside
its own `try`/`catch` — the very block whose comment states the method must never
throw. One bounds-before-arithmetic guard closes all three. No new `SR-AUD-*`
identifier was issued for the throwing defect: it is inseparable from the repair,
and numbering stays frozen at 364. SR-AUD-007 is now the split row
`007a remediated / 007b open`.

Gate **14,514 tests across 37 executables** (0 warnings, 0 errors), up from
14,476. Audit index **57 remediated / 306 confirmed of 364**. Module graph
**41 / 91**, canonical Doxygen **1,941 / 1,942**, negative fixtures **9 / 66**,
version seams **2 / 18** — all unchanged.

Premises corrected by measurement, historical audit text preserved: the
"Required post-audit verification" text of **all four** CCF-002 reports asks for
an assertion that a failed `TryParse` does **not** overwrite its output —
.NET does the opposite (`DateTimeParse.cs:2470` assigns `MinValue`), so
implementing those paragraphs literally would have pinned a divergence as the
contract; SR-AUD-006 understates its own severity by omitting the UB; the port's
year/month/day exception identity already deviates from .NET and is deliberately
excluded; `DateTimeOffset` validated the offset last only because a
mem-initialiser cannot sequence a check before a member's construction;
SR-AUD-009's fractional scan is **not** generally wrong (`"10:20:30.1"` correctly
yields `.100`); and `TimeOnly` is a **counter-example inside the family**, not a
member of its constructor half.

Both implementations are **mutation-checked** with four mutations: deleting the
hour/minute/second check fails 15 permanent tests, the millisecond check 7,
restoring the pre-repair `DateTimeOffset` ordering 2, and deleting the
offset-field guard 3. Mutation 3 initially failed to *build*
(`-Werror=unused-function`), which would have silently re-run the previous
binary; it was redone before its result was believed.

New tickets **#1879** (`needs_user` — the parser accepted-grammar change, with
fifteen exact before/after rows in the plan §20.1) and **#1880** (`todo`,
inactive — `TryParse` failure-output normalisation, no `SR-AUD-*` identifier,
deliberately inactive rather than `needs_user` because choosing needs its own
probe of same-repository `Try*` conventions).

Nothing pushed/merged/rebased/tagged; CNA and mobile-eggbert untouched; #1773
stays `blocked`; #1875 was left inactive and untouched;
#1854/#1858/#1862/#1863/#1865 remain `needs_user` with their
`docs/FloatingValueFidelityPlan.md` §19 decision records unchanged.

Ready queue: **empty** — every open ticket is `blocked`, `needs_user`, or
deliberately inactive. There is no compatible CCF-002 work left: SR-AUD-009 and
SR-AUD-061 are pure grammar, and both parsers already range-check their
components. Recommended next work is the #1879 decision, then CCF-012
(composite-format grammar), CCF-017 (Attribute identity) or the CCF-019
remainder. Full detail, build-directory sizes, three-job parallelism record and
probe accounting: `NEXT.md` under "Autonomous batch handoff, 2026-07-30
(CCF-002, PARTIALLY REMEDIATED)".

---

## Autonomous remediation batch, 2026-07-30 — CCF-012 composite formatting (#1881, #1882, #1883, #1884)

Branch `feature/remediation-batch-ccf012-composite-format`, off
`feature/remediation-batch-ccf002-datetime-validation`. The ready queue held
only the deliberately inactive #1875 and #1880, so the batch's first work unit
was the family choice itself — and it was made by **measurement, not by title
or finding count**. **CCF-012 is now PARTIALLY REMEDIATED — not closed**, and
the plan said so in its own completion criteria in advance rather than after
the fact.

**The comparison, run before anything was chosen.** One probe
(`build-probe/1881_family_compare_probe.cpp`, plus `1881_extra_probe.cpp`)
reproduced **both** CCF-012 and CCF-017 against the shipped headers in one
process tree, every case forked under a 3-second watchdog so a hanging case
could not hide the ones after it. The full table is section 0 of
`docs/CompositeFormatBoundaryPlan.md`. CCF-012 and CCF-017 are both one
`medium` finding in `Core.Base`; what separated them is that CCF-012's measured
severity is **non-termination + UB + silent corruption + an escaped `std::`
exception** across 23 public entries and 22 wrappers, while CCF-017 is a
value-semantics divergence with nothing for a sanitizer to find.

**CCF-017 was deferred, not closed, downgraded or reclassified.** SR-AUD-114
stays `confirmed` and reproduces exactly as filed. Its demanded repair — .NET's
same-type **fieldwise** `Equals`/`GetHashCode` — requires runtime reflection,
which CLAUDE.md lists as a permanent out-of-scope deviation, so no C++
base-class policy can implement it; and the only shapes that *are*
implementable (per-type overrides) are the ones the finding explicitly rejects.
Everything left of it is approval-gated: making `Attribute` abstract is a public
source break, and the green `AttributeTests` fixture deliberately pins
`Attribute a, b` as unequal. **CCF-019 was considered and not taken** — none of
its gate conditions holds, and both of its open members need public
ownership-model changes this batch may not infer permission for.

**Design first.** Ticket **#1881** wrote
`docs/CompositeFormatBoundaryPlan.md` (20 sections plus the comparison in §0).
It inventoried **22** `String::Format` overloads sharing three helpers, **4**
`FormattableString` entries, and **22** `StringBuilder::AppendFormat` /
`Console::Write`/`WriteLine` wrappers that inherit every defect — where the
cross-cutting record names "the String path" and "the FormattableString path"
and no wrapper at all.

**The line between compatible and gated** was drawn where #1857→#1858,
#1864→#1865 and #1878→#1879 drew it: repairing behaviour that is undefined,
non-terminating, a leaked foreign exception or a silently wrong value is
compatible; changing the accepted **textual grammar**, or what a
currently-succeeding call returns, is approval-gated.

**#1882 — SR-AUD-015's String half, remediated (+34).** `String::Format`
produced its output by repeatedly mutating a buffer that already held
substituted argument text. That one property produced four defect classes the
audit never named. `replaceArg` reset its scan cursor to 0 after every
substitution, so it re-read what it had just inserted: `Format("{0}", "{0}")`
**never returned**, and this needed no malformed format string —
`Format("{0}", userText)` hung whenever `userText` contained `{0}`, a
public-input denial of service in the library's most-called formatting entry.
`fmtInt`'s padding loop prepended one character at a time, so
`Format("{0:D999999999}", 7)` was a **second, unrelated** non-termination
(~10^18 byte copies). `std::abs(std::stoi(...))` on `INT_MIN` was
UBSan-confirmed undefined behaviour at `String.cpp:149`. And `std::out_of_range`
and `std::invalid_argument` **escaped a System-shaped public API**, while
`extractSpec` gave every occurrence of an index the *first* occurrence's
specifier (`"{0:X}/{0:D3}"` → `"FF/FF"`). The repair replaces all three helpers
with one `formatCore` that walks the format string **once** and appends to a
separate output — .NET's own `AppendFormatHelper` shape — and gives
`fmtInt`/`fmtDouble` a bounded, non-throwing specifier parse that keeps
`std::stoi`'s prefix semantics (so `"{0:D-3}"` still yields `"007"`) while
adopting the reference's own digit bound.

**#1883 — SR-AUD-015's FormattableString half, remediated (+20).** The same
cause in a different shape: one full find/replace sweep over the *result* per
argument index, so text inserted for an earlier index was re-read while a later
one was substituted. Exactly **one row of its eight-case matrix** changed —
`FS("{0}", {"{1}", "second"})` now returns `"{1}"` instead of `"second"` — and
the other seven are byte-identical. The per-file report's attribution of the
escaped-brace result to brace handling was corrected: there was no escape
handling at all, and the real mechanism was the **outer** loop, which is why an
argument naming its *own* index was already safe while one naming a later index
was not.

**#1884 — created `needs_user`, not implemented.** This finding's own headline
claims are the gated remainder: `{{`/`}}` escaping, rejecting a malformed
closing brace, alignment padding, and `FormattableString`'s missing-index
`FormatException`. All four change what currently-succeeding calls return or
whether they throw, across **46 public entries**. Fourteen exact before/after
rows and the precise approval wording are in the plan's §20. Four
`PinsCurrentGrammar_*` tests in each suite assert today's behaviour so #1884
cannot land silently.

**No new `SR-AUD-*` identifier was issued**; numbering stays frozen at **364**.
All four newly measured classes were found while remediating an existing finding
in the files that finding owns, so they are folded into SR-AUD-015's record by
appending, following the SR-AUD-081 / SR-AUD-362 convention.

**Evidence.** 54 permanent add-only regressions (30 `StringFormatBoundaryTests`,
20 `FormattableStringBoundaryTests`, 4 `StringBuilderTests`); every pre-existing
test passes unmodified. Repository **14,568 tests across 37 executables**, from
14,514. Clean build, zero warnings; `scripts/local_ci_check.sh build` passed.
**Mutation-checked four ways**, each rebuilt and re-executed. UBSan silent at
`String.cpp:149` afterwards, with the probe compiled *together with*
`String.cpp` so the changed code is instrumented directly. ASan + UBSan + LSan
over 3,675 `String::Format` cases and the full `FormattableString` matrix, every
format string and operand built at run time so constant folding cannot suppress
a diagnostic: zero diagnostics, zero leaks, **0 escaped** non-`System`
exceptions. TSan recorded **not applicable**. Source, ABI, `noexcept` and layout
consequences: **none** in either ticket.

Ready queue: **empty** — every open ticket is `blocked`, `needs_user`, or
deliberately inactive. #1773 stays `blocked` and no downstream repository was
inspected; #1875 and #1880 were left inactive and untouched;
#1854/#1858/#1862/#1863/#1865/#1879 remain `needs_user` with their decision
records unchanged. Recommended next work is a **design-only** CCF-019 ticket
(SR-AUD-327 `JsonNode`, SR-AUD-333 `XObject` — ASan-confirmed use-after-free,
needing an ownership design and an explicit approval before any production
change), or a design-only CCF-017 ticket recording its permanent-deviation
argument. Full detail, build-directory sizes, three-job parallelism record and
probe accounting: `NEXT.md` under "Autonomous batch handoff, 2026-07-30
(CCF-012, PARTIALLY REMEDIATED)".

---

## Autonomous design batch, 2026-07-30 — CCF-019 owned-tree lifetimes (#1885, #1886–#1894)

Branch `feature/design-ccf019-owned-tree-lifetimes`, off
`feature/remediation-batch-ccf012-composite-format`. **DESIGN-ONLY by explicit
instruction: no production file was changed and no CCF-019 implementation is
approved.** CCF-019 is **DESIGN-COMPLETE, NOT REMEDIATED**, and must not be
reported otherwise.

**Reproduced before anything was designed.** 47 cases against the shipped
bodies, one `fork()`ed process each under a 5-second watchdog, in **three builds
from one source**: `-fsanitize=address,undefined`, `-fsanitize-recover=address`
(`halt_on_error=0`, so every faulting access is counted rather than the first),
and no sanitizer (the *silent* shapes ASan's quarantine hides). Freshness is
structural, not asserted: **one** `g++` invocation compiles the probe together
with **60 production translation units** — all of `Text.Json`'s node body, all of
`Xml.Linq`, and the `Xml` DOM they link against — so **no prebuilt archive is
linked** and a stale or non-instrumented object is impossible.

Measured: **29 ASan `heap-use-after-free` accesses** (8 `JsonNode`, 21
`XObject`), **3 ASan `stack-overflow`s**, **57 reads-after-free and zero writes**
under recoverable ASan, and **12** further cases that give a wrong answer with
**no diagnostic in any build**. Two of those twelve are only visible without a
sanitizer: a freed `JsonArray` slot refilled by a `JsonObject` at the identical
address (the retained child then reports the wrong `GetValueKind`), and a freed
`XElement` slot refilled by another element (the retained text node then reports
the squatter's name).

**Nine premises corrected by measurement**, historical audit text preserved and
corrections appended to both owning per-file reports,
`AUDIT_CROSS_CUTTING_FINDINGS.md`, `AUDIT_FINAL_REPORT.md` and
`AUDIT_PROGRESS.md`. The largest: the surface is **76 public entries across 27
headers and 13 bodies**, not the nine files and two accessors the cross-cutting
record names; **SR-AUD-333's severity is understated** because
`XObject::getParentProperty` dispatches a **virtual call** through the freed
parent, so eight public entry points abort the process (`pure virtual method
called`) **with no sanitizer present**; the two members are **structurally
asymmetric** (`XObject` deletes copy/move, `JsonNode` does not, so
`JsonArray copy = *orig;` compiles and shares children that still report the
original as their parent); `JsonObject::SetItem` and `XNode::ReplaceWith` both
corrupt or **lose** data on their exception paths; and **three stack-overflow
shapes** exist that no CCF-019 text mentions, one reachable from **untrusted
JSON**. **No new `SR-AUD-*` identifier was issued**; numbering stays frozen at
**364**.

**Selected contract, both families: the owner detaches what it owns, in its own
destructor** — the contract #1769 already shipped for `LinkedListNode<T>` under
this same cause. `docs/OwnedTreeLifetimeContractPlan.md` (32 sections) records
it, the complete strong/weak ownership graph, the cycle analysis, eight measured
destruction proofs, and every rejected alternative. Cost, measured: **`sizeof`
unchanged for all 11 public types, zero vtable slots, zero allocations, zero
per-access cost, no signature change**; consumers recompile but edit nothing. It
closes **27 of the 29** use-after-free accesses.

**Every representation-changing candidate was rejected with evidence, not
preference.** A **strong parent link** — the only shape that reproduces .NET's
contract exactly — **leaks by construction**: 2 constructed, **0 destroyed**, 272
bytes in 3 allocations, LeakSanitizer-confirmed. A **`weak_ptr` parent link**
would make children of the repository's own **77 automatic-storage containers**
silently report *no parent*, because `weak_from_this()` on a stack object is
expired. The tombstone-cell candidate works but costs +8/+24 bytes and an
allocation per container for an identical observable contract, and is kept as the
recorded fallback. **.NET's exact lifetime contract is unreachable in C++ with
`shared_ptr` ownership**, and the plan proves that rather than implying parity.

**Nine tickets created, every one gated.** #1886 (JsonNode core), #1887
(JsonNode exception path), #1888 (JsonNode value semantics — **source break**),
#1889 (enumerator lifetime — **the only layout change**, 48→56, module graph
91→92), #1890 (XObject core), #1891 (XNode exception path), #1892 (Xml.Linq
borrowed views — **source break**), #1893 (deep-parse/teardown bounds —
**accepted-input change**), all `needs_user`; #1894 (negative fixtures +
sanitizer closure) `blocked`. Six independent approval items are worded verbatim
in the plan's §31; **item 1 covers both core repairs with one approval**.

Baselines carried forward unchanged because nothing was rebuilt: **14,568 tests /
37 executables**, module graph **41/91**, Doxygen **1,941/1,942** (not re-run and
not needed — `Doxyfile` `INPUT = modules README.md` and this batch touched
neither), negative fixtures **9/66**, version seams **2/18**. Audit tally
**unchanged at 57 remediated / 306 confirmed / 364**, two of the 306 now carrying
the `confirmed (design-complete)` qualifier the index header already defines.

Nothing pushed/merged/rebased/tagged; **CNA and mobile-eggbert were not
inspected, searched, built or modified**; **#1773 stays `blocked`**; CCF-017
remains deferred; #1875 and #1880 remain inactive and untouched;
#1854/#1858/#1862/#1863/#1865/#1879/#1884 remain `needs_user` with their decision
records unchanged.

**Ready queue: empty.** Every open ticket is `blocked`, `needs_user`, or
deliberately inactive. The next work is the **#1886/#1890 approval decision**
(plan §31 item 1), not another family. Full detail, build-directory sizes,
three-job parallelism record and probe accounting: `NEXT.md` under "Autonomous
design batch handoff, 2026-07-30 (CCF-019, DESIGN-COMPLETE)".

## Autonomous remediation batch, 2026-07-31 — CCF-019 owner-side detachment (#1886, #1890)

Branch `feature/remediation-ccf019-owner-detachment`, off
`feature/design-ccf019-owned-tree-lifetimes`. The user approved
**`docs/OwnedTreeLifetimeContractPlan.md` §31 item 1 and only item 1**, which
covers **#1886** and **#1890** together. Items 2–6 were not answered, so #1887,
#1888, #1889, #1891, #1892 and #1893 stay `needs_user` and none was started.
CCF-019 is now **PARTIALLY REMEDIATED**: both core repairs shipped, **both
findings stay `confirmed (design-complete)`**, and that must not be reported
otherwise.

**#1886 — `JsonArray`/`JsonObject` detach their children.** Two destructors, two
headers. Each clears the parent link of every child whose link still names *that*
container. The `== this` guard strengthens §13.1's unconditional sketch and is
load-bearing here: `JsonArray copy = *orig;` (J08) and the public
`DetachParent()` (J13) both leave a container holding a child owned by someone
else. No helper was added, so no new name of any linkage entered a public header.

**#1890 — `XContainer`/`XElement` detach their children and attributes.** Same
rule, plus `XElement` clears each owned attribute's **second** borrowed link,
`XAttribute::next_`, which dangles independently of `parent_`. Both links are
written directly (the classes are already mutual `friend`s) because the
destructor must *read* the parent link to decide ownership and `AdoptObject` only
writes; the link is only ever compared, never dereferenced.

**Measured by re-running the #1885 probe unmodified**, through the unmodified
#1885 build script, in the same three builds from one source, one forked process
per case under the same watchdog:

| | #1885 baseline | after #1886 | after #1890 |
|---|---|---|---|
| ASan `heap-use-after-free` **cases** | 29 | 22 | **3** |
| Faulting **accesses**, recoverable ASan | 57 | 49 | **5** |
| `pure virtual method called` process aborts | 8 | 8 | **0** |

**26 of 29 use-after-free cases closed.** §1 of the design record estimated 27;
the measured figure is **26**, recorded rather than rounded (§34.4) — §13.5 and
§14.3 always listed three cases as outside the core repair, not two. The three
that remain are **J11** (stale `JsonArray` iterator → #1889), **X15**
(`Extensions::Ancestors`' raw `XElement*` → #1892) and **X17**
(`getAttributesProperty()`'s reference → #1892); none goes through `parent_`, so
no parent-link repair could have reached them.

**Cost, measured on both sides:** `sizeof` unchanged for all **11** public types
(24/48/48/40 and 16/16/40/128/120/48/56, `static_assert`ed in the permanent
suites); GCC's own `-fdump-lang-class` class and vtable dumps **identical** for
all eleven, pre-fix headers versus current; **zero** allocations added to
construction, access or destruction (counting `operator new`, 100,000 parent/root
accesses allocate nothing on either side); LeakSanitizer clean; module graph
still **41/91**. ABI: the JsonNode side is symbol-identical (219 external defined
symbols before and after); the Xml.Linq side **gains three weak COMDAT symbols**
(`XContainerD0/D1/D2Ev`) because GCC previously inlined that implicit destructor
and emitted no standalone definition — **no symbol name was removed**.

**Recorded against itself:** two things this batch found and did not paper over.
(i) The probe's J02 and J16 bodies dereference the returned parent without a null
check, so post-fix they fault *in probe code*; the library-side use-after-free is
gone and the permanent suite asserts the defined answer for the same shapes.
(ii) The ownership guard is **not** mutation-detectable on the Xml.Linq side —
insertion there erases from the previous owner and `XObject` deletes copy/move,
so no container can hold a foreign object; removing both guards fails zero tests.
They are retained because item 1 specifies them and #1891/#1892 may change
insertion.

**Baselines:** tests **14,568 → 14,635** across 37 executables (+32
`JsonNodeLifetimeTests`, +35 `XLinqLifetimeTests`; all 53 existing
`JsonNodeTests` and 92 existing Xml.Linq cases pass unmodified). Mutation-checked
five ways in total. Under `build-asan`: 179/179 Text.Json and 127/127 Xml.Linq,
zero ASan/UBSan/LSan diagnostics, with sanitizer activation proved by a
controlled self-test. Doxygen **1,941/1,942**, negative fixtures **9/66**,
version seams **2/18**, module graph **41/91**, audit tally **unchanged at 57
remediated / 306 confirmed / 364** — no new `SR-AUD-*` identifier, numbering
frozen at **364**.

Nothing pushed/merged/rebased/tagged; **CNA and mobile-eggbert were not
inspected, searched, built or modified**; **#1773 stays `blocked`**; #1875 and
#1880 remain inactive; #1854/#1858/#1862/#1863/#1865/#1879/#1884 remain
`needs_user`. Maximum aggregate compilation parallelism was **three jobs**.

**Ready queue: empty again.** The next work is the **§31 items 2–6 approval
decision**, in the order #1887/#1891 (item 2, no source break), #1888 (item 3),
#1892 (item 5), #1889 (item 4), #1893 (item 6), then #1894. Full detail,
build-directory sizes and probe accounting: `NEXT.md` under "Autonomous
remediation batch handoff, 2026-07-31 (CCF-019, PARTIALLY REMEDIATED)".

## Autonomous remediation batch, 2026-07-31 — CCF-019 exception paths (#1887, #1891) and four design records (#1888, #1889, #1892, #1893)

Branch `feature/remediation-ccf019-residuals`, off
`feature/remediation-ccf019-owner-detachment`. Four ticket commits plus the
handoff. **Nothing was pushed, merged, rebased, tagged or published.**

### What was authorised

The user's batch instruction of 2026-07-31 directed
`docs/OwnedTreeLifetimeContractPlan.md` **§31 item 2** to be started (#1887 then
#1891) and set an approval boundary granting nothing for public source breaks,
virtual-interface or vtable changes, object- or iterator-layout changes, return
calling-convention changes, mandatory downstream migration, or broad observable
semantic changes. Item 2 requires none of those. **Items 3, 4, 5 and 6 remain
unanswered**, so #1888, #1889, #1892 and #1893 stay `needs_user` and #1894 stays
`blocked`; none of them was started and nothing they own was absorbed.

### Tickets completed (two implementations, four design records)

| # | Finding | Outcome |
|---|---|---|
| **#1887** | SR-AUD-327, probe **J10** | `JsonObject::SetItem` adopts the incoming value **before** detaching the value it replaces, matching `JsonArray::SetItem`. A rejected call no longer leaves the object holding a value that reports no parent, and a second container now refuses it. |
| **#1891** | SR-AUD-333, probe **X20** | `XNode::ReplaceWith` restores the replaced node when a replacement is refused; `XContainer::InsertNodeAt` adopts after it inserts. `<a>victim</a>` stays `<a>victim</a>` instead of becoming `<a/>`. |
| #1888 | §37 | compatibility review — **no compatible repair exists**; stays `needs_user` |
| #1892 | §38 | compatibility review — **§31 item 5 is not implementable as worded**; stays `needs_user` |
| #1889 | §39 | full sixteen-item design package — **item 4 is also a source break and a silent ABI break**; stays `needs_user` |
| #1893 | §40 | root-cause classification — **three distinct defects**, and the depth bound item 6 asks for already exists in the module; stays `needs_user` |

### Measured result

| | #1885 baseline | after #1886/#1890 | after #1887/#1891 |
|---|---|---|---|
| ASan `heap-use-after-free` cases | 29 | 3 | **3** (J11, X15, X17) |
| Faulting accesses (recoverable ASan) | 57 | 5 | **5** |
| `pure virtual method called` aborts | 8 | 0 | **0** |
| ASan `stack-overflow`s | 3 | 3 | **3** |
| Timeouts | 2 | 2 | **2** |
| **Silent data-loss paths** | **2** | **2** | **0** |
| Leaks | 0 | 0 | **0** |

The pre-change replay reproduced the previous batch's recorded end state exactly
(**0 of 58 cases changed**) before either edit, so the baseline was verified
rather than inherited. After both edits, diffing every answer line of the
no-sanitizer probe build across all 58 cases yields exactly **two** semantic
differences in the whole matrix — J10 and X20.

**+48 permanent tests**; repository gate **14,635 → 14,683 across 37
executables**, zero warnings, zero errors. Six mutation checks (5 / 12 / 12 / 3,
plus one recorded honestly at **0**). ASan+UBSan+LSan clean over 201/201
Text.Json and 153/153 Xml.Linq. Module graph **41/91**, Doxygen **1,941/1,942**,
seams **2/18**, negative fixtures **9/66** — all unchanged. No signature, member,
`sizeof`, `alignof` or vtable change; the only ABI movement is two weak COMDAT
standard-library instantiations newly emitted in `XNode.o`, no name removed.

### Five premises corrected by measurement

Appended to the design record rather than rewritten into it: §35.4 (.NET's
`JsonObject.SetItem` does **not** assign before detaching), §36.4 (validating
replacements before removing the node regresses document-root replacement),
§38.1 (`Ancestors` **cannot** return owning handles without a layout change),
§39.1 (#1889 is a source break and a silent ABI break, not only `+8`), and
§40.2/§40.3 (`JsonDocumentOptions::DefaultMaxDepth = 64` already exists and is
already applied by `JsonDocument::Parse`; item 6's J19d/X27d half needs a layout
change it says it does not).

**SR-AUD-327 and SR-AUD-333 both stay `confirmed (design-complete)`**; the
post-audit tally is unchanged at **57 remediated / 306 confirmed / 364** and
numbering stays frozen at **364**. **CNA and mobile-eggbert were not inspected,
searched, built or modified**; **#1773 stays `blocked`**. Maximum aggregate
compilation parallelism was **three jobs**.

## Autonomous remediation batch, 2026-07-31 — CCF-019 compatible closure (#1895, #1898) and the CCF-009 plan (#1900–#1903)

Branch `feature/remediation-ccf019-final-compatible`, off
`feature/remediation-ccf019-residuals`. Four commits plus the handoff. **Nothing
pushed, merged, rebased, tagged or published.**

### The user's decisions on §31 items 3–6, and what each produced

| Item | Decision | Result |
|---|---|---|
| **3** (#1888) | **declined** — no layout/ABI change, record that no compatible implementation exists | `needs_user` → **blocked**, design preserved (§37) |
| **4** (#1889) | **declined** — do not change the public iterator contract; preserve §39 for a coordinated ABI-breaking release | `needs_user` → **blocked**, §39 package intact |
| **5** (#1892) | **wording rejected as non-implementable** — produce a precise replacement | #1892 retired; **#1898 done**, **#1899 blocked** on one question (§42) |
| **6** (#1893) | **split**; compatible teardown half **approved** | #1893 retired; **#1895 done**, **#1896** and **#1897 blocked** (§40, §41) |

### What landed

- **#1895** — `~JsonArray`, `~JsonObject` and `~XContainer` release owned trees
  **iteratively**: the outermost destructor publishes a worklist, and every
  container destructor running while one is published donates its children and
  returns. Probe **J19c** and **X27c** go from ASan `stack-overflow` to `clean`;
  exactly 2 of 58 cases changed. +34 tests; two mutations detect it 4/17 each,
  all as `SIGSEGV`. All six approval conditions met and measured.
- **#1898** — the Xml.Linq **borrowed-view contract**, stated as preconditions,
  postconditions, invalidation and failure behaviour and pinned by 14 tests,
  including `Attributes()` as the owning alternative. Doc-comments and tests
  only.
- **#1900** — `docs/SharedPrngConcurrencyPlan.md`, the next family (**CCF-009**),
  with #1901/#1902/#1903 created in dependency order.

### Measured

Repository gate **14,683 → 14,731 tests across 37 executables**, 0 warnings,
0 errors. Module graph **41/91**, Doxygen **1,941/1,942**, seams **2/18**,
negative fixtures **9/66** — all unchanged. ASan+UBSan+LSan clean over 218/218
Text.Json and 184/184 Xml.Linq. ABI: `JsonNode.o` keeps all 225 symbol names
(six destructors weak → strong), `XContainer.o` 49 → 62 with **no name removed**.
The one cost is recorded rather than glossed: destroying a container with at
least one child now performs **exactly one** heap allocation where it performed
none; an empty container still allocates nothing.

**CCF-019 is compatible-remediation-complete and not implemented.** §43
reconciles all six items; the residue is J11/J12 (#1889, declined), J08/J09/J13
(#1888, declined), X15/X17 (#1899, one question), J19d/X27d (#1896, declined) and
X28c (#1897, one question). **SR-AUD-327 and SR-AUD-333 stay `confirmed
(design-complete)`**; the tally is unchanged at **57 remediated / 306 confirmed /
364** and numbering stays frozen at **364**. **CNA and mobile-eggbert were not
inspected**; **#1773 stays `blocked`**. Maximum aggregate compilation parallelism
was **three jobs**.

## Autonomous remediation batch, 2026-07-31 — CCF-010 default comparison contract (#1904–#1912)

Branch `feature/remediation-ccf010-comparison-contract`, off
`feature/remediation-ccf019-final-compatible`. **CCF-010 is CLOSED and
SR-AUD-046 is `remediated`** — the second post-audit family finished outright,
after CCF-009. Nothing in it needed user approval and nothing is left blocked.

**Family selection.** With CCF-009 complete, only three families had unplanned,
compatible, unblocked work: CCF-001 (tracked-CI matrix gap), CCF-010 and
CCF-015 (UTF-8 whitespace). CCF-010 was selected because it is the only one of
the three containing undefined behaviour. The full candidate review — every
family's member statuses re-derived from `audit/AUDIT_FINDINGS_INDEX.md` rather
than from the cross-cutting file's prose — is §0 of
`docs/ComparisonContractPlan.md`.

**What the family is.** Six header-only `Core.Base` files used a raw C++ `<`,
`>` or `==` where .NET uses `Comparer<T>.Default` / `EqualityComparer<T>.Default`.
For every type in the port except `float` and `double` those agree exactly,
which is why the defect was invisible for the whole life of the files; for those
two they differ in NaN's order and in NaN's equality with itself.

**The severity the record understated.** `Array::Sort`, `MemoryExtensions::Sort`
and `Linq::OrderBy` did not merely place NaN wrongly: raw `<` over a NaN-bearing
range is not a strict weak ordering, so `std::sort`'s `[alg.sort]` precondition
was violated and the **finite** elements came out unsorted — **64 of 196**
measured shapes corrupted, worst case **3,874 inversions** in 65,536 elements.
**ASan, UBSan and `_GLIBCXX_DEBUG` were all silent**, before and after; the
permanent suite is the only possible gate, which is why the family carries ten
mutations including a negative control.

| Ticket | Scope |
|---|---|
| #1904 | design-only plan, `docs/ComparisonContractPlan.md`, 36-case probe |
| #1905 | `System/detail/ComparisonPolicy.hpp` + `Array.hpp` (10 entries) |
| #1906 | `MemoryExtensions.hpp` (5 entries; its file-local equality rule now forwards to the shared policy) |
| #1908 | `Tuple.hpp` + `ValueTuple.hpp` (41 bodies, incl. the hash invariant) |
| #1907 | `Nullable.hpp` (4 entries; `operator==` deliberately unchanged) |
| #1909 | `Linq.hpp` (6 entries, incl. the asymmetric `Min`/`Max` NaN rules) |
| #1910 | reconciliation and closure |
| #1911 | CCF-003 and CCF-006 closure notes (record defect found during §0's review) |
| #1912 | the `Collections` population, opened and **not** begun |

**Baselines after the batch:** 14,815 tests across 37 executables (was 14,745);
post-audit tally **59 remediated / 304 confirmed / 364**, numbering frozen at
**364**; module graph **41 / 91** unchanged; canonical Doxygen **1,941** of the
1,942 ceiling, unchanged; negative fixtures **9 / 66**; version seams **2 / 18**;
`nm --extern-only` on `libsharp_runtime_core.a` **identical** before and after
(6,168 symbols); `sizeof`/`alignof` identical for all eleven measured
instantiations; `scripts/check_selective_components.sh` **run and passed**,
because the family adds a public header; `scripts/local_ci_check.sh build`
passed; `git diff --check` clean. Measured cost on 1,000,000 elements: 1.011×
`float` without NaN, 0.994× with 1% NaN, 1.009× `int`.

---

## Batch 2026-08-03 — `System::Threading` #1951–#1955, the #1956–#1959 approval package, and the Tasks/Channels review (#1964)

Branch `feature/remediation-batch-threading-1951-1955`, cut from the previous batch's tip.
Seven local unsigned commits. This is the direct continuation of the `System::Threading`
namespace review (#1950): it implemented the whole **compatible** half of that review's queue
and left the approval-gated half with a design that is ready to be approved rather than
re-derived.

| Ticket | Cause | Findings closed | Tests |
|---|---|---|---|
| #1951 | T-B — CCF-011 in a third module | SR-AUD-190/192/198/217/222 + the 213/219 callable halves | +24 |
| #1952 | T-C | SR-AUD-183 | +7 |
| #1953 | T-C | SR-AUD-188, SR-AUD-199 | +9 |
| #1954 | T-C | SR-AUD-184, SR-AUD-205, SR-AUD-213 (completing it) | +9 |
| #1955 | T-A | SR-AUD-207 ×3, 212, 216, 218 + the 203 race half | +11 |
| #1964 | review | — (opens #1965–#1970) | — |

**What made this batch different from a routine validation sweep** is that seven of the
audit's own statements turned out to be wrong in a way that changed the repair, and one
requested repair was declined outright:

- Two of #1951's seven sites are **not** `ArgumentNullException` sites, because .NET has no
  check there at all and faults with `NullReferenceException`; applying the family's usual
  spelling would have left both findings' measured divergence open.
- SR-AUD-183 has **three** non-terminating shapes, not one, and its `-2` timeout gives two
  different wrong answers depending on collection size.
- SR-AUD-199 has a second route the finding never names — a **moved-from** `CancellationToken`
  — and that route is what decided the repair (tolerance, not rejection).
- SR-AUD-219's stated consequence is wrong: the failure was a **silent default value**, not a
  deferred `bad_function_call`.
- SR-AUD-205's defect was confined to the reported property; the lock already behaved
  correctly.
- SR-AUD-216 had **two** racing reads.
- The #1955 TSan probe's first version was itself unsound — it reported zero races for two
  types that were equally racy, because its writer loop finished before its reader started.
- **SR-AUD-200 was deliberately not repaired** (new ticket #1963): its report carries no
  managed probe, .NET appears to truncate a fractional `PeriodicTimer` period exactly as this
  port does, and the reference tree is absent from this environment, so rejecting it would
  have been a narrowing away from .NET on a recollection.

**Baselines after the batch:** 15,165 tests across 37 executables (was 15,105), 15,158
passing, 1 skipped, 6 failing for two re-measured environmental causes; post-audit tally
**87 remediated / 277 confirmed / 364**, numbering frozen at **364**; module graph **41 / 91**
unchanged; negative fixtures **10 / 81**; version seams **2 / 18**; checker self-tests
**45 / 45** and **15 / 15**; `sizeof`/`alignof` **identical** for all six types #1955 touched;
`scripts/check_selective_components.sh` run and **passed**; `scripts/local_ci_check.sh build`
passed every static gate; `git diff --check` clean. **Doxygen was not run — it is not
installed in this environment**, so the 1,942 ceiling is retained as historical rather than
newly verified. Maximum aggregate compilation parallelism: **two jobs**, never exceeded, with
no two builds running concurrently.


## Batch 2026-08-03 — `Threading.Tasks` + `Threading.Channels` #1965–#1968, and the #1958 Group A split (#1971)

Branch `feature/remediation-batch-tasks-channels-1965-1968`, cut from `a0cd647`. Five ticket
commits plus a handoff, all unsigned; pushed to
`origin/claude/remediation-batch-1804-namespace-b1yjh5` at the user's explicit mid-batch
request, with no merge, rebase, tag or PR and no previous commit rewritten.

| Ticket | Cause | Findings closed | Tests |
|---|---|---|---|
| #1965 | TC-A — CCF-011 in a *third* module | SR-AUD-231 | +37 |
| #1966 | TC-B/1 | SR-AUD-232 | +10 |
| #1967 | TC-C | SR-AUD-234 (two sites) | +13 |
| #1968 | TC-B/2 | SR-AUD-233 | +12 |
| #1971 | T-H — the verified-compatible half of #1958 Group A | SR-AUD-214, SR-AUD-189 | +16 |

**What made this batch worth reading rather than a routine sweep** is that eight of the audit's
own statements were wrong in ways that changed the repair, and one recommended split turned out
to be over-broad:

- **SR-AUD-231 has 22 public entries, not two**, and `Parallel`'s empty callable was **already
  catchable** — it arrived as `System::AggregateException`, so CCF-011's strongest consequence
  ("the failure is the wrong type") never applied to that file.
- **`Parallel::Invoke` is not an `ArgumentNullException` site**: .NET rejects a null *element*
  with a plain `ArgumentException` carrying no parameter name. The same trap
  `ThreadingNamespaceReviewPlan` §17.1 recorded, in a third module.
- **SR-AUD-232's repair cannot be placed where .NET places it** — .NET validates in the
  `ParallelOptions` setter, and this port's field is a public data member — and its check had to
  be inserted *above* #1965's, confirmed by measurement on the intermediate tree.
- **SR-AUD-234 has a second site the finding explicitly denies.** `ChannelWriter::WriteAsync`
  loses the same boundary as `ReadAsync`; repaired together, no new identifier.
- **SR-AUD-233 collided with a contradictory claim inside this repository.** `Channel.hpp`'s own
  comment asserted the opposite of the finding, citing a reading of `BoundedChannel.cs`.
  Resolved in favour of the finding's *behavioural managed probe*, the comment replaced rather
  than dropped, and the limitation stated: the reference tree is absent here. This is the mirror
  image of **#1963**, declined precisely because *its* report carries no managed probe — the
  same rule, opposite answer.
- **#1958 Group A is two members, not three.** SR-AUD-215 was verified **not** compatible: no
  consumer can obtain a non-null `ExecutionContext*` at all, so rejecting null would make `Run`
  throw for every call that can be written. It stays with the blocked #1958.

**Baselines after the batch:** 15,253 tests across 37 executables (was 15,165), 15,246 passing,
1 skipped, 6 failing for two re-measured environmental causes; audit **93 remediated / 271
confirmed / 364**, numbering frozen at **364**; module graph **41 / 91** unchanged; negative
fixtures **10 / 81**; version seams **2 / 18**; checker self-tests **45 / 45** and **15 / 15**;
`check_selective_components.sh` run and **passed**; `local_ci_check.sh build` passed every
static gate and stopped at the same known Ping failures; `git diff --check` clean. **Doxygen was
not run — it is not installed in this environment.**

**Sanitizers, with every probe's capability proved first** (§19.4's rule): TSan reported **0 data
races before and after** everywhere — correct, since every defect repaired was a *contract*
defect — and the pre-fix runs are what make that zero informative: 3,600 wrong outcomes → 0
(#1967), 600 → 0 (#1968), 8,000 → 0 (#1971). ASan + UBSan + LSan: 0 reports throughout. Two
honest non-discriminators are recorded rather than dressed up.

**Consequences:** no public signature, vtable, `noexcept`, exception-contract or component-edge
change anywhere. One internal type grew — `detail::ChannelState<int>` 240 → 248 bytes for the
rendezvous waiting-peer counter — while every public channel type is unchanged and now pinned by
`static_assert`. One behaviour-incompatible change by design: a zero-capacity bounded channel is
now a rendezvous, and the two tests that pinned the old behaviour were **rewritten, not
deleted**, identified as such in the file and the commit. Maximum aggregate compilation
parallelism: **two jobs**, never exceeded, with no two compilations running concurrently.

**Remaining queue:** the only `todo` is **#1963**, which needs the absent .NET reference tree.
Six approvals are outstanding — #1956, #1957, #1958 (six members now), #1959, #1969, #1970. The
recommended next unit is another namespace review, `System.Runtime` or `System.Uri`; if any
approval lands first, **#1969** is the cheapest remaining win.


## Batch 2026-08-03 — the `System::Runtime` namespace review (#1972) and its whole compatible half (#1973–#1978, #1982)

Branch `feature/remediation-batch-system-runtime-review`, cut from `66ff8b3`. Nine local
unsigned commits, no push. This is the third namespace review in the #1950/#1964 series.

| Ticket | Cause | Finding closed | Tests |
|---|---|---|---|
| #1972 | review | — (opens #1973–#1986) | — |
| #1973 | R-E — the CCF-011 policy in a **fourth** module | SR-AUD-155 | +10 |
| #1974 | R-B | SR-AUD-172 | +2 |
| #1975 | R-A | SR-AUD-169 (save/restore half) | +5 |
| #1976 | R-F | SR-AUD-156 | +8 |
| #1977 | R-D | SR-AUD-170 | +5 |
| #1978 | R-J | SR-AUD-059 + SR-AUD-168 disclosure | +2 |
| #1982 | R-I | SR-AUD-162 (documented deviation) | +3 |

**What made this batch worth reading rather than a routine sweep** is that five of the
audit's own statements were wrong in ways that changed the repair, one of them was this
review's *own probe*, and one requested repair was deliberately withheld:

- **SR-AUD-155 has four undefined-behaviour routes, not the two it names.** The implicitly
  declared move constructor and move assignment leave a moved-from `ExceptionDispatchInfo`
  empty, so `Throw()` faults through ordinary well-formed C++ that passes no null anywhere.
  A check placed only where the finding asks leaves both open. Third occurrence of this
  shape after SR-AUD-199, and the fourth site at which the CCF-011 policy needed a
  non-default exception type for a non-default API shape.
- **SR-AUD-169's consequence is sharper than recorded.** `SIG_IGN` on `SIGHUP` became
  `SIG_DFL`, and SIGHUP's default *terminates* — the audit's SIGWINCH probe cannot show
  this, because SIGWINCH's default is ignore.
- **SR-AUD-170 rejects the positive spelling of *supported* signals too**, so the enum was
  accepted in exactly one of its two valid spellings; the repair had to make them agree.
- **SR-AUD-162's premise does not survive translation** — the managed constraint has a CLR
  cause with no C++ counterpart — so the disposition is a documented widening, not a
  narrowing.
- **This review's first probe produced two confident false negatives.** It forked *after*
  the parent had registered, so the children inherited `watcherRunning_ == true` with no
  watcher thread; nothing drained the pipe and both SR-AUD-171 and SR-AUD-172 "did not
  reproduce". Reordered, both reproduce. Every forked case now prints a liveness marker.
- **SR-AUD-171 was deliberately not repaired** (#1979): the port's behaviour is reproduced
  here but .NET's is not, because the audit's reference basis is a *reading* of
  `pal_signal.c` taken when the reference tree was present and carries **no managed probe**.
  That is the same line the repository drew between #1968 (probe present, repair landed) and
  #1963 (none, nothing changed).

**A wording correction carried forward:** the previous handoff called #1967 *"no
exception-contract change"*. #1967 changed **no** signature, `noexcept` or ABI, but it
**did** deliberately change the exception type crossing an API boundary — that was its
point. Recorded as `docs/ThreadingTasksChannelsReviewPlan.md` §19. **#1967 is not reopened**;
its implementation and tests are complete.

**Baselines after the batch:** 15,288 tests across 37 executables (was 15,253), 15,281
passing, 1 skipped, 6 failing for two re-measured causes; audit **100 remediated / 264
confirmed / 364**, numbering frozen at **364**; module graph **41 / 91** unchanged; negative
fixtures **10 / 81**; version seams **2 / 18**; checker self-tests **45 / 45** and
**15 / 15**; `check_selective_components.sh` run and **passed**; `local_ci_check.sh build`
passed every static gate with a clean build and stopped at the same known Ping failures;
`git diff --check` clean. **Doxygen was not run — it is not installed in this environment.**

**Sanitizers, every harness's capability proved first:** ASan + UBSan + LSan **0 reports**
across all five instrumented tickets, with `PosixSignalRegistration.cpp` compiled **from
source** rather than linked from the uninstrumented archive; TSan **0 data races** over
100,000 handler/watcher interleavings. Two control defects confirmed the harnesses report (2
and 1 findings respectively). One honest non-discriminator recorded: TSan cannot see #1974's
defect at all, because a hang is not a data race.

**Consequences:** no public signature, object layout, vtable, `noexcept`, mangled-symbol or
component-edge change anywhere. One change makes a currently succeeding call fail —
`GCSettings`' setters now reject out-of-domain input, which .NET also does — and it was
stated in the plan's §9 before it was made. One strict widening (raw signal numbers). Two
documentation repairs with zero runtime effect. Maximum aggregate compilation parallelism:
**two jobs**, never exceeded, with no two compilations running concurrently.

**Remaining queue:** three `todo` — #1963 (needs the absent reference tree), #1985 and #1986
(both recorded inactive this batch). Nine approvals are outstanding: #1956, #1957, #1958,
#1959, #1969, #1970 carried forward, plus the new #1979, #1980, #1981. The recommended next
unit is the **`System::Uri` namespace review** (14 findings, all medium, no plan yet); if any
approval lands first, **#1969** is the cheapest win overall and **#1980 G-1** the cheapest
inside `System::Runtime`.

## Batch 2026-08-04 — the `System::Diagnostics` compatible half (#2024–#2028) and the recovered selective-components gate

Branch `feature/remediation-batch-diagnostics-2024-2028`, cut from the clean tip `9319580`.
All commits local and unsigned; nothing pushed, merged, rebased, tagged or amended.

**Record-keeping note, stated rather than backfilled.** `plan.md` has no section for the
`System::Uri` batch (#1987–#2005), the `System::Text` batch (#2006–#2012) or the
`System::Text` approvals + `System::Diagnostics` review batch (#2022–#2023); its last entry
before this one is the `System::Runtime` batch. Those sections were not written by the
sessions that did the work. They are **not** invented here — `NEXT.md` and the per-namespace
plans hold those records — but the gap is recorded so it is not mistaken for the work not
having happened.

> **Correction, appended 2026-08-04 by the #2033/#2034 batch.** That note is wrong about one
> of its three items: the `#2022`/`#2023` batch **did** write its section — commit `9319580`
> added 98 lines to `plan.md`, and the section *"2026-08-03 — `System::Text` approval package
> verified (#2022) and the `System::Diagnostics` review (#2023)"* is in this file. The genuine
> gap is **three** batches, not four: the `System::Uri` review (#1987–#1994), the `System::Uri`
> follow-up (#2000–#2005) and the `System::Text` review (#2006–#2012). All three are recorded in
> this file's *"Last verified"* header stack, so `plan.md` was never silent about them — it
> lacked only a dated section. Those three sections are now backfilled at the end of this file
> from committed evidence.

**Work unit 0 — the selective-components check the previous session could not finish.**
Preflight confirmed no `cmake`, `ninja`, `make`, `cc1`, `cc1plus`, `g++`, `clang++` or
`check_selective_components.sh` process existed. One instance was then run, verified
repeatedly by `ps` at exactly **two** `cc1plus` processes and one
`cmake --build … --parallel 2`. It **passed, exit 0, in 16 min 40 s** — all ten components,
all three negative fixtures rejected, and every `Text.Json` isolation assertion held. The
previous session's report of "~35 minutes on `Core.Base` alone" is consistent with that run
having been contending with its own accidental duplicate; here `Core.Base` took ~6 minutes.
**The previous batch's validation gap is closed.**

**Work unit 1 — #2025 → #2026 → #2024 → #2027 → #2028**, the order
`docs/SystemDiagnosticsNamespaceReviewPlan.md` §13 sets.

| Ticket | Finding | What changed |
|---|---|---|
| #2025 | SR-AUD-270 | restarting a `Process` no longer aborts the whole process |
| #2026 | SR-AUD-274 | the forked child performs no allocating call before `exec` |
| #2024 | SR-AUD-268, 272 | `WaitForExit` honours `Timeout.Infinite`, rejects `< -1`, and retries on `EINTR` |
| #2027 | SR-AUD-275 | `Debug`'s provider and both indent-size globals are race-free |
| #2028 | — | the three headers' contracts made true, and #2029/#2030/#2031 **pinned** |

**Six premises corrected by measurement, none of them cosmetic.** `Process`'s default
constructor is **private**, so every public call to the instance `Start()` is a restart by
construction. SR-AUD-270's trigger is a **joinable reader thread**, not a running child, so a
restart after the previous child had already exited aborted too — the plan's own "must work"
row was already broken. Captured text **accumulated** across restarts. An **unredirected**
restart while the child ran silently **abandoned** it unreaped, a path §7.1's one-line
justification did not cover, so #2025 gets its own observable-change table at §7.4.
SR-AUD-275's before-state included **heap-use-after-free**, not merely the data race it
described. And UBSan for #2024 is **non-discriminating** — clean before *and* after — which is
recorded as such rather than presented as evidence.

**One new defect, filed rather than absorbed: #2032.** `WaitForExit(milliseconds)` blocks past
its own deadline when a grandchild holds the redirected pipe — measured **29,951 ms against a
5,000 ms bound**, because `/bin/sh` is `dash`, which forks rather than execs. Every repair
decides the reader-thread policy that **#2029 gates**, so absorbing it would have pre-empted an
approval. No `SR-AUD-*` identifier issued; numbering stays frozen at **364**.

**#2029, #2030 and #2031 remain blocked and are now PINNED**, which #2022 made a mandatory
acceptance criterion rather than an optional courtesy. Every pin was shown to be
discriminating: the two #2029 pins by temporarily applying §14.1's recommended option C to
`~Impl` (both failed with their intended messages; reverted, `git diff` clean), #2030 by a
`static_assert` that makes the §14.2 return-type change a **compile error**, and #2031 by a
control grandchild that does *not* call `setsid` and **is** killed by the `killpg`.
**No approval was requested, implied or assumed.**

**Numbers.** Audit **112 remediated / 252 confirmed / 364 total** (was 107 / 257), with
`confirmed (design-complete)` unchanged at **38**. `SharpRuntimeTests_Diagnostics` **159 → 219
(+60)**. Whole gate **15,529 tests across 37 executables run individually, 15,522 passed, 1
skipped, 6 failed** — the same two measured causes, unchanged: five `PingTests` (#1962;
`ping_group_range` is the empty range `1 0`, so `SOCK_DGRAM` ICMP gives `EACCES` while
`SOCK_RAW` ICMP succeeds — exactly the missing fallback) and one `SocketTests` (no
`/proc/net/if_inet6`). Graph **41 / 91 unchanged**, seams **2 / 18**, negative fixtures
**10 / 81**, build **0 warnings / 0 errors**. **Doxygen was not run — it is not installed.**

**Consequences.** No public signature, object layout, vtable, `noexcept`, mangled-symbol or
component-edge change. `Process` stays a pimpl; `Debug` and `Trace` have no data members.
Three narrowed rows, each tabulated before being made: `WaitForExit` below `-1` now throws,
`WaitForExit(-1)` now blocks instead of returning a meaningless `false`, and a restart while
the previous child runs is refused. Maximum aggregate compilation parallelism **two jobs**,
never exceeded, with no two compilations ever running concurrently.

Full evidence, sanitizer accounting and probe inventory: `NEXT.md` under
"Autonomous batch handoff, 2026-08-04".

## Batch 2026-08-03 — the `System::Uri` namespace review (#1987) and its compatible half (#1988–#1994)

**Backfilled 2026-08-04** from committed evidence — commits `2ba68fa` … `501d866`,
`docs/SystemUriNamespaceReviewPlan.md` and this file's own *"Last verified"* header stack. The
session that did the work wrote a header paragraph but no dated section. Nothing here is new.

The fourth namespace review. Fourteen open `modules/uri` findings — **all medium, no high** —
mapped to causes U-A … U-J and tickets #1987–#1999. The namespace was chosen over `System::Runtime`
on count, having no `docs/` plan of its own.

Implemented, all compatible: **#1988** recognises the scheme by grammar instead of searching for
`"://"`; **#1989** consults the default-port table on every path that yields a port; **#1990**
resolves query-only, fragment-only and network-path references per RFC 3986; **#1991**/**#1992**
close both halves of SR-AUD-145 (IP-literal brackets, `UriKind` domain); **#1993** splits copied
user-info into `UserName` and `Password`; **#1994** makes two headers' contracts true.
`SharpRuntimeTests_Uri` **213 → 272**.

**Five causes remain approval-gated** — **#1995** (identity as rendered text), **#1996** (setter
narrowing plus rendering changes), **#1997** (four absent public shapes), **#1998**
(`IsKnownScheme` narrowing), **#1999** (`UriTypeConverter`'s virtual signature) — each with a
complete design and an exact approval sentence in the plan's §14, each behaviour-pinned. **None was
implemented, and no approval was requested or assumed.**

## Batch 2026-08-03 — the `System::Uri` post-audit follow-up queue (#2000–#2005)

**Backfilled 2026-08-04** from commits `4280903`, `942f27a`, `bef4e43` and the header stack.

The six post-audit defects #1987's review filed under ordinary ticket numbers, **no `SR-AUD-*`
issued**. Implemented: **#2000** rejects an empty authority wherever a port applies, while
`file:///path` stays accepted; **#2001** resolves against an opaque base per RFC 3986 §5.2.2/§5.3
instead of fabricating an authority; **#2002** splits a relative reference's query and fragment as
the other two branches always did; **#2004** hashes `UriBuilder`'s rendered string instead of
re-parsing it, removing four routes on which `Equals(self)` succeeded but `GetHashCode` threw, with
**no returned value changed** and no part of #1995's gated identity policy introduced.

**#2003** (an embedded NUL crossing the parser into every component) is design-complete and
**blocked** on approval to narrow, its preserve-as-data behaviour pinned. **#2005** (surrounding
whitespace) stays **deferred**, its missing reference evidence re-verified absent. The audit was
unchanged at 104 remediated / 260 confirmed / 364.

## Batch 2026-08-03 — the `System::Text` namespace review (#2006) and its compatible half (#2007–#2012)

**Backfilled 2026-08-04** from commits `67cbfa7` … `4f459de`,
`docs/SystemTextNamespaceReviewPlan.md` and the header stack.

The fifth namespace review. Fourteen open `modules/text` findings (SR-AUD-286 … SR-AUD-299) mapped
to eleven causes **T-A … T-N** and tickets #2006–#2021. Scope was narrowed by measurement: the C++
namespace spans three CMake components and this review owns **one** (`Text`), while
`NormalizationForm` lives in `modules/core` with no `Normalize` surface at all and
`Encoder`/`Decoder` have no incremental conversion surface — recorded as explicit exclusions rather
than invented as findings.

Implemented, all compatible: **#2007** gives every raw decode entry one argument policy, transcribed
from `UTF7Encoding`; **#2008** makes both fallback setters reject null (the finding names only the
decoder direction); **#2009** stops `StringBuilder::CopyTo`'s capacity overflow **defeating** its
bounds check — an invalid capacity had become a silent write, ASan-confirmed with the call returning
normally; **#2010**/**#2011** close the diagnostics halves of SR-AUD-298 and SR-AUD-297; **#2012**
makes three headers' contracts true. `SharpRuntimeTests_Text` **238 → 288**.

**Seven audit premises were corrected**, historical text preserved, including that **CCF-012's
exclusion list is wrong to say `System.Text.CompositeFormat` "is not ported"**. **Nine causes remain
approval-gated (#2013–#2021)**; none was implemented and no approval was requested or assumed.

## Batch 2026-08-04 — the consolidated approval package, #2033, and the `System::Net` review (#2034)

Branch `feature/remediation-batch-approval-packages-next-review`, cut from the clean tip `1b892f6`.
All commits local and unsigned; **nothing pushed, merged, rebased, tagged, amended or force-pushed,
and no PR was created.**

**Work unit 1 — the `System::Diagnostics` approvals, verified rather than restated.** #2032 is
**folded into #2029**: the reader-thread join lives in `reapIfNeeded`, not in the destructor, and is
reached from **five** public doors — `WaitForExit(500)` 7,502 ms, `getHasExitedProperty()` 7,502 ms,
`Kill(false)` 7,502 ms, the restart `Start()` 7,503 ms and `~Process` 8,004 ms against an 8 s
grandchild holding the redirected pipe. §14.1's destructor-only approval sentence would have left
four of them blocking with **no bound at all**. A second, sharper finding: §14.1's recommended
option C is **unsound as written** — `drainPipe` holds a raw pointer into the `Impl`, so detaching
in `~Impl` is a **use-after-free** unless the reader gains shared ownership of its buffer. #2030's
two pins and #2031's one were **independently re-verified discriminating** by temporarily applying
the proposed change (the return-type change is a compile error; §14.3's `/proc` walk did kill the
`setsid` grandchild); every mutation was reverted with `git diff` clean.

**Work unit 2 — the `System::Text` approvals.** Verified and found to stand, with three additions:
#2017 gains **six** alternatives rather than four and its recommendation changes, because the stock
replacement fallback measurably **ignores** its `char` argument and the decoder surface is already
byte-vector-shaped; CCF-012 was re-enumerated from scratch across all 16 brace-scanning files
(**exactly two** composite-format implementations) with grammar rejections separated from
index-out-of-range; and A1+A2 was promoted to an explicit atomicity requirement. **CCF-012 is not
marked closed.**

Both are now requested in one place, `docs/ConsolidatedApprovalPackage.md`, as **three** Diagnostics
decisions and **nine** Text decisions with one grouped checklist. **No approval was requested,
implied or assumed, and no gated work was implemented.**

**The one compatible portion found and implemented — #2033.** The reader-join disclosure half: six
`Process.hpp` contracts made true and `ProcessReaderJoinBlockingPinTests.cpp` added (+6 — four pins,
one control proving they isolate the inherited pipe holder rather than slow calls, one teardown
check), mutation-checked by replacing the join with option C's `detach`, which failed all four while
the control stayed green. Zero executable production change.
`SharpRuntimeTests_Diagnostics` **219 → 225**.

**Work unit 3 — the next namespace, re-derived by measurement.** `System::Net`: **10 open findings
of which 3 are `high`** (30 %, the highest of any un-reviewed namespace with more than six
findings), **no** `docs/` plan, one module and one namespace, a freshly reviewed `Uri` dependency,
and **#1962 is `modules/net-network-information`** so it gates nothing here.
`docs/SystemNetNamespaceReviewPlan.md` (#2034, 16 sections) reproduced **all ten** premises,
including an ASan `heap-buffer-overflow` at the finding's own line. **Four premises were corrected.**
Seven causes **N-A … N-G**, six compatible tickets **#2035–#2039, #2041** and four gated
**#2040, #2042, #2043, #2044**. **Nothing was implemented.**

**Audit:** SR-AUD-305/306/308/309 move to `confirmed (design-complete)`.
**112 remediated / 252 confirmed / 364 total**, design-complete **38 → 42**; the confirmed total is
unchanged because this batch remediated nothing, by design. **No `SR-AUD-*` identifier was created.**

Full evidence, sanitizer accounting and probe inventory: `NEXT.md` under
"Autonomous batch handoff, 2026-08-04 (approval packages + `System::Net` review)".

## Batch 2026-08-04 — the whole compatible `System::Net` queue (#2035–#2039, #2041) and the pins ticket (#2047)

Branch `feature/remediation-batch-system-net-compatible`, cut from the clean tip `4777f95`. Seven
work commits plus this handoff, all `git -c commit.gpgsign=false` because this environment has no
usable signing key. Nothing pushed, merged, rebased, tagged, force-pushed or published; no PR; no
remote reference altered; no history rewritten.

### Execution order

`docs/SystemNetNamespaceReviewPlan.md` §13's order was followed exactly and not reordered:
**#2041 → #2035 → #2036 → #2037 → #2038 → #2039**, then the mandatory disclosure-and-pins ticket
as **#2047**.

| Ticket | Finding | Repair | Tests |
|---|---|---|---|
| **#2041** | SR-AUD-307 | both `CookieCollection` indexers route through one `validatedIndex(intcs)` rejecting `index < 0 \|\| index >= Count` | +14 (incl. layout pins for all five §10 types) |
| **#2035** | SR-AUD-300 | `GetIPEndPoint` validates family and **declared** size against `IPEndPoint::Create`'s own two constants; the `IPAddress`+port constructor validates the port | +17 |
| **#2036** | SR-AUD-301 | one shared `validatedScopeId` on **both** doors, each reporting its own `paramName` | +9 |
| **#2037** | SR-AUD-302 | after `']'` may come exactly nothing, or `':'` and the port | +9 |
| **#2038** | SR-AUD-303 | the masked IPv6 base keeps its scope id; `Contains` compares **bytes** so containment stays scope-independent | +8 |
| **#2039** | SR-AUD-304 (3 of 4) | one literal parser, the family filter on the literal path, `SOCK_STREAM` hints + dedup, and messages that stop fabricating a Win32 code | +16 |
| **#2047** | — | three headers made true; **10 pins** for #2040/#2042/#2044, mutation-checked | +10 |

`SharpRuntimeTests_Net` **240 → 324**, entirely add-only.

### Ten premise corrections, all measured

The load-bearing one is #2039's. `docs/SystemNetNamespaceReviewPlan.md` §5's cause N-C says the
duplicate `sscanf` parser "produces duplicates". Measured against `getaddrinfo` **directly**, it
does not: `hints.ai_socktype = 0` asks for one `addrinfo` **per socket type**, so every answer came
back **three** times — `localhost`, `runsc`, `vm` and `127.0.0.1` as well as the finding's
`"1.2.3"` — and `GetHostEntry(8.8.8.8)` returned **six** entries for two distinct addresses.
Replacing the literal parser alone would have made `GetHostAddresses("1.2.3")` return one address
**by accident**, because `IPAddress::TryParse` accepts short forms, while leaving every resolved
**name** tripled. Two further corrections changed the code that shipped: each scope-id door
reports its **own** `paramName` rather than §7.3's shared `"value"`, and a family-mismatched `Dns`
literal **throws** rather than returning §7.3's predicted empty vector, because two pre-existing
`DnsTests` already pin that as this repository's own reasoned contract. The full table is the
plan's new **§18**.

### Two new ordinary inactive tickets, deliberately not absorbed

- **#2045** — `IPEndPoint` accepts a trailing `':'` with no port and reports port 0, in **both**
  branches. Cause N-C's *shape* but not SR-AUD-302's site, and rejecting it would violate #2037's
  own acceptance criterion. Blocked on **evidence**; both results pinned.
- **#2046** — `Dns` applies the address-family filter to a **literal** but not to a resolved
  **name**, so `("localhost", Unix)` still returns an IPv4 address. Narrowing it removes a
  currently-succeeding result for every name.

No `SR-AUD-*` identifier was created for either, nor for any of the ten corrections; audit
numbering stays frozen at **364**.

### The four gated tickets are now all behaviour-pinned

§15's completion criterion 2 is satisfied: **#2043** by #2039's `DnsWildcardPinTests`, and
**#2040**, **#2042** and **#2044** by #2047's `NetGatedBehaviourPinTests`. Six mutations were
applied temporarily and **eight of the ten pins failed**, then everything was reverted with an
empty `git diff`. The two that stayed green are deliberate controls. **None of the four was
implemented, approved or preselected.**

### Sanitizer evidence

| Ticket | Sanitizer | Before | After |
|---|---|---|---|
| #2041 | ASan | **9** reports | **0** |
| #2041 | UBSan | **2** reports — §11 predicted a non-result and was **half wrong** | **0** |
| #2035 | ASan | **2** `heap-buffer-overflow` READs (`:106` IPv4, `:110` IPv6) | **0** |
| #2035 | UBSan | none | none — non-discriminating, recorded as a non-result |
| #2036 | UBSan | none | none — non-discriminating, as §11 predicted |
| #2039 | LSan | — | clean across **12,800** calls; a control build leaking one `addrinfo` list per iteration **is** reported |

### Validation

Gate **15,619 / 15,612 passed / 1 skipped / 6 failed** across all 37 executables run
individually — `15,535 → 15,619`, exactly this batch's +84. The six failures are the same two
measured causes: five `PingTests` (#1962 — `ping_group_range` is `1 0`, so unprivileged
`SOCK_DGRAM` ICMP is denied while `SOCK_RAW` ICMP **succeeds**, and `Ping` has no raw receive
path) and one `SocketTests` IPv6 case (`/proc/net/if_inet6` **absent**). Graph **41 / 91**, seams
**2 / 18**, negative fixtures **10 / 81** (91 invocations, peak 2 jobs, 29.6 s), checker
self-tests **45 / 45** and **15 / 15**, module-boundary self-tests **7 / 7**, catalogue current,
DB consistency OK, build **0 warnings / 0 errors**, `git diff --check` clean, selective components
**PASSED**. Doxygen and `ccache` are **not installed** here and nothing was installed.

## Batch 2026-08-04 — the `System::Buffers` namespace review (#2048) and seven of its eight compatible tickets

Branch `feature/remediation-batch-buffers-review` from `27061bf`; six commits, all unsigned and
local-only, no push. Durable records: `docs/BuffersNamespaceReviewPlan.md` and this file's own
*"Last verified"* header stack.

The **eighth** namespace review. **The selection was re-derived, not inherited**: the index was
re-parsed in full (364 / 117 remediated / 204 confirmed / 43 design-complete, matching the
previous batch exactly) and every un-reviewed unit with ≥ 6 open findings was scored.
`modules/buffers` measured **11 open / 3 high / 27 %** — tied with `modules/io` on count, but
`io` has **zero** high findings, and 27 % is the highest high ratio of any un-reviewed unit with
more than six findings. All three highs are memory safety on a public door. The previous
handoff's nomination was therefore **right**, and is now right for a recorded reason.

**One correction to the selection itself.** The *namespace* has **12** open findings, not 11:
`SR-AUD-088` owns `System::Buffers::MemoryHandle`, which lives in `modules/core` because
`Core.Base` cannot depend on `Buffers`, so a module-path reading of the index hides it. It was
reviewed here and is now design-complete.

### What was implemented

| Commit | Tickets | Repair |
|---|---|---|
| `11bf575` | #2048 | the review; nothing implemented |
| `a620ade` | #2049, #2050, #2051 | validate `ReadOnlySequence`'s raw metadata and `TryGet`'s position; `ArrayBufferWriter` growth in `longcs` → `OutOfMemoryException` |
| `e77ec9d` | — | plan §23 corrections |
| `a926730` | #2052, #2053, #2055 | empty `ToString` for a zero symbol; validated `ArrayPool::Create`; linear `PositionOf` |
| `f9d2a11` | #2061 | disclosure + 23 pins + 6 layout `static_assert`s, **zero executable production change** |
| `4ed2621` | — | audit records, eight status changes |

**No signature, `noexcept`, virtual, vtable, data member or object-layout change anywhere.**
`modules/buffers` is header-only and, measured, no module outside it includes any header this
batch changed except `MemoryHandle.hpp` (doc-comment only), so the blast radius is the module.

### The corrected premises that changed shipped code

**SR-AUD-073 needs no forged position.** `seq.getStartProperty()` held across `seq.Slice(...)`
and passed to the slice's `TryGet` returns a view covering elements the slice does not contain —
an ordinary caller mistake, not the forgery the finding describes. The repair validates the
**range**, which closes that path and the ASan-confirmed negative-position path in one rule.

**SR-AUD-071 is two defects.** The post-dispose getter needs 8 bytes `MemoryPoolHeapOwner_` has
no padding for (32 → 40, a public layout change); the retained view needs a `Memory<T>` ownership
change in `Core.Base`. Only the disclosure was available without approval.

**SR-AUD-072's negative length does not fault** — it escapes a native `std::length_error`, and
UBSan is silent. **SR-AUD-086 is two unverifiable claims**, not one, and the reference tree is
absent, so it was deferred (#2060) rather than implemented — widening an accepted input set on an
unverified premise is what the method exists to prevent.

### Sanitizer evidence

One probe source compiled twice, the `before` include path shadowed by a `git show` tree, so the
only difference between columns is the code under test:

| Mode | Before | After |
|---|---|---|
| `trygetneg` | ASan `heap-buffer-overflow` READ | `ArgumentOutOfRangeException` |
| `trygetslice` | **no exception**, out-of-slice data | `ArgumentOutOfRangeException` |
| `ctornull` | UBSan null load + ASan `SEGV` | `ArgumentNullException` |
| `ctorneg` | native `std::length_error` | `ArgumentOutOfRangeException` |
| `growth` | UBSan `signed integer overflow: 1 + 2147483647` | `OutOfMemoryException` |

The whole suite also ran clean under ASan + UBSan + LSan. **Two honest non-results**: UBSan stays
silent for `trygetneg` (predicted, and confirmed as a non-result), and **TSan has no subject** —
both pool singletons are stateless, so no compatible ticket touches shared mutable state.

### #2061's pins

Six mutations applied temporarily; every one failed its own pin — disposed-throw (3 pins),
default-enumerates-0 (2), `MemoryHandle` destructor (1), `'G'`/`'D'` accepts `+` (2, while both
`'N'` pins stayed green), segment-chain constructor (compile-time `static_assert`),
`MemoryPoolHeapOwner_` gains a `bool` (`sizeof` `static_assert`, 32 → 40). All reverted, `git
diff` empty. **Three pins are controls** and correctly did not move.

### Validation

Gate **15,692 / 15,685 passed / 1 skipped / 6 failed** across all 37 executables run individually
and parsed from captured logs — `15,619 → 15,692`, exactly this batch's +73. The six failures are
the same two measured causes: five `PingTests` (#1962 — `ping_group_range` is `1 0`, so
unprivileged `SOCK_DGRAM` ICMP is denied while `SOCK_RAW` ICMP **succeeds** and `Ping` has no raw
receive path) and one `SocketTests` IPv6 case (`/proc/net/if_inet6` **absent**). Graph **41 / 91**,
seams **2 / 18**, negative fixtures **10 / 81** (91 invocations, peak 2 jobs, 32.8 s), checker
self-tests **45 / 45** and **15 / 15**, module-boundary self-tests **7 / 7**, catalogue current,
DB consistency OK, build **0 warnings / 0 errors**, `git diff --check` clean. Doxygen and `ccache`
are **not installed** here and nothing was installed.

### Three process mistakes, recorded rather than hidden

Reverting a temporary mutation with `git checkout --` destroyed two **uncommitted** #2061 doc
edits; both were detected by a grep over the expected ticket references, re-applied and verified
before the commit, and every remaining mutation used file backups instead. The same reflex
truncated **this file's** stacked header block once — caught immediately by a line count
(6,619 → 3,479), restored from `HEAD`, and redone as a **prepend**, which is what this file's
convention actually is, confirmed at 129 insertions / 0 deletions. And a global string replace
inserted this batch's `local_ci_check.sh` row into **thirteen** historical `NEXT.md` tables
instead of one; twelve were removed and the diff re-inspected until its only deletions were the
previous header, which is retained below it as a snapshot. Separately, `a620ade` carries #2051 as
well as #2049/#2050 although its message names only the latter two.

### Remaining

**#2054** is the one compatible ticket left in this namespace (SR-AUD-070 + 077), left last
deliberately because it is the only one needing a new negative-consumer-fixture site.
**#2056–#2059** are blocked and behaviour-pinned; **#2060** is a deferred verification. The next
namespace by the same measured rule is `modules/net-http` (9 open, 2 high) or `modules/xml`
(8 open, 2 high) — re-derive before committing.
