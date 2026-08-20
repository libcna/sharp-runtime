# Changelog

All notable changes to Sharp Runtime are documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project
adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html). While the major version is
0, the public API may change in any release — see the pre-1.0 note in
[`docs/releasing.md`](docs/releasing.md).

## [Unreleased]

## [0.1.0-alpha.1] — 2026-08-20

First tagged release. Sharp Runtime has been developed continuously since 2025-05-30; this tag
names a state of the tree rather than introducing new work, so the entries below describe what
the release contains, not what changed since a previous tag.

### Added

- **A practical subset of the .NET `System.*` libraries in C++23**, built as **41 independently
  selectable CMake components** with a validated module dependency graph
  (`scripts/validate_module_boundaries.py`, [`docs/ComponentCatalog.md`](docs/ComponentCatalog.md)).
- **Core value types** — strings, spans, dates, times, `Decimal`, `Int128`/`UInt128`, `Half`,
  `BFloat16`, `Guid`, exceptions, delegates and environment helpers.
- **Collections** — generic, immutable, object-model, concurrent, blocking and asynchronous, with
  fail-fast enumerators throughout.
- **Text and data** — `System::Text` encodings, `StringBuilder`, regular expressions,
  globalization, JSON (`Text.Json`), XML and XML LINQ.
- **I/O** — streams, files, compression, ZIP archives, hashing, isolated storage and
  `FileSystemWatcher`.
- **Networking** — `Uri`, sockets, HTTP with header parsing, MIME, WebSockets and network
  information.
- **Threading** — threads, tasks and continuations, channels, timers and synchronization
  primitives.
- **Numerics** plus non-encryption cryptography (hashes, HMAC, PBKDF2, secure random bytes).
- **`SharpRuntime/Version.hpp`** — the release identity generated from the build's single source
  of truth, exposing `SharpRuntime::getVersionString()` and the `SHARP_RUNTIME_VERSION_*` macros.

### Dependency pins

Every dependency of the **library** is pinned by this repository, so checking out this tag selects
them: `vendor/googletest` through its submodule gitlink
(`7e2c425db2c2e024b2807bfe6d386f4ff068d0d6`, `v1.14.0-223-g7e2c425d`, 2025-06-05), and
`vendor/nlohmann` (JSON for Modern C++ **3.10.4**), `vendor/tinyxml2` (**11.0.0**) and
`vendor/miniz` (**11.3.1**) by being checked in as source rather than fetched.

**Two things the tag does not select**, and they are named here rather than left to be discovered:

- **zlib** is a *system* dependency. `modules/io-compression` calls `find_package(ZLIB REQUIRED)`,
  so the `All` and `IO.Compression` builds take whatever the host provides (on Emscripten it is the
  `-sUSE_ZLIB=1` port instead). Verified against **zlib 1.3.1**.
- **tzdata** decides test *expectations*, not just behaviour. `TimeZoneInfo`'s negative-DST
  expectations are derived from the installed database precisely because a literal would go stale —
  see the 2026-08-17 note in `CLAUDE.md` — so the environment below is part of what "the gate is
  green" means.

This release was built and verified on:

    Debian GNU/Linux 13 (trixie)
    GCC 14.2.0, glibc 2.41, CMake 3.31.6, tzdata 2026b, zlib 1.3.1

Recording these is a stopgap: it documents the environment without enforcing it. A configure-time
check, or vendoring the remaining system dependency, is the intended replacement.

**Downstream** consumers have the mirror-image problem: CNA and mobile-eggbert consume this
repository as a sibling checkout with `add_subdirectory`, so their builds take whatever revision
this checkout is on. CNA's own changelog records that revision; with this tag it can name a tag
instead. See [`docs/releasing.md`](docs/releasing.md).

### Known limitations

- Pre-release quality: interfaces are expected to change before 1.0.
- **Permanent deviations** — reflection, GC, serialization infrastructure, P/Invoke, symmetric
  and asymmetric cryptography, X.509/TLS, and Unicode normalization are out of scope by explicit
  decision, not unfinished work. Every public index, length and count is a **UTF-8 storage byte**
  where .NET's is a UTF-16 code unit. See `CLAUDE.md` § *Parity philosophy* for the full list and
  the reasoning behind each.
- **Platform coverage is not uniform.** The verified build-and-test baseline is Linux/GCC;
  Windows, macOS and Emscripten builds compile but are not covered by the same test run, and
  several subsystems (sockets, `Process`, POSIX signals, `FileSystemWatcher`, network-interface
  enumeration) are available only on named platforms and throw
  `PlatformNotSupportedException` elsewhere. See `CLAUDE.md` § *Platform policy*.
- **`Decimal`, `Int128` and `UInt128` require compiler-provided native 128-bit integers**
  (`SHARP_RUNTIME_HAS_NATIVE_INT128`); they are absent on MSVC and 32-bit MinGW, which is a
  known, accepted and permanent boundary.

[Unreleased]: https://github.com/openeggbert/sharp-runtime/compare/v0.1.0-alpha.1...HEAD
[0.1.0-alpha.1]: https://github.com/openeggbert/sharp-runtime/releases/tag/v0.1.0-alpha.1
