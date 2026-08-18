<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `Environment` can express a present-but-empty variable (ticket #2313)

*2026-08-18.* `GetEnvironmentVariable` returned `""` both for an **absent** variable and for one
**present with an empty value**, and `SetEnvironmentVariable(name, "")` **deleted**. .NET does
neither. Both are fixed.

Landed under `docs/StandingApprovals.md` **SA-10** with SA-2's five conditions. **Two public
signatures change, and one behaviour reverses.** Read §4 — it names two `cna` sites that change
meaning.

---

## 1. What changed

| | Was | Is |
|---|---|---|
| `GetEnvironmentVariable(name)` | `std::string` | **`std::optional<std::string>`** |
| ...for an absent variable | `""` | **`std::nullopt`** |
| ...for a present, empty variable | `""` — indistinguishable | **`std::optional("")`** |
| ...for a `User`/`Machine` target | `""` | **`std::nullopt`** |
| `SetEnvironmentVariable(name, value)` | `const std::string&` | **`const std::optional<std::string>&`** |
| `SetEnvironmentVariable(name, "")` | **deleted the variable** | **stores an empty value** |
| removing a variable | `SetEnvironmentVariable(name, "")` | **`SetEnvironmentVariable(name, std::nullopt)`** |

## 2. Why

`Environment.Variables.Unix.cs:49-57` is unambiguous:

```csharp
if (value == null) { s_environment.Remove(variable); }
else               { s_environment[variable] = value; }
```

Only `null` removes; everything else is stored, empty strings included. There is no
empty-to-`null` conversion on the read side either (`Environment.Variables.Unix.cs:19-33`), so
.NET's `string?` genuinely distinguishes the two states — and so does POSIX, whose `setenv(name,
"", 1)` stores an empty value that `getenv` returns as `""`.

The port's own doc-comment used to say *"Pass an empty string for value to remove the variable"*.
That stated **this port's** contract, not .NET's, and the deferral was blocked on not being able
to check which was right. It is checked now.

## 3. To migrate — first party

```cpp
// before
std::string v = Environment::GetEnvironmentVariable("HOME");
if (v.empty()) { /* absent */ }

// after — ask the question you actually meant
const auto v = Environment::GetEnvironmentVariable("HOME");
if (!v.has_value()) { /* absent */ }
if (v == std::optional<std::string>("")) { /* present, empty — a NEW state you can now see */ }

// or keep the old collapsed answer explicitly
const std::string collapsed = Environment::GetEnvironmentVariable("HOME").value_or("");

// removing a variable
Environment::SetEnvironmentVariable("X", std::nullopt);   // was: ("X", "")
```

**Every non-empty setter call is source-compatible**, because `std::optional<std::string>`
converts implicitly from `const char*` and `std::string`. The negative consumer fixture
`test/consumer/core_environment_nullable_negative.cpp` pins all four broken getter spellings and
the four surviving setter ones.

## 4. Downstream, measured — and one item needing the maintainer's attention

Measured in both local checkouts on 2026-08-18:

| | `GetEnvironmentVariable` sites | `SetEnvironmentVariable` sites |
|---|---|---|
| `cna` | **0** | **98** |
| `mobile-eggbert` | **0** | **0** |

The **return-type change breaks nothing downstream** — there are no `Get` sites at all. Of the 98
`Set` sites, 96 pass a non-empty literal and keep compiling and behaving identically.

**Two do not, and they change meaning silently:**

* `cna/modules/renderers/headless/examples/headless_coverage_gaps_test.cpp:105` —
  `SetEnvironmentVariable("CNA_HEADLESS_MODE", "")`
* `cna/modules/graphics/tests/Microsoft/Xna/Framework/Graphics/Texture2DTests.cpp:1447` —
  `SetEnvironmentVariable("FNA_GRAPHICS_JPEG_SAVE_QUALITY", "")`, whose own comment reads
  `// empty value deletes it`

Both intend **deletion** and will now **set an empty value**. Neither will fail to compile; both
will change what the surrounding test observes, and the second one documents in its own comment
that it is relying on the behaviour this ticket removed. `cna` **was not modified** — it may be
read but never edited without a per-action instruction — so this is filed as **#2366** and is the
one item here that needs the maintainer rather than a rebuild.

The one-line fix in each case is `std::nullopt` in place of `""`.
