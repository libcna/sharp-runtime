---
name: classify-plan-types
description: Classify unclassified .NET types tracked in plan.sqlite3 (table task) as ported / ignore / tobedecided, per prompt.md's full workflow. Use when reviewing or advancing plan.sqlite3 namespace classification in sharp-runtime.
---

`plan.sqlite3` (table `task`) tracks indexed .NET types from dotnet/runtime.
Rows started with an empty status; the current maintainer snapshot is fully
classified. Full workflow detail lives in `prompt.md` — this is the summary:

1. For each type where status is `''` or `todo` (System-namespace types first), look up what it does in `/rv/tmp/runtime/src/libraries/` and classify it **without asking the user**:
   - **Port it** → check if the file exists in sharp-runtime, review against the full checklist, port or fix, then set `status = 'ported'` and commit.
   - **Out of scope / irrelevant** → set `status = 'ignore'`, and set `outofscope = 1` for permanent-deviation categories (reflection, GC internals, P/Invoke, serialization infra, etc.) or `outofscope = 0` otherwise.
   - **Genuinely ambiguous** → set `status = 'tobedecided'` rather than guessing; the user reviews these by hand later.
2. Keep processing items back-to-back — do not stop between items to ask for confirmation.

Valid statuses written by the current workflow are `''` (unset), `todo`,
`ported`, `ignore`, and `tobedecided`. The database also contains legacy
`ignored` rows; treat them as classified and do not rename them mechanically.
**`in_progress` does not exist** — porting happens directly with no
intermediate state.

State lives in `plan.sqlite3` + git history, not conversation memory, so this process resumes cleanly after any context reset — just re-open `prompt.md` and continue from Step 1.
