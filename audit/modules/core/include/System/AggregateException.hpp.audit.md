# Audit: `modules/core/include/System/AggregateException.hpp`

## Metadata

- Audit status: AUDITED (145-line inline implementation, fully read).
- Validation: `AggregateExceptionTests.*` passed 13/13 on 2026-07-26.
- Reproduction: `/tmp/sharp-runtimervc-aggregateexception-audit-probe` reports lost custom message/inner state and, in isolated modes, a null-inner segfault (exit 139) and empty-handler `std::bad_function_call` termination (exit 134).
- Reference basis: local .NET `AggregateException.cs` constructor validation, `InnerException`, `Handle`, and `Flatten` implementations.

## SR-AUD-097 — high — null inner `exception_ptr` is accepted and later crashes in aggregate message processing

Unlike .NET, which rejects null inner exceptions at construction, `std::vector<std::exception_ptr>{nullptr}` is accepted. The collection constructor immediately sends that null pointer to `std::rethrow_exception` in `buildMessage`; the isolated probe exits 139 with a segmentation fault. The message-plus-vector and message-plus-single constructors also retain null entries without validation, leaving later `GetBaseException` / `Flatten` paths unsafe. Existing tests cover only non-null runtime errors.

## SR-AUD-098 — medium — aggregate constructors and transformations lose .NET causal diagnostics and flatten ordering

The message-plus-inner/vector constructors neither set the base `Exception::innerException_` to the first inner exception nor append contained messages to `what()`. The probe prints `custom_message=outer` and `custom_inner=null`; .NET exposes the first inner through `InnerException` and includes each inner diagnostic in `Message`. `Handle` rebuilds unhandled errors with the generic default text, and `Flatten` likewise discards the caller's message. Its recursive depth-first `collectLeaves` emits nested leaves before a later direct leaf (`a,b,c`), where current .NET's queue algorithm returns direct leaves before queued nested leaves (`c,a,b`). The green test `MessageAndSingle_Ctor_StoresInner` locks the incomplete C++ text rather than the .NET-shaped causal message.

## SR-AUD-099 — medium — `Handle` accepts an empty predicate and defers validation to `std::bad_function_call`

`Handle` invokes its `std::function` without validating it. An empty predicate therefore raises native `std::bad_function_call` only after aggregate construction and first traversal; the isolated probe terminates when this exception is left uncaught. .NET rejects a null predicate deterministically at the public boundary with `ArgumentNullException`. This extends the empty-callable pattern in CCF-011.

## Other missing assertions and diagnostics

- Tests omit empty/null inner entries, first-inner identity, custom-message aggregation, `Handle` message/order preservation, predicate exceptions, empty predicates, and nested/direct leaf order.
- `GetBaseException` returns copied `exception_ptr` state for a zero/multiple aggregate because this C++ value API cannot return an owning pointer to `*this`; tests do not document this adaptation or compare nested identity.
- No HResult assertion checks the inherited `Exception` code.

## Final assessment

Normal non-null happy paths pass, but public invalid-input handling and key causal diagnostics are incompatible. No source or test was modified.

## Post-audit remediation for SR-AUD-097 (ticket #1807, 2026-07-29): REMEDIATED

The audit evidence above is retained unchanged. **SR-AUD-098 and SR-AUD-099 are
untouched and stay `confirmed`** — this ticket repaired the null inner
`exception_ptr` only. SR-AUD-098 is a causal-diagnostics and flatten-ordering
contract; SR-AUD-099 belongs to **CCF-011** (empty `std::function` values crossing
public boundaries), which the remediation roadmap requires be taken as a scoped
family, not one file at a time.

Ticket #1807 (`REMED-CORE-AGGREGATEEXCEPTION-NULL-INNER`, P1, size S) rejects a
null entry at every constructor that accepts inner exceptions.

**The finding named one crash path. There were three, and two silent ones.**
`std::rethrow_exception` has undefined behaviour for a null argument, and three
members call it, so one accepted null armed all three at once. Measured before
any production change, one process per case
(`build-probe/1807_prefix_defects.cpp`, log `build-probe/1807_prefix_defects.log`):

| # | Input / call | Pre-fix | Post-fix |
|---|---|---|---|
| 1 | `AggregateException(vector{null})` | **ASan SEGV** in `std::rethrow_exception`, address `0xffffffffffffff80`, exit 1 | `ArgumentException` — *An element of innerExceptions was null.* |
| 2 | `AggregateException({null})` | same | same |
| 3 | `AggregateException(vector{valid, null})` | same | same |
| 4 | `AggregateException("m", vector{null})` | **constructed**, `count=1` | same |
| 5 | …then `Flatten()` | same SEGV, via `collectLeaves` | rejected at construction |
| 6 | …then `GetBaseException()` | same SEGV | rejected at construction |
| 7 | …then `Handle(always-true)` | completed, `predicate-saw-null=1` | rejected at construction |
| 8 | `AggregateException("m", exception_ptr())` | **constructed**, `count=1` | `ArgumentNullException` — *(Parameter 'innerException')* |
| 9 | …then `Unwrap()` | completed, `unwrapped-null=1` | rejected at construction |
| 10 | `AggregateException(vector{valid})` | `One or more errors occurred. (boom)` | **byte-identical** |
| 11 | `AggregateException("m", valid)` | `count=1 message='outer'` | **byte-identical** |

Cases 7 and 9 are the ones worth naming separately: they did **not** crash. The
message-plus-collection and message-plus-single constructors never built a
message from their inner exceptions, so they stored the null quietly and
`Handle()` passed it straight to the caller's predicate while `Unwrap()` returned
it. The crash then happened in consumer code, at a `std::rethrow_exception` the
consumer wrote, with nothing left to indicate where the null had entered.

The trap address is `0xffffffffffffff80` rather than a plain `0x0`: that is what
a null `std::exception_ptr` decodes to inside libstdc++'s `rethrow_exception`, and
it is recorded here so a future reader matching this signature is not misled into
looking for an ordinary null dereference.

**The two exception types are not interchangeable and the split is deliberate.**
.NET's private `AggregateException(string?, Exception[], bool)` core constructor —
which every public collection-taking constructor funnels through — throws
`ArgumentException(SR.AggregateException_ctor_InnerExceptionNull)`, *"An element of
innerExceptions was null."*, for a null **element**, while
`AggregateException(string?, Exception)` at `AggregateException.cs:59` opens with
`ArgumentNullException.ThrowIfNull(innerException)` for a null **argument**. This
port now reproduces that split. Because `ArgumentNullException` derives from
`ArgumentException`, a test that caught only the base type would pass even if the
two were collapsed, so one permanent test asserts the collection case is **not**
caught as `ArgumentNullException`.

`Flatten()` and `Handle()` construct new aggregates internally; both build from
entries that were successfully rethrown or copied from an already-validated
vector, so neither can trip the new check. The two in-repository producers,
`CancellationTokenSource` and `Parallel`, both push `std::current_exception()` from
inside a `catch (...)`, which is never null there.

Closure evidence: 10 new permanent regressions in `ExceptionRemainingTests.cpp` —
null in a vector, null in an initializer list, null after a valid entry, the exact
.NET message text, null with the message-plus-vector constructor, null with the
message-plus-single constructor, its parameter name, the type split described
above, an assertion that no constructed aggregate (flat, nested, or either
flattened) can hold a null inner, and the unchanged valid paths including the
empty-collection default message. `SharpRuntimeTests_Core_Base` **4,982/4,982**
(was 4,972), and the same 4,982 under AddressSanitizer + UndefinedBehaviorSanitizer
+ LeakSanitizer with zero reports (`build-asan/1807_core_base_asan.log`).
Repository gate: 0 warnings, 0 errors, **13,958 tests across 37 executables** (was
13,948). Doxygen 1,941 of the 1,942 ceiling, unchanged.

Source and ABI consequences: none. `AggregateException` is header-only and gains
two private static helpers; no public signature, object layout, vtable or exported
symbol changed. One behavioural note for consumers: code that constructed an
aggregate over a null `std::exception_ptr` now receives an argument exception at
construction instead of crashing later. No in-repository caller did so.

The audit report's "Other missing assertions and diagnostics" list is **not**
closed by this ticket beyond its null entry: first-inner identity,
custom-message aggregation, `Handle` message/order preservation, predicate
exceptions, empty predicates, nested/direct leaf order, the `GetBaseException`
value-API adaptation, and the HResult assertion all belong to SR-AUD-098 and
SR-AUD-099, which remain open.

---

## SR-AUD-099 — REMEDIATED (ticket #1867, 2026-07-30, CCF-011)

The original evidence above is retained unchanged.

`AggregateException::Handle` now rejects an empty `std::function` predicate with
`System::ArgumentNullException("predicate")` **before** the loop, matching
`AggregateException.Handle`'s own `ArgumentNullException.ThrowIfNull(predicate)`
in the current local reference. The message is byte-identical to .NET's:
`Value cannot be null. (Parameter 'predicate')`.

**Correction to the finding's premise (measured 2026-07-30).** The finding
describes only the `std::bad_function_call` path — "an empty predicate therefore
raises native `std::bad_function_call` only after aggregate construction and
first traversal". Measured, that is true only when the aggregate *has* an inner
exception. An aggregate with **no** inner exceptions accepted the same empty
predicate and returned normally (`aggregate.handle.noinner=no-throw` in
`build-probe/1866_prefix.log`), so the identical wrong call was fatal or
invisible depending on the aggregate's contents. .NET's check runs before the
loop and therefore throws in both cases; the repair does too. The historical text
above is left as written, per this repository's practice.

Closure evidence: 5 new permanent regressions in `ExceptionRemainingTests.cpp`
(empty predicate with an inner exception; empty predicate with **no** inner
exception; the exact `paramName` in the message; catchability as
`System::Exception`, which `std::bad_function_call` never had; and an assertion
that a non-empty `std::function` predicate still handles and still rethrows
exactly as before). `LazyTests` + `AggregateExceptionTests` 75/75. The direct
probe `build-probe/1866_empty_callable_probe.cpp`, compiled **with**
`-fsanitize=address,undefined` so this header-only change is itself instrumented,
exits 0 with zero AddressSanitizer, UndefinedBehaviorSanitizer and LeakSanitizer
reports, including a 2,000-iteration stress loop that constructs and rejects
heap-owning predicates.

Source, ABI and layout consequences: none. `Handle` keeps its signature, its
`const` qualification and its (absent) `noexcept` specification; no data member,
virtual function or exported symbol changed. No in-repository caller passes an
empty predicate.

The plan for this family is `docs/EmptyCallableBoundaryPlan.md` (ticket #1866).
SR-AUD-098 and the remaining items listed at the end of the #1807 note —
first-inner identity, custom-message aggregation, `Handle` message/order
preservation, predicate exceptions, nested/direct leaf order, the
`GetBaseException` value-API adaptation and the HResult assertion — are **not**
closed by this ticket.

---

## SR-AUD-098 — PARTIALLY REMEDIATED, STAYS `confirmed` (tickets #2306-#2310, 2026-08-11)

The audit evidence above is retained unchanged. **SR-AUD-098 remains
`confirmed`.** It is a conjunction of six clauses; two close here and four do
not, so the finding does not transition. The full review record is
`docs/AggregateExceptionCausalPlan.md` (ticket #2306).

`/rv/tmp/runtime/src/libraries/` is **not present in this environment.** Every
.NET statement below is quoted from the frozen finding text itself — repository-
owned recorded evidence — and nothing is recalled from memory.

**The finding split by assertion, and its disposition:**

| Clause | Frozen assertion | Ticket | Disposition |
|---|---|---|---|
| C1 | base `Exception::innerException_` not set to the first inner | #2307 | **closed** |
| C2 | contained messages not appended to `what()` | #2309 | deferred, `needs_user` |
| C3 | `Handle` rebuilds with the generic default text | #2309 | deferred, `needs_user` |
| C4 | `Flatten` discards the caller's message | #2309 | deferred, `needs_user` |
| C5 | depth-first `a,b,c` where .NET's queue returns `c,a,b` | #2308 + #2310 | **closed** |
| C6 | a green test locks the incomplete text | #2309 | follows C2 |

**C1 needed no external authority.** `System::Exception` documents
`getInnerExceptionProperty()` as "the exception that caused the current
exception", and every one of this class's six constructors left it null while
`getInnerExceptionsProperty()` held the causes — an aggregate that stored *N*
causes reported none. That is a violation of this port's own base-class
contract. *Premise extension, measured per constructor:* the frozen text names
only the "message-plus-inner/vector constructors", but the plain vector and
initializer-list constructors had the identical gap; all six were measured, the
four that store a cause now name it, and the two that store none stay null.

**C5 needed no external authority either, because the finding itself records
the answer** — both the algorithm ("current .NET's queue algorithm returns
direct leaves before queued nested leaves") and the exact expected output
(`c,a,b`). That recorded evidence is used exactly as scoped. Ordering was not
guessed; had the finding recorded only "different leaf order", C5 would have
been deferred with the rest.

**C2/C3/C4 are one decision, not three, and it is approval-bound.** This class
has no raw-message field: it stores only the composed `what()` string, so a
constructor that composed `"m (a) (b)"` cannot later be asked for `"m"` to
rebuild a derived aggregate from. Preserving the message through
`Handle`/`Flatten` and composing contained messages into it therefore cannot
both hold without a new member — a public representation change — and the
composed text itself (wording, punctuation, separator) is the unavailable fact.
Writing a plausible string would present invention as parity. Ticket #2309.

**A defect in the C5 repair, found and fixed in the same batch (ticket #2310).**
Ticket #2308 introduced the queue but subscripted it
`pending[pending.size() - 1 - head]`. `pending` grows by one entry per enqueued
list, so `pending.size()` and `head` advance in lockstep and the index stays
pinned at `0`: the seed list is re-walked forever, and the loop condition
`head < pending.size()` never fails because the body keeps extending what it
tests against. Measured with `build-probe/2309_flatten_defect.cpp` under
`timeout 25` and `ulimit -v`:

| Shape | #2308 as shipped | After #2310 |
|---|---|---|
| `{a, b}` — already flat | `a,b` | `a,b` |
| `{a, Aggregate{b,c}}` — #2308's own *control* | **did not terminate** | `a,b,c` |
| `{Aggregate{a,b}, c}` — the finding's shape | not reached | `c,a,b` |

The hang reached **every caller of `Flatten()` on a nested aggregate**, which is
the only case `Flatten()` exists for. The repair is `pending[head]`; termination
then holds because each iteration consumes exactly one list and each nested
aggregate contributes exactly one, over a finite tree. #2308's own order tests
cover the hanging shapes and would have caught this by hanging — they were never
run, and #2308's commit message asserts a validation that did not happen. #2310
adds two assertions on exact leaf multiset and count so a future queue that
terminates but re-walks a list fails fast instead of hanging.

**Source, ABI and layout consequences: none.** `AggregateException` is
header-only. No public signature, overload, base, member, object layout, vtable,
`noexcept` specification, exported symbol or module edge changed;
`sizeof`/`alignof` stay 192/8.

**Behavioural transitions, both required by the frozen finding.** An aggregate
that stores causes now reports its first cause through the inherited
`getInnerExceptionProperty()` instead of null; no first-party production code
reads that property on an aggregate, every use in the repository is in a test
tree, and none pinned it as null. `Flatten()` returns the same leaves, with the
same identities and count, in queue order rather than depth-first order; shapes
on which the two algorithms agree are byte-identical. Because the default
message is composed from the leaves in leaf order, a flattened aggregate's
default message lists the same leaves in the new order — a consequence of C5,
not an independent message decision. C1's null-element scan moved into the
base-class initializer, sequenced before every member initializer, so ticket
#1807's `ArgumentException` / `ArgumentNullException` split, both message texts
and the `paramName` survive verbatim.

The "Other missing assertions and diagnostics" list is **not** closed beyond
first-inner identity and nested/direct leaf order: custom-message aggregation,
`Handle` message/order preservation, predicate exceptions, the
`GetBaseException` value-API adaptation and the HResult assertion remain open
under C2/C3/C4 and the residual list.
