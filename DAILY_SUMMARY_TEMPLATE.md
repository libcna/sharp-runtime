# Work-session summary template

Use this checklist at the end of a substantive Sharp Runtime session. Do not
prepend another chronological diary to `NEXT.md`; update its existing
snapshot, recent-changes, limitations, and next-task sections in place. Git
history preserves older handoffs.

Durable detail belongs in three places:

- The affected `ticket.notes` row in local `plan.sqlite3`.
- A focused commit message with the ticket number and validation result.
- `NEXT.md` only when the cold-start facts or ordering actually changed.

Keep the summary factual: exact commits, test counts, graph counts, toolchain
versions, and confirmed behavior. Distinguish a fixed bug from a clean audit
or a deliberately deferred gap.

```markdown
## Session result — YYYY-MM-DD: <short theme>

**Branch/code baseline:** `feature/work` at `<short-hash>`

**Validation:**

- Module graph: <N> physical modules, <N> production edges
- Build: <0 warnings, 0 errors or exact blocker>
- Tests: <N> passed across <N> executables
- Extra checks: <selective matrix / TSan / ASan / UBSan / cross-build / none>

**Ticket state:**

- Ticket #<N>: `<before-status>` → `<after-status>`
- Queue after the session: <todo/doing/blocked/needs_user counts>

**Behavior changed:**

- <What was wrong, the relevant .NET reference path, what changed, regression
  test, and commit hash.>

**Audited with no behavior change:**

- <What was checked and the evidence for a clean result.>

**Deferred or newly discovered:**

- <Exact gap, why it was not included, and the proposed next bounded step.>

**Documentation updated:**

- <README/NEXT/plan/component catalogue/module README, or “not needed”.>

**Recommended next task:**

- <One bounded task and why it is next.>
```

## End-of-session checks

Run the full local gate:

```bash
scripts/local_ci_check.sh build
```

If component metadata changed, also run the complete selective matrix:

```bash
scripts/check_selective_components.sh
```

Then verify the queues:

```bash
sqlite3 plan.sqlite3 \
  "SELECT status, priority, COUNT(*) FROM ticket GROUP BY status, priority ORDER BY priority, status;"

sqlite3 plan.sqlite3 \
  "SELECT status, COUNT(*) FROM task GROUP BY status ORDER BY status;"
```

Update `NEXT.md` when any of these facts changed:

- Code baseline or verified test count.
- Component/module/edge count.
- CI or platform evidence.
- A known limitation or permanent scope decision.
- The ordered next-task list.

Do not copy routine command output into `NEXT.md`; record only the measured
result. If a test failed because the execution environment denied local
networking, distinguish that from a product failure and rerun in an
environment that permits the suite's socket/HTTP/ping tests.
