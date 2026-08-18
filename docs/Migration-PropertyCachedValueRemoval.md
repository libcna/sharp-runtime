<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `Property<T>` loses its vestigial cache (ticket #2246)

*2026-08-18.* `SharpRuntime::Experimental::Property<T>` carried a `T cachedValue` member that
nothing read and nothing wrote. It is gone.

Landed under `docs/StandingApprovals.md` **SA-3** (a private data member removed from a public
type; no vtable, mangled-symbol, signature or `noexcept` change; before/after `sizeof` pinned by a
layout test; full gate green). **Consumers must be recompiled** — the object shrank. No source
change is needed, and one source *restriction* is lifted.

---

## 1. What changed

| | Was | Is |
|---|---|---|
| `sizeof(Property<int>)` | **72** | **64** |
| `sizeof(Property<std::string>)` | **96** | **64** |
| `sizeof(Property<T>)` in general | two callables **+ a `T`** + padding | exactly **two callables** |
| `T` must be default-constructible | **yes** | **no** |
| every getter, setter, conversion, assignment | — | **unchanged** |

`ReadOnlyProperty<T>` adds nothing of its own and moved with the base, as it always has.

## 2. Why the member was wrong, not merely unused

The value a property holds lives in whatever storage the supplied getter and setter close over. A
cache inside the wrapper can therefore only ever **disagree** with it — which is exactly what
SR-AUD-179 measured and #2244 recorded. #2244 stopped short of removing it because that changes
the object, and left the request as #2246.

## 3. The restriction that is lifted

Every constructor default-initialised the member, so `T` had to be default-constructible — a
requirement a getter/setter wrapper never needed and never used. This now compiles:

```cpp
struct NoDefaultCtor { explicit NoDefaultCtor(int v); int value; };

NoDefaultCtor storage{7};
Property<NoDefaultCtor> p([&] { return storage; }, [&](const NoDefaultCtor& v) { storage = v; });
```

A test asserts it, because a `sizeof` pin cannot express it.

## 4. To migrate

Rebuild. Nothing to edit: the member was private and unreachable, so no caller can have named it.

## 5. Downstream, measured

Neither `cna` nor `mobile-eggbert` references `SharpRuntime::Experimental::Property` —
**zero sites in both**. Neither repository was modified. The full-rebuild requirement is recorded
here for any future consumer.
