<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `BinaryData::ToString()` decodes UTF-8 (ticket #2106)

*2026-08-17.* `BinaryData::ToString()` copied its byte range **verbatim**, so it could return a
`std::string` that is not valid UTF-8. .NET's is `Encoding.UTF8.GetString(_bytes.Span)` and
always returns valid text.

Landed under `docs/StandingApprovals.md` SA-5. No public signature, layout, vtable or `noexcept`
change.

---

## 1. What changed

| Bytes | Was | Is |
|---|---|---|
| `FF` | `FF` — not valid UTF-8 | `EF BF BD` (U+FFFD) |
| `E2 82` (truncated U+20AC) | `E2 82` | `EF BF BD EF BF BD` — one per ill-formed **byte** |
| `FF 'a' E2 82 AC` | unchanged | `EF BF BD 'a' E2 82 AC` |
| any well-formed UTF-8 | — | **byte-identical** |

One replacement per ill-formed byte, because the decoder resumes one byte later. That is what
stops a truncated sequence swallowing the character that follows it.

## 2. The second half is a deliberate permanent deviation

The finding has two halves, and they are answered **differently**.

.NET's `BinaryData(byte[])` **wraps** its argument — `_bytes = data`, `BinaryData.cs:60-63` — so
a later mutation of the caller's array is observable through the `BinaryData`. This port
**copies**, and will keep copying.

Reproducing the aliasing here would mean holding a reference to storage this object does not own,
in a language with no GC to keep it alive. That is precisely the borrowed-reference defect
CCF-019 exists to remove, and that #1959, #2029, #2066, #2088, #2096 and #2134 have spent this
programme removing. A managed alias is safe because the runtime keeps the array reachable; a C++
one is a use-after-free waiting for the caller's vector to leave scope.

So it is a deviation with a reason rather than an unfinished repair, and it is pinned in that
direction.

## 3. The UTF-8 decoder moved to `Core.Base`

`modules/io` does not depend on `Text`, where the one shared UTF-8 scalar decode lived (ticket
#2014). The options were a **sixth** copy of that decode, a new **public** component edge from
`io` to `Text`, or moving the single definition somewhere everything already depends on.

It now lives at `modules/core/include/System/detail/Utf8Scalar.hpp`, and
`System::Text::detail::Utf8Scalar.hpp` re-exports the two names, so **every existing caller is
unchanged**. The moved bodies are byte-identical to the originals, verified by diff. The module
graph is unchanged: 41 modules, 92 edges.

## 4. To migrate

If you fed `BinaryData` well-formed UTF-8 — which every call site in this repository does — the
output is byte-identical.

If you were using `ToString()` to retrieve arbitrary bytes, that was never what it meant. Use
`ToArray()`, which is unchanged and returns the bytes.

## 5. Downstream, measured

Neither `cna` nor `mobile-eggbert` references `System::BinaryData` — **zero sites in both**.
`cna` matches the substring three times, all in vendored Vulkan headers
(`pVendorBinaryData`, `vkGetShaderBinaryDataEXT`), which are unrelated. Neither repository was
modified.
