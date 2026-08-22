<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Final audit findings reconciliation against `next`

*2026-08-22, ticket #2417.* Every one of the 71 findings that was still
`confirmed` or `confirmed (design-complete)` after commit `115ce1b0` was
checked individually against the current implementation, tests, public contract,
and discoverable ticket/commit history. The result below is generated from
`audit/final_dispositions.json` and checked against the authoritative index.

<!-- audit-status-counts: accepted-deviation=19; confirmed=0; confirmed (design-complete)=0; external-blocked=0; false-positive=2; remediated=343; total=364 -->

## Final distribution

| Disposition | Count |
| --- | ---: |
| `remediated` | 343 |
| `accepted-deviation` | 19 |
| `false-positive` | 2 |
| `external-blocked` | 0 |
| `confirmed` | 0 |
| `confirmed (design-complete)` | 0 |
| **total** | **364** |

`remediated` means the reported contradiction is fixed and covered.
`accepted-deviation` means the remaining behavior is an explicit, tested limit of
the practical C++ subset rather than forgotten implementation work.
`false-positive` means the audit premise was factually wrong. No finding is hidden
behind a closed status: the historical per-file reports remain intact as audit-time
evidence, while this document and the index describe current HEAD.

## Individually reviewed findings

| Finding | Final status | Current-HEAD evidence |
| --- | --- | --- |
| SR-AUD-001 | `remediated` | GitHub CI now runs the same ten selective components as the local matrix, including Collections.Blocking; validate_selective_component_matrix.py and four fixture tests prevent drift. |
| SR-AUD-002 | `remediated` | The boundary checker now has 17 isolated fixture tests covering public/private edges, test-only dependencies, unknown and stale declarations, allow-list errors, duplicate identities, cycles, and malformed module shapes. |
| SR-AUD-003 | `remediated` | BlockingCollection converts TimeSpan ticks to whole milliseconds before validating [-1, INT_MAX], matching .NET truncation at -0.5 ms, -1.5 ms, -2 ms, and the positive representability boundary; regressions cover all four timed APIs. |
| SR-AUD-004 | `remediated` | source_header_inventory.py now derives namespace ownership from header paths, treats definitions and aliases (not foreign forward declarations) as ownership, and cross-references plan.sqlite3 in both directions. Missing/unreadable inputs, unmatched primary headers, plan-only ported tasks, and stale exact exemptions all fail closed; the strict live checkout and focused fixtures are local gates. |
| SR-AUD-005 | `remediated` | index_dotnet_types.py has no checkout-specific path: callers select source and output, symlink outputs are rejected, default installation is atomic no-clobber even against a late creator, --replace is explicit, and scan/read failures preserve the prior database. Eight path/race/failure tests cover the contract. |
| SR-AUD-013 | `remediated` | Send_InvokesCallbackSynchronously now asserts invocation, state propagation, same-thread execution, and callback mutation instead of succeeding without observing the callback. |
| SR-AUD-014 | `remediated` | All file-backed compression integration tests use an atomically-created unique temporary directory with RAII cleanup; fixed shared /tmp names and pre-test recursive deletion were removed. |
| SR-AUD-042 | `remediated` | Tickets #2169 and #2170 added total-order Equals/GetHashCode and IEqualityComparer polymorphic binding for float, double, and Half; bit-pattern, layout, signature, and interface regressions are present. |
| SR-AUD-081 | `false-positive` | The premise was inverted: current .NET's Base64.DecodeFromUtf8 also counts accepted trailing whitespace in bytesConsumed. The port and its tests already match that measured contract (#1819). |
| SR-AUD-087 | `accepted-deviation` | ReadOnlySequence deliberately implements the project's contiguous single-segment subset and states that representation in its public contract. Linked segment construction would be a new multi-segment subsystem, not a repair to the promised surface. |
| SR-AUD-088 | `accepted-deviation` | MemoryHandle intentionally requires explicit Dispose: .NET's MemoryHandle struct has no finalizer/destructor, and adding RAII unpinning to this copyable C++ handle would unpin once per copy. The corrected contract and idempotent Dispose are pinned (#2059). |
| SR-AUD-092 | `remediated` | Commit 3a417d64 (#2323) gives a directly constructed Exception the exact nonempty .NET fallback and preserves explicit empty messages; direct base and derived-message regressions replace the stale empty-message pins. |
| SR-AUD-098 | `remediated` | Tickets #2307-#2310 now name the first cause, compose all inner diagnostics, preserve the raw/custom message through Handle and Flatten, retain breadth-first leaf order, and terminate repeated Flatten; dedicated regressions cover all clauses. |
| SR-AUD-124 | `remediated` | Ticket #2291 made ApplicationId validate Name, clone its byte-token in and out, and represent nullable Culture and ProcessorArchitecture with optional values; identity, null-vs-empty, copy, layout, and migration tests cover the repaired shape. |
| SR-AUD-125 | `remediated` | Ticket #2291 made ApplicationId::ToString emit .NET's quoted identity grammar, uppercase public-key-token bytes, and omit absent optional fields; unequal tokens no longer serialize identically and exact-text regressions cover the grammar. |
| SR-AUD-139 | `remediated` | Ticket #1996 promotes relative UriBuilder constructor text through the default HTTP base instead of producing :///; relative, default, and already-absolute regressions are present. |
| SR-AUD-140 | `remediated` | Ticket #1995 routes UriBuilder equality and hashing through canonical Uri identity, so user-info/fragment-only differences and other identity-equivalent builders agree. |
| SR-AUD-141 | `remediated` | Ticket #1996 validates and lower-cases schemes and brackets IPv6 hosts; invalid scheme, normalization, rendering, and round-trip tests cover the original cases. |
| SR-AUD-142 | `remediated` | Ticket #1995 canonicalized Uri equality and hashing across scheme/host case and default ports, with equality-hash agreement regressions. |
| SR-AUD-148 | `remediated` | Ticket #1999 changed UriTypeConverter::ConvertFrom to an optional result so empty input returns nullopt while invalid nonempty input remains diagnostic; compile-time and runtime tests cover the new shape. |
| SR-AUD-149 | `accepted-deviation` | UriCreationOptions now has constructor and TryCreate consumers (#1997), but its flag is intentionally inert because this Uri subset performs no path/query canonicalization to disable. The public warning and tests state that exact limitation. |
| SR-AUD-152 | `remediated` | Ticket #1980 added the valid default OSPlatform value while retaining named factories and equality semantics; default-vs-named behavior is tested. |
| SR-AUD-153 | `accepted-deviation` | RuntimeIdentifier is implemented as the .NET AppContext lookup with unknown fallback (#1980). FrameworkDescription is intentionally absent because it names the .NET product/version, for which this native runtime has no truthful counterpart. |
| SR-AUD-154 | `remediated` | Ticket #1983 uses native Windows OS-architecture detection rather than process architecture and treats unknown build targets as configuration errors; platform-specific tests pin the distinction. |
| SR-AUD-158 | `remediated` | Ticket #1980 made AmbiguousImplementationException final, reparented it directly to Exception, and added the message-plus-inner constructor; inheritance, construction, HResult, and negative-shape tests cover it. |
| SR-AUD-159 | `accepted-deviation` | ExternalException now has the error-code constructor and ErrorCode alias (#1980). Its specialized ToString remains intentionally absent (#2387), because correct most-derived type text requires reflection this runtime does not provide and a static name would misidentify downstream subclasses. |
| SR-AUD-160 | `remediated` | Ticket #1980 made CompilerFeatureRequiredAttribute::IsOptional constructor-initialized and getter-only; compile-time negative coverage prevents post-construction mutation. |
| SR-AUD-161 | `remediated` | Ticket #1981 made ConditionalWeakTable enumeration Reset a no-op and retain only Current rather than the entire snapshot; lifecycle and enumeration regressions cover both clauses. |
| SR-AUD-163 | `remediated` | Ticket #1980 introduced OSPlatformAttribute and reparents all platform annotations through it; inheritance and public-shape tests cover the metadata hierarchy. |
| SR-AUD-164 | `remediated` | Ticket #2417 represents all six nullable VersioningAttributes string properties with optional values while preserving empty-vs-absent and mutable Url semantics; runtime, reset, and layout regressions cover the full set. |
| SR-AUD-165 | `remediated` | Ticket #1980 corrected UnmanagedType::LPStruct to 43 and added Currency and IDispatch with their managed values; exact enum tests prevent collisions and omissions. |
| SR-AUD-166 | `remediated` | Ticket #1980 corrected StructLayout Pack and DllImport PreserveSig/BestFitMapping defaults to 0/true/false as appropriate, with constructor-default and explicit-value tests. |
| SR-AUD-167 | `remediated` | Tickets #1980/#2417 provide typed MarshalAs metadata and final getter-only InterfaceTypeAttribute/ClassInterfaceAttribute values with enum and raw-short constructors; runtime and negative-shape fixtures cover the public contract. |
| SR-AUD-168 | `accepted-deviation` | Interop attributes are intentionally inert metadata because declaration attachment, P/Invoke, COM, and managed marshalling are outside the practical subset. The header now states that boundary and directs callers to native C++ mechanisms; metadata values themselves are tested. |
| SR-AUD-173 | `remediated` | Commit 2cc9870a (#2336) drives all CharUnicodeInfo numeric queries from generated Unicode 16.0 numeric data across the full scalar range; Arabic-Indic digits, Roman numerals, fractions, overloads, and invalid inputs are covered. |
| SR-AUD-174 | `remediated` | Commit ac76bf19 (#2315), extended by bb4189c6, replaced locale predicates with generated Unicode 16.0 category/case tables over the full code space; combining marks, punctuation, private-use, symbols, surrogates, and string-index overloads are covered. |
| SR-AUD-176 | `accepted-deviation` | BFloat16 now implements the bounded arithmetic/classification/formatting surface selected in #2340/#2382. Generic-math interfaces and language-specific static abstract APIs remain outside the practical C++ subset and are explicitly enumerated in the public contract. |
| SR-AUD-182 | `accepted-deviation` | The runtime deliberately follows .NET invariant-globalization normalization semantics: all strings are treated as normalized and returned unchanged. Full UAX #15 needs ICU or absent decomposition/composition tables; #2338 records and tests the limitation. |
| SR-AUD-186 | `accepted-deviation` | BinaryData deliberately snapshots every source, including ReadOnlyMemory, rather than aliasing caller storage. This defensive owning-copy contract avoids borrowed-lifetime hazards, is disclosed on every affected door, and is pinned under #2106. |
| SR-AUD-191 | `remediated` | Ticket #1956 made Threading timer disposal a terminal state: Change after Dispose reports failure instead of accepting work for a stopped worker; lifecycle regressions cover repeat disposal and post-dispose operations. |
| SR-AUD-193 | `remediated` | Ticket #1958 assigns each native thread a distinct stable ManagedThreadId using thread-local identity; main, worker, repeated-read, and cross-thread tests cover uniqueness. |
| SR-AUD-194 | `remediated` | Ticket #1958 added the parameterized Thread callback shape and forwards Start(void*) state to it instead of discarding the argument; exact identity and one-shot tests cover the path. |
| SR-AUD-196 | `accepted-deviation` | ThreadStartException now matches .NET's final type, fixed message, inner cause, and HResult. Constructors remain public under SA-12 because this port has no runtime producer or C++ equivalent of internal; the accessibility deviation is explicit and tested (#2390). |
| SR-AUD-201 | `remediated` | Ticket #1957 enforces PeriodicTimer's single-consumer rule so a concurrent second WaitForNextTick is rejected rather than sharing one tick; cancellation/disposal/reuse regressions cover the state machine. |
| SR-AUD-203 | `remediated` | Tickets #1955, #1956, and #2389 make disposed state atomic and make ReaderWriterLockSlim::Dispose reject both owned locks and queued waiters; TSan and disposal-state regressions cover the formerly split finding. |
| SR-AUD-208 | `remediated` | Ticket #1956 gives Mutex, AutoResetEvent, and ManualResetEvent terminal Close/Dispose state and ObjectDisposedException on later waits; all three handle families and repeated disposal are tested. |
| SR-AUD-209 | `remediated` | Ticket #1958 derives both event types from WaitHandle, enabling heterogeneous WaitAny/WaitAll collections; inheritance, ordering, timeout, and disposal tests cover the shared base. |
| SR-AUD-215 | `accepted-deviation` | ExecutionContext is an explicit synchronous no-flow subset: this runtime has no async/await ambient context to capture, Capture returns null, and Run invokes synchronously even with null. The public contract no longer promises managed flow semantics. |
| SR-AUD-219 | `remediated` | Tickets #1951 and #1956 reject empty ThreadLocal factories at construction and throw ObjectDisposedException from IsValueCreated after disposal; validation order and lifecycle tests cover both original clauses. |
| SR-AUD-220 | `remediated` | Ticket #1958 implements trackAllValues and Values, including the false-tracking diagnostic, cross-thread collection, disposal, and repeated-read behavior; the flag is no longer dead state. |
| SR-AUD-228 | `accepted-deviation` | TimeZoneInfo intentionally compares the available base-offset and DST-support model. Faithful HasSameRules needs stored TZif adjustment rules and an environment-specific tzdata subsystem not in scope; #2185 documents and pins the one-directionally permissive result. |
| SR-AUD-235 | `remediated` | Ticket #1969 validates BoundedChannelOptions::FullMode on assignment and rejects undefined enum values before channel construction; capacity and validation-order regressions cover the prior overfill path. |
| SR-AUD-239 | `remediated` | Ticket #2155 makes System::Timers::Timer an Object and passes the raising instance to Elapsed; sender identity, polymorphic shape, size/vtable cost, and handler behavior are permanently tested. |
| SR-AUD-246 | `remediated` | GenericPrincipal role membership now compares decoded Unicode scalars with deterministic invariant simple case folding instead of locale-dependent bytewise tolower; non-ASCII, multi-code-point, and malformed UTF-8 regressions cover the boundary. |
| SR-AUD-259 | `remediated` | Tickets #2149 and #2150 route compression strategy to zlib and add ZLibCompressionOptions constructors to DeflateStream, GZipStream, and ZLibStream; option propagation, container identity, and default-byte-stability tests cover both halves. |
| SR-AUD-273 | `remediated` | Commit f9f0833b (#2031) implements Kill(entireProcessTree) with a Linux descendant walk rather than one process-group signal and documents the portable fallback; nested setsid and cleanup regressions cover the original survivor. |
| SR-AUD-279 | `accepted-deviation` | StringInfo and TextElementEnumerator now share UTF-8 scalar boundaries, so SubstringByTextElements never treats an element index as a byte offset and never splits a valid scalar; valid UTF-8 input remains valid. Malformed bytes are deterministic one-byte elements. Full grapheme-cluster segmentation remains an explicit no-ICU/UAX #29 subset limitation. |
| SR-AUD-281 | `remediated` | Calendar is now abstract and cannot be instantiated as a disguised Gregorian calendar; a private concrete test adapter exercises the reusable base defaults, while compile-time assertions pin Calendar as abstract and GregorianCalendar as concrete. |
| SR-AUD-282 | `remediated` | IdnMapping::AllowUnassigned now controls generated-Unicode OtherNotAssigned rejection: default false rejects U+0378 and true admits it, with UTF-8, label, and property regressions. |
| SR-AUD-283 | `accepted-deviation` | CompareInfo validates each door's option domain, supplies deterministic Unicode ordinal/simple-ignore-case comparison, and rejects Ordinal/OrdinalIgnoreCase from GetSortKey as .NET does; unsupported linguistic flags fail explicitly. Culture-tailored ICU collation remains outside the invariant practical subset and is documented. |
| SR-AUD-284 | `accepted-deviation` | TextInfo no longer uses locale-dependent bytewise C routines and instead applies deterministic invariant Unicode simple casing and title-case word scanning, including leading nonletters and apostrophe tails. Culture-specific tailoring remains outside the no-locale-database subset and is stated in the API contract. |
| SR-AUD-285 | `accepted-deviation` | CultureInfo and RegionInfo deliberately provide an invariant/US-oriented fallback without a locale database. Unknown-name pseudo-region behavior and USD defaults are now documented as subset behavior rather than claimed full culture validation. |
| SR-AUD-294 | `remediated` | Rune classification and casing already use generated Unicode 16.0 tables; ticket #2417 removes the residual incorrect FEFF whitespace classification and adds a regression alongside non-ASCII letter/digit/case coverage. |
| SR-AUD-308 | `remediated` | Commit dba5ace2 (#2042) implements CookieContainer total/per-domain capacities, maximum cookie size, aging, cleanup, and deterministic eviction; capacity and concurrency tests cover bounded storage. |
| SR-AUD-317 | `remediated` | Commit 52a6a5d6 (#2070) makes StringContent derive both payload bytes and charset from the selected Encoding, rejecting unsupported combinations; UTF-8/UTF-16 and header-byte agreement tests cover the original contradiction. |
| SR-AUD-324 | `remediated` | Commit 6b827945 (#2117) shares document lifetime/disposal state with captured JsonElements, which now throw ObjectDisposedException after their owner is disposed while keeping the backing DOM alive safely; retained-element regressions cover it. |
| SR-AUD-325 | `accepted-deviation` | JsonProperty::ToString now renders the JSON property name, separator, and value instead of dropping the name. JsonElement raw text remains canonical DOM re-rendering because the parser intentionally does not retain source lexemes; that limitation is explicit. |
| SR-AUD-327 | `remediated` | Tickets #1886/#1887/#1889/#2118/#2391 repaired JsonNode parent ownership, destruction, assignment/copy shape, and fail-fast enumeration; ticket #2417 also invalidates enumerators when an existing JsonObject value is replaced. Lifetime, ownership, negative-shape, and version regressions cover the full finding. |
| SR-AUD-333 | `accepted-deviation` | Tickets #1890/#1891/#1896/#1898/#1899 repaired owner destruction, mutation rollback, copy shape, events, and safe visitor alternatives. The remaining Ancestors pointer list and attribute reference are explicitly borrowed C++ views with documented owning/callback alternatives, not lifetime-extending .NET objects. |
| SR-AUD-336 | `remediated` | Ticket #2199 implements XObject Changing/Changed registration and dispatch with reentrancy/event-order behavior, sender identity, and mutation coverage; the former assertion-free inert-event pins were replaced. |
| SR-AUD-362 | `false-positive` | The premise was inverted: current .NET ToFrozenDictionary/Create uses last-value-wins for duplicate keys. FrozenDictionary already matches that behavior and its duplicate-key tests are therefore retained (#1778/#1779). |

## SR-AUD-071 / 071b

The post-dispose owner getter is remediated and throws. A previously retained Memory remains a non-owning C++ view, like span, and is invalid after owner disposal/destruction by the explicit IMemoryOwner lifetime contract; making every Memory shared-owning would contradict the project's borrowed-view model.

The prior row was internally inconsistent: it was marked `remediated` while saying
071b remained open. Its final status is therefore `accepted-deviation`, not
`remediated`; the owner getter repair remains credited, and the retained-view rule
is now stated consistently in IMemoryOwner, MemoryPool, its migration note, and tests.

## Metadata correction

**SR-AUD-117:** Ticket #2289 applies C++ [[deprecated]] diagnostics to all five managed-obsolete members and negative fixtures prove -Werror rejection. ApplicationId closure text accidentally appended to this row belonged to SR-AUD-124/125 and is removed by this reconciliation.

## Other stale index summaries corrected

- **SR-AUD-230:** Ticket #1970 makes TaskCanceledException own a copied Task handle, preserving the same shared state and later status changes without retaining the caller's raw pointer; lifetime, state-identity, layout, and borrowing-mutation regressions cover the repair.
- **SR-AUD-247:** Ticket #2088 gives every ClientWebSocket async operation an interruptible in-flight lifetime boundary and copies SendAsync input bytes; destruction no longer leaves raw-this tasks running, and the earlier CCF-019-open tail was only the state at that ticket's landing.
- **SR-AUD-263:** Ticket #2134 gives Socket async work a crossable in-flight boundary; final ticket #2417 extends it to public Close, move construction, source-side move assignment, failed Task startup, and descriptor-preserving drain semantics. AcceptAsync is stop-polled, teardown may shutdown blocking I/O, and source moves naturally await Connect/Send/Receive so the transferred socket remains usable. Deterministic lifetime, move, bidirectional connected-socket, rollback, layout, and negative-seam regressions cover the complete boundary.
- **SR-AUD-310:** Ticket #2066 gives HttpClient async methods a destructor-waited in-flight boundary, so no task can dereference a destroyed client; the earlier blocked-design and CCF-019-open paragraphs are historical pre-remediation evidence, not current status.

## Repository policy and external work

The close-out sanitizer pass after the index reconciliation found two additional in-scope UB
classes before ticket #2417 was closed. Half and BFloat16 integral conversions now avoid native
out-of-range floating-to-integral casts while matching the current .NET runtime's unchecked
lowering, and ClientWebSocket no longer passes null `vector::data()` pointers to zero-byte
`memcpy`. Permanent regressions cover both. The same pass corrected a member-initialization-order
bug in a diagnostics test helper, an owned-enumerator leak in a runtime test, and two socket tests
whose immediate `Task::IsCompleted` assertion raced terminal-state publication even though the
raw-`this` lifetime boundary had already been crossed; the Socket production boundary itself did
not need widening.

Ticket #2417's audit-close-out verification is reproducible from the repository gates: a
cache-disabled two-job build was warning-free; all **17,781 tests across 38 executables**
passed with no failure or skip; all ten selective components passed; and module, catalogue,
audit, planning, inventory, Unicode, temporary-path, seam, and negative-fixture checks passed.
ASan+UBSan+LSan covered **12,793 relevant tests
across 22 executables**, strict `float-cast-overflow` UBSan covered all **6,156 Core.Base tests**,
and three repeated targeted TSan groups passed **57/57** without a report. The audit distribution
remains 343/19/2 because these late defects were fixed before #2417 closed rather than hidden in
a new open status.

A later post-#1941 consumer audit (#2418) found a bounded DateTimeKind ripple outside those 364
historical findings. It repaired DateTime clocks/arithmetic, DateTimeOffset Kind and offset
boundaries, TimeZone/TimeZoneInfo validation and result Kinds, and the affected IO, HTTP, XML,
globalization, threading, and timer consumers. Its current gate is **17,840/17,840 across 38
executables**, with graph **41 modules / 96 edges**. ASan+UBSan+LSan and strict UBSan each passed
**9,680 relevant tests across 9 executables**; TSan passed **7,488 tests across 5 executables**
and the process-timezone concurrency case passed five repeated runs. The audit distribution
remains 343/19/2 because #2418 is a later correctness ticket, not a relabeling of the closed
historical audit.

The checked Doxygen ceiling remains 2,675 warnings and runs in both GitHub CI and
`scripts/local_ci_check.sh`; this reconciliation does not re-baseline historical
warnings upward. Audit/index consistency is now a local gate as well.

The planning tickets #1773, #1962, and #2381 are external/environment blockers,
not unresolved audit findings. They remain blocked in `plan.sqlite3` and are not
reclassified by this source-tree reconciliation.
