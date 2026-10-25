<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Museum B11: deferred host storage policy (2026-10-23)

CNA's `StorageDevice::SetAppNameEXT` used to create its root immediately to pass a
fixed isolated-storage override here. A missing writable directory therefore prevented
application startup before any storage operation. Removing that override would silently
lose per-application isolation when System.IO isolated storage is accessed first.

`StoragePaths::SetIsolatedStorageRootResolver` adds a deferred hosting policy. Registration
performs no filesystem access; `GetIsolatedStorageRoot` copies the policy under its lock,
invokes it outside the lock, and creates the returned directory. Errors propagate on access
and are not cached. A fixed override and a resolver replace each other. Clearing either
restores the platform default. The caller owns synchronization of captured state and must
unregister before unloading callback code, after outstanding calls finish.

`GetDefaultIsolatedStorageRootPath` exposes the existing platform path selection without
creation or overrides. In particular, CNA's Android policy can derive the package-private
directory without recursively calling itself or clearing its installed policy. Platform
path selection is unchanged. These two additive static methods are hosting extensions,
not .NET compatibility APIs. Existing symbols, object layout and fixed-override behavior
are unchanged. Consumers calling the additions must relink against this revision.

Validation in the existing `build/`, one compiler job, shared `/rv/cnaccache`,
`CCACHE_BASEDIR=/rv`, MemoryHigh4GiB/MemoryMax6GiB/no swap:

- Unmodified baseline build: clean. Initial full test attempt stopped on five absent
  authentic XNA fixture files, not an implementation failure; no skips were accepted.
- Incremental repaired build: clean, no warnings/errors. New Storage executable: 6/6.
- Full gate with existing `build-probe/cna-repair-xna-samples` as `XNA_SAMPLES_ROOT`
  and the original Yacht fixture on a private loopback port: **18,201/18,201** across
  42 executables, zero failures or skips. IO.IsolatedStorage remains 63/63.
- Module boundary validation: 44 physical modules / 109 edges, unchanged and valid.
- CNA consumer native storage regressions: five eager-root cases fail before, pass
  after. Its unrelated inherited path-error tests remain red; this is not a full CNA pass.
- Linux runtime only; Android/browser policy code was reviewed, not runtime-validated.

Full test command (no compilation inside this runner):

```sh
XNA_SAMPLES_ROOT="$PWD/build-probe/cna-repair-xna-samples" \
SHARP_RUNTIME_BUILD_JOBS=1 CMAKE_BUILD_PARALLEL_LEVEL=1 \
python3 scripts/run_component_tests_with_soap_fixture.py \
  /rv/tmp/samples/SAMPLE-071-Yacht_4_0/xna4-build/bin build
```

Durable campaign record: CNA's `CNA_MUSEUM_REPAIR_HANDOFF.md`, removed on 2026-10-25 and kept in CNA's Git history
(`git show 9d49828f4:CNA_MUSEUM_REPAIR_HANDOFF.md`). Detailed local evidence:
cna-demos `build/linux-full/reports/repair-museum-storage-sharp-*`. Publish this dependency
before the CNA caller; do not repeat the previously reported unpublished-dependency gap.
