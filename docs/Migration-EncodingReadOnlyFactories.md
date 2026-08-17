<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — the factory encodings are read-only (ticket #2013)

*2026-08-17.* `System::Text::Encoding::UTF8()` and its six siblings still return one shared
object each, but that object now **rejects fallback mutation**. Setting a decoder or encoder
fallback on one throws `System::InvalidOperationException("Instance is read-only.")`.

Landed under `docs/StandingApprovals.md` SA-5 for the behaviour and SA-3 for the layout.

---

## 1. What was wrong

Each factory returned one shared, **mutable** object. A caller that installed a fallback on
`Encoding::UTF8()` changed what **every other caller in the process** decoded, and the audit
recorded a TSan read/write race between the setter and a concurrent decode. Structurally this is
CCF-009 — a process-wide singleton with publicly mutable state and no ownership boundary — the
same shape `Random::Shared` and `Guid::NewGuid` had.

.NET solves it by making exactly those instances read-only: `ASCIIEncoding.s_default` and its
siblings carry `_isReadOnly`, and both fallback setters open with

```csharp
if (this.IsReadOnly) throw new InvalidOperationException(SR.InvalidOperation_ReadOnly);
```

(`Encoding.cs:485-497`; `SR.InvalidOperation_ReadOnly` is `"Instance is read-only."`).

## 2. What to change

```cpp
Encoding::UTF8()->setDecoderFallbackProperty(myFallback);   // was: silently global. now: throws
auto mine = std::make_shared<UTF8Encoding>();               // now: construct your own
mine->setDecoderFallbackProperty(myFallback);
```

Reading is unaffected: `GetBytes`, `GetString`, `GetByteCount`, `GetCharCount`, the properties and
the preamble all behave exactly as before on the factory instances.

`getIsReadOnlyProperty()` is new and reports which kind of instance you hold — the counterpart of
.NET's `Encoding.IsReadOnly`.

### 2.1 The read-only test runs before the null test

`setDecoderFallbackProperty(nullptr)` on a factory instance reports the **read-only** violation,
not the null one. That order is .NET's (`Encoding.cs:490-494`) and is pinned by test.

## 3. `Clone()` is deliberately absent

.NET has `Encoding.Clone()`, which returns a **writable** copy, and it would be the natural
migration path. It is **not** added here: it would be a new virtual on a public base class, which
`docs/StandingApprovals.md` SA-3 explicitly excludes from standing approval. It would also buy
little — .NET needs `Clone` because a caller may only have an `Encoding` reference, whereas here
every concrete encoding is publicly constructible. Recorded so the omission reads as a decision
rather than an oversight.

## 4. The layout change, and who must rebuild

| Type | `sizeof` before | `sizeof` after |
|---|---:|---:|
| `Encoding` | 40 | **48** |
| `UTF8Encoding`, `ASCIIEncoding`, `Latin1Encoding` | 40 | **48** |
| `UnicodeEncoding`, `UTF32Encoding` | 48 | **48 — unchanged** |

The vtable pointer and two `shared_ptr`s used all 40 bytes, so the new `bool` costs a whole
aligned slot. The endian-aware encodings did **not** grow: their own two flags used to sit in
their own tail padding and now sit in the base's, which the new slot created. A pin written as
"base + one pointer" would have been wrong in both directions, so the pin compares against shadow
structs instead.

**Every consumer must be fully recompiled** — a `sizeof` change across a stale-header boundary is
an ODR violation with no diagnostic.

## 5. Downstream, measured

Per SA-2 condition 5, both consumer checkouts were searched. `cna` names none of these types.
`mobile-eggbert` uses `Encoding::UTF8()->GetString` and `->GetBytes` at `Worlds.cpp:304` and
`:325` — **reads only, no fallback setter**, so its behaviour is unchanged. It must be rebuilt,
like every consumer. Neither repository was modified.
