# Sharp Runtime

**Sharp Runtime** is a C++ reimplementation of a small C#/.NET runtime subset (mainly used by CNA and game ports).

The goal of this project is to provide a lightweight, .NET-inspired foundation layer for C++ projects, with a focus on:

* familiar API design (`System::*`-like namespaces)
* clean and modern C++ implementation
* compatibility with higher-level frameworks (e.g. CNA)

> ⚠️ This is **not** a full .NET runtime or CLR implementation.
> It is a pragmatic subset designed for use in native C++ applications.

---

# License

This project is licensed under the **MIT License** — see the [LICENSE](LICENSE) file for details.

## Attribution

Sharp Runtime is **partly derived from the .NET runtime**
([dotnet/runtime](https://github.com/dotnet/runtime), MIT License, © .NET Foundation and Contributors).

Specifically:
- The **public API design** of `System::*` types — class names, method signatures, namespace structure, and enum values — is based on the .NET standard library.
- Some **algorithmic implementations** (number formatting, date/time arithmetic, Unicode handling, etc.) are informed by or translated from the dotnet/runtime source code.
- The **C++ implementation** (headers, `.cpp` bodies, CMake build, tests) is original work by Robert Vokac and contributors.

The dotnet/runtime source is available at https://github.com/dotnet/runtime under the MIT License.

## Permanent deviations from .NET

Some .NET features are intentionally out of scope and will never be ported — reflection
(`System.Type`, `Activator`, `Enum.GetNames`/`GetValues`), the GC, delegates beyond
`std::function`, serialization infrastructure, P/Invoke, and full symmetric/asymmetric
cryptography, X.509 certificates, and TLS (`SslStream`). See **[CLAUDE.md](CLAUDE.md)**
("Parity philosophy" section) for the complete list and the reasoning behind each one.

---

# 🚀 Goals

* Recreate useful parts of `.NET` API in idiomatic C++
* Provide building blocks such as:

    * exceptions
    * events / delegates
    * basic system types
* Serve as a foundation for higher-level frameworks (e.g. CNA)
* Keep the codebase simple, readable, and well-documented

---

# 🛠️ Build

```bash
git submodule update --init --recursive
cmake -S . -B build -DSHARP_RUNTIME_BUILD_TESTS=ON
cmake --build build --parallel 4
./build/SharpRuntimeTests
```

A library-only build (no test binary) is also supported:

```bash
cmake -S . -B build-no-tests -DSHARP_RUNTIME_BUILD_TESTS=OFF
cmake --build build-no-tests --parallel 4
```

---

# 🗂️ Tracking: `plan.sqlite3`

Porting progress and stabilization work are tracked in a local, git-ignored SQLite database,
`plan.sqlite3`, with **two separate tables that must not be confused with each other**:

## `task` — .NET type classification

One row per type from [dotnet/runtime](https://github.com/dotnet/runtime)'s public surface. Tracks
*whether and how* a given `System.*` type has been dealt with.

| `task.status` | Meaning |
|---|---|
| `''` / `todo` | Not yet classified or ported. |
| `ported` | Implemented, tested, meets the full porting checklist. |
| `ignore` / `ignored` | Permanently out of scope (both values exist — `ignored` predates the current workflow and is left as-is, not "fixed" to `ignore`). |
| `tobedecided` | Genuinely ambiguous; needs a human architecture decision, not a guess. |

`in_progress` is **not** a valid `task.status` value.

## `ticket` — stabilization work queue

One row per concrete stabilization task (documentation fixes, correctness audits, platform checks,
test coverage, etc.) that isn't itself "port a .NET type." Independent of `task`.

| `ticket.status` | Meaning |
|---|---|
| `todo` | Not started. |
| `doing` | Actively being worked. |
| `done` | Complete — acceptance criteria met, build clean, tests passing/updated. |
| `blocked` | Can't proceed for an external/technical reason (recorded in `notes`). |
| `needs_user` | Requires a human decision that can't be made safely alone (recorded in `notes`). |
| `wontfix` | Deliberately not doing this (recorded in `notes`, e.g. permanent out-of-scope). |

`ticket.status` and `task.status` are **different systems with different value sets** — a ticket is
never `ported`, and a task is never `doing`.

```bash
# Next ticket to work on
sqlite3 plan.sqlite3 "SELECT ticket_no, priority, title FROM ticket WHERE status='todo' ORDER BY priority, ticket_no LIMIT 1;"

# Overall progress
sqlite3 plan.sqlite3 "SELECT status, priority, COUNT(*) FROM ticket GROUP BY status, priority ORDER BY priority, status;"
```

---

# 📊 Implementation Status Convention

Doxygen `@note Status: ...` comments on individual classes/methods are a **secondary, human-readable
hint**, not the source of truth — `plan.sqlite3`'s `task` table is authoritative for whether a type
counts as ported. These statuses are not enforced by the compiler.

## Status values

* **Todo** — not implemented yet
* **Stub** — skeleton only, returns placeholder or fails
* **Partial** — partially implemented; compiles and mostly works but has known, documented gaps
* **Implemented** — functionally complete
* **Verified** — validated against expected .NET behavior

---

# 📝 Comment Format

Each class or function may include a status note:

```cpp
/**
 * @note Status: Partial
 */
```

Full example:

```cpp
/**
 * @brief Provides 2D sprite rendering functionality.
 *
 * @note Status: Partial
 */
class SpriteBatch
{
public:
    /**
     * @brief Begins a sprite drawing batch.
     *
     * @note Status: Implemented
     */
    void Begin();

    /**
     * @brief Draws a texture at the specified position.
     *
     * @note Status: Partial
     */
    void Draw(Texture2D& texture, Vector2 position, Color color);

    /**
     * @brief Ends a sprite drawing batch.
     *
     * @note Status: Todo
     */
    void End();
};
```

---

# 🧠 Design Philosophy

* Prefer clarity over completeness
* Avoid unnecessary complexity
* Keep APIs close to .NET where it makes sense
* Use modern C++ (RAII, strong typing, clear ownership)

---

# ⚠️ Scope

Sharp Runtime intentionally **does not aim to implement:**

* the CLR (Common Language Runtime)
* JIT compilation
* full .NET standard compatibility

Instead, it focuses on a **practical subset** useful for native development.

---

# 🔗 Related Projects

* CNA — C++ reimplementation of XNA 4.0 (built on top of this library)

