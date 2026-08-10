# Audit: `modules/threading-tasks/include/System/Threading/Tasks/Parallel.hpp`

## Metadata

- AUDITED: Parallel options, loop state/result, batched For/ForEach, and
  Invoke exception collection.
- Validation: `ParallelTests.*`, `ParallelWithLoopStateTests.*`, and
  `ParallelLoopResultTests.*` passed 19/19 within the 171/171 module run on
  2026-07-27.  Direct C++20/current-.NET 10 probes used zero and -2 maximum
  degree values with a valid one-iteration body.
- Reference basis: current .NET 10 `ParallelOptions.MaxDegreeOfParallelism`
  validation and `Parallel.For` behavior.  The documented ascending-batch
  Break/Stop adaptation is not duplicated as a finding.

## SR-AUD-232 — medium — invalid maximum degrees are silently converted to hardware concurrency instead of rejected

The options overload changes every `MaxDegreeOfParallelism <= 0` to
`hardware_concurrency()`.  Consequently native calls with zero and -2 both
return a completed loop (`parallel_zero_valid_body_completed=1` and
`parallel_minus_two_valid_body_completed=1`).  The identical current-.NET
calls throw `ArgumentOutOfRangeException`; only -1 is the valid special value.
The unchecked conversion also obscures a caller's configuration error by
running work at a potentially much higher degree than requested.

## Assessment

The implemented batching avoids the former one-thread-per-item behavior in the
loop overloads and collects thrown worker exceptions.  Public options are still
accepted outside their managed domain.

## Other missing assertions and diagnostics

- Add zero, less-than--1, -1, one, and oversized degree tests; assert exact
  argument diagnostics and that invalid options run no body.
- Add null-equivalent body/action checks for every Parallel entry point;
  currently an empty `std::function` becomes an aggregate of delayed
  `bad_function_call` exceptions.
- Stress `Invoke`, which still launches every initializer-list action at once,
  and test thread-creation failure/aggregation, Break versus Stop under several
  in-flight batches, exception plus Stop races, and nested loops.

## Final assessment

SR-AUD-232 is confirmed by direct C++/current-.NET comparison.  No production
or test source was changed during this audit.

---

## Note — the empty-body half landed with SR-AUD-231 (#1965, 2026-08-03)

The second bullet of "Other missing assertions and diagnostics" above — *"an
empty `std::function` becomes an aggregate of delayed `bad_function_call`
exceptions"* — is a CCF-011 shape, not a degree-of-parallelism one
(`docs/ThreadingTasksChannelsReviewPlan.md` §3.1 item 5), and was repaired by
ticket **#1965** under SR-AUD-231. All five loop entries now reject an empty
`body` with `System::ArgumentNullException("body")` before any iteration is
dispatched, and `Invoke` rejects a null **element** with the plain
`System::ArgumentException("One of the actions was null.")` that .NET uses —
see that finding's Correction C3.

Two measured corrections belong here as well:

- **The aggregate was catchable.** `Parallel` already wrapped every worker
  exception in `System::AggregateException`, so ported
  `catch (const System::Exception&)` code *did* see the failure. CCF-011's
  "uncatchable" consequence never applied to this file.
- **The failure was iteration-count-dependent.** An empty range or an empty
  source ran no iteration, so the identical wrong call returned a normally
  completed `ParallelLoopResult`.

**SR-AUD-232 itself remains `confirmed`** — the maximum-degree validation is
ticket #1966.


---

## Correction and remediation — ticket #1966, 2026-08-03 (cause TC-B/1)

*Audit text above preserved verbatim; this section is appended.*

**Evidence:** `build-probe/1966_probe1_parallel_degree.cpp`, logs
`1966_probe1_before.log` / `1966_probe1_after.log` / `1966_probe1_asan.log`
(`hardware_concurrency = 4` on this machine).

### The finding reproduced exactly, and the concurrency claim is confirmed by measurement

| `MaxDegreeOfParallelism` | before | after |
|---|---|---|
| −3 | `no-throw`, ran 8, peak concurrency 4 | `ArgumentOutOfRangeException (Parameter 'MaxDegreeOfParallelism')`, ran **0** |
| −2 | `no-throw`, ran 8, peak 4 | rejected, ran **0** |
| −1 | `no-throw`, ran 8, peak 4 | **unchanged** — .NET's "unlimited" sentinel |
| 0 | `no-throw`, ran 8, peak 4 | rejected, ran **0** |
| 1 | ran 8, peak 1 | unchanged |
| 2 | ran 8, peak 2 | unchanged |
| cores + 4 | ran 8 | unchanged |

The report's second consequence — *"running work at a potentially much higher
degree than requested"* — is confirmed structurally and its magnitude is
machine-dependent: an invalid cap silently became a cap of
`hardware_concurrency()`, four here and as many as the core count elsewhere.

### C1 — the repair cannot be placed where .NET places it

.NET validates in the `ParallelOptions.MaxDegreeOfParallelism` **setter**, so an
invalid value can never be stored. In this port `MaxDegreeOfParallelism` is a
**public mutable data member** with nowhere to put a check, so validation happens
at the entry of the one method that reads it. The exception type and parameter
name are .NET's; only the *point of detection* moves, and that is stated in the
field's own doc-comment. Converting the field to a property pair would be a
public source break — the identical shape that gates SR-AUD-235 as ticket #1969
— and is deliberately not done.

### C2 — the check must precede #1965's `body` check, and did not by default

Measured on the intermediate tree, with #1965 landed and #1966 not:
`order.badDegree_and_emptyBody=sysexc:Value cannot be null. (Parameter 'body')`.
Because .NET rejects the invalid degree in the options setter — which necessarily
runs before `Parallel.For` is called, and therefore before .NET's own `body` null
check — reporting the body error there is a divergence. The degree check is
inserted **above** `requireNonEmptyBody`, and a regression pins the order. Same
shape as `docs/ThreadingNamespaceReviewPlan.md` §17.3's constraint on #1954.

### C3 — one site, not several

Only `For(fromInclusive, toExclusive, ParallelOptions, body)` reads the option;
there is no `ForEach` options overload in this port. The other four loop
overloads call `DefaultMaxDegreeOfParallelism()` directly and are untouched,
which a regression asserts.

*Reference-evidence limitation:* `/rv/tmp/runtime/src/libraries/` is absent from
this environment. The audit's managed probe records the exception **type**
(`ArgumentOutOfRangeException`) and the −1-only sentinel rule; the parameter name
`MaxDegreeOfParallelism` comes from .NET's `nameof(MaxDegreeOfParallelism)`
setter idiom rather than from a fresh reading of local source.

### Result

Valid values are −1 and every value ≥ 1; 0 and every value ≤ −2 are rejected
before any iteration is dispatched, with `ran = 0` measured in every rejecting
case. ASan + UBSan + LSan: 0 reports, exit 0 (32 sanitizer symbols).
`SharpRuntimeTests_Threading_Tasks` **208 → 218**. No public signature, object
layout, vtable or component-edge change.

**SR-AUD-232: `confirmed` → `remediated` (#1966, 2026-08-03).**
