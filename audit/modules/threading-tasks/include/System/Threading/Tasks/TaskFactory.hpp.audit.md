# Audit: `modules/threading-tasks/include/System/Threading/Tasks/TaskFactory.hpp`

## Metadata

- AUDITED: factory construction defaults, exposed scheduler/options, action and
  generic StartNew forwarding, and `Task::Factory` definition.
- Validation: `TaskFactoryTests.*` passed 7/7 on 2026-07-27.

## Assessment

Default cancellation forwarding and generic action execution are covered.  A
factory can retain configuration values but cannot route work to a custom
scheduler or enact scheduler-related options; this follows the explicitly
documented TaskScheduler practical subset and is not an undisclosed finding.
Empty callable behavior is the shared SR-AUD-231 boundary defect in `Task.hpp`.

## Other missing assertions and diagnostics

- Add constructor/getter coverage for every supplied scheduler/token/options
  combination and explicit no-routing assertions.
- Add empty action/function regressions requiring the same immediate argument
  error as direct Task entry points, plus moved/destroyed scheduler-pointer
  lifetime tests.

## Final assessment

No additional finding beyond SR-AUD-231 was confirmed.  No source or test was
changed during this audit.

---

## Note — remediated with SR-AUD-231 (#1965, 2026-08-03)

All four `StartNew` overloads forward to `Task::Run` / `TaskT<TResult>::Run`,
whose constructors now reject an empty callable at the public boundary, so each
overload raises `System::ArgumentNullException` synchronously with .NET's own
per-overload parameter name — `action` for the two action overloads, `function`
for the two generic ones. No body in this header changed; only the
doc-comments, which now state the contract. Five regressions (including
`Task::Factory().StartNew`) pin it.
