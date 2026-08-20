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
