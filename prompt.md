# Instructions for iterative review of plan.sqlite3

## Initialization (at the start of each new context)

1. Read `CLAUDE.md` and `NEXT.md`.
2. Open `plan.sqlite3` — table `task` with columns: `id, namespace, name, type, internal, outofscope, status`.

## Workflow — one iteration

### Step 1 — Select the next item

- Take the first record where `status = ''` or `status = 'todo'` (skip `ignore` and `ported`).
- **Priority:** namespaces starting with `System` take precedence over others.

### Step 2 — Describe the item

Print:

| namespace | name | type | status |
|-----------|------|------|--------|
| <value> | <value> | <value> | <current value> |

Then briefly describe what the type does (look in `/rv/tmp/runtime/src/libraries/`), and give your opinion — whether it makes sense to port into sharp-runtime (e.g. reflection, threading, diagnostics etc. may be out of scope).

### Step 3 — Ask the user

> **Should I port this?**

- **Yes** → check whether the corresponding file already exists in sharp-runtime:
  - **Exists** → **do not mark as `ported` immediately** — the file must be reviewed against the full checklist in `CLAUDE.md` (API surface, doc-comments, SPDX, build, tests) as if it did not exist yet. Only set `status = 'ported'` after a successful review.
  - **Does not exist** → port it according to the checklist in `CLAUDE.md`, then set `status = 'ported'`.
- **No** → set `status = 'ignore'`, then ask:

> **Out of scope?**

  - **Yes** → set `outofscope = 1`.
  - **No** → set `outofscope = 0`.

### Step 4 — Save to DB and move to the next iteration

```sql
UPDATE task SET status = '...', outofscope = ..., updated_at = datetime('now') WHERE id = ...;
```

## Allowed `status` values

| Value | Meaning |
|-------|---------|
| `''`    | Not yet decided |
| `todo`  | Will be ported |
| `ported`| Done, satisfies the checklist |
| `ignore`| Skip (out of scope or irrelevant) |

> `in_progress` **does not exist** — porting happens directly, with no intermediate state.
