# Changelog

All notable changes to Sharp Runtime are documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project
adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html). While the major version is
0, the public API may change in any release — see the pre-1.0 note in
[`docs/releasing.md`](docs/releasing.md).

## [Unreleased]

## [0.1.0] — 2026-10-10

The final 0.1.0 release. Compared with `0.1.0-beta.1` it adds the XML serialization and SOAP
service-model components that the ported XNA samples needed, carries the platform work done for
Windows/MSVC, Android, Emscripten and macOS on Apple silicon, and fixes the defects those ports
surfaced.

### Added

- **`Xml.Serialization`** — a new component providing the `XmlSerializer` subset the samples use:
  primitive lists, enums, inheritance, nested serialization and .NET's missing-member rule,
  checked against golden fixtures produced by Microsoft's own `XmlSerializer`; plus
  `IXmlSerializable`, Base64 and BinHex element content, and C# reference members modelled as
  `std::shared_ptr`.
- **`ServiceModel`** — a new component: SOAP 1.1 over HTTP (`BasicHttpBinding`) and
  `ServiceHost`, measured against the original Yacht game service.
- **XML** — a .NET-faithful `XmlWriter` text form; `XmlReader` navigation, namespaces, line
  information and settings.
- **`ComponentModel`** design-time conversion substrate, a real value store for
  `SerializationInfo`, and `ResourceManager.GetObject` with a binary resource route and an AOT
  fallback.
- **Formatting** — custom numeric format strings for `Single` and `Double`, general `TimeSpan`
  formatting and `TimeSpan` compound assignment.
- `File.Create`, `File.OpenRead`, `EventHandler::Share`, portable culture identity including the
  XNA-compatible Australian culture name, and a `Double` alias in `SharpRuntime`.
- **macOS** — a kqueue `FileSystemWatcher` backend, `NetworkInterface` enumeration, `Ping`,
  `Process` tree kill, `Environment.ProcessPath`, and `arc4random_buf` entropy for `Guid` and
  `RandomNumberGenerator`; the test suite builds with Apple clang and libc++.
- **Android** — package-private storage, host-scoped isolated storage and secure entropy on
  API 24. **Emscripten** — opt-in threads.

### Fixed

- **`TimeZoneInfo.BaseUtcOffset` and `StandardName` follow .NET's rule for a zone that changes
  its standard offset.** .NET takes them from the zone's latest standard-time period up to now;
  the port took the first one of the calendar year. The two agree for every zone whose standard
  offset holds all year, and tzdata 2026c made them disagree for Morocco, which moved to
  permanent +00 on 2026-09-20: `Africa/Casablanca` and `Africa/El_Aaiun` now report +00 as .NET
  does, instead of +01. Of the 487 zones installed on the verification host, those two are the
  only ones whose reported values change.
- Floating-point parsing uses the C locale on every platform's `strtod` fallback, not only on
  Apple's, so a comma-decimal process locale no longer changes results.
- **Windows** — `FileStream`, `File` and `XmlReader` take UTF-8 paths, including non-ASCII ones;
  `Environment` uses the wide Win32 calls and reads back what it writes, and a value cleared with
  `""` no longer stays readable; `Core.Base` links `bcrypt`; MSVC build and consumer-portability
  fixes.
- **Standard-library portability** — `Regex` behaves the same on libc++'s strict ECMAScript
  engine and refuses `\p{...}`; time-zone conversions before 1900 work on Darwin;
  `ReferenceEqualityComparer` hashes null to 0; `AggregateException`'s null-inner check no
  longer depends on argument order; public headers conform where GCC was lenient; `Console.hpp`
  no longer makes `System::Single` ambiguous.
- **Formatting and collections** — `String.Format` and `Int32.ToString` apply custom numeric
  formats and stop widening a float; invariant `Single` parsing; `List<T>` accepts element types
  without equality; `Dictionary` accepts keys that carry `GetHashCode` and enumerates in .NET's
  order; `DirectoryInfo` resolves path casing.
- **The vendored tinyxml2 now lives in `SharpRuntime::Vendor::tinyxml2`.** It kept upstream's
  `::tinyxml2` names, so a program carrying its own tinyxml2 linked two disagreeing definitions
  of the same classes, which crashed MeshCraft's export.

### Dependency pins

As for `0.1.0-alpha.1`: `vendor/googletest` through its submodule gitlink
(`7e2c425db2c2e024b2807bfe6d386f4ff068d0d6`), and `vendor/nlohmann` (**3.10.4**),
`vendor/tinyxml2` (**11.0.0**, now in its own namespace as above) and `vendor/miniz` (**11.3.1**)
checked in as source. **zlib** remains a system dependency and **tzdata** still decides two
`TimeZoneInfo` test expectations, so the tag selects neither.

New in this release are two **external test prerequisites**, neither part of this repository,
without which the zero-skip gate cannot pass: the two live `ServiceModel` tests need the
original Yacht SOAP service, which `scripts/run_component_tests_with_soap_fixture.py` starts as
a private copy under Mono on a loopback port, and the five `XnaRealFixtureTests` need the
official XNA Game Studio samples tree, located by `XNA_SAMPLES_ROOT`
(`README.md`, `docs/releasing.md`).

### Known limitations

- Everything listed under `0.1.0-alpha.1` still holds: the permanent deviations, UTF-8 storage
  byte indices, non-uniform platform coverage, and native-`Int128` platforms only.
- The verified gate is Linux: GCC for the build and the tests, Clang for the production warning
  gate. The macOS, Windows, Android and Emscripten work above came from those ports and is not
  covered by this release's gate.
- GitHub's *Full compatibility build* job has neither the SOAP service nor the XNA samples, so the
  live `ServiceModel` tests and the `XnaRealFixtureTests` skip there and the zero-skip rule fails
  that job even when everything else passes. The zero-skip gate is met locally, with both.

### Verification

- **18,136/18,136** tests pass across **41** executables, with 0 failed and 0 skipped — including
  the two live `ServiceModel` tests against a private copy of the original Yacht SOAP service and
  the five `XnaRealFixtureTests` against the official XNA samples.
- The module graph is **44 modules / 109 edges**; all **11** selective component configurations
  pass with their own tests; **55** negative consumer fixtures reject all **284** sites; the 5
  test-only seams have one definition each.
- GCC 14.2.0 builds with **0 warnings and 0 errors**, and Clang 19.1.7 builds all 231 production
  translation units under `-Werror` with **0 warnings**.
- The Doxygen no-regression gate passes with **2,674 warnings** against the ceiling of 2,675.
- Verified on Debian GNU/Linux 13 with GCC 14.2.0, Clang 19.1.7, glibc 2.41, CMake 3.31.6,
  tzdata **2026c**, zlib 1.3.1, Doxygen 1.9.8 and Mono 6.12.0.199. In the verifying checkout
  `vendor/googletest` was a plain copy (`GOOGLETEST_VERSION` 1.16.0) rather than an initialised
  submodule, so its revision was not checked against the gitlink.

## [0.1.0-beta.1] — 2026-08-22

Sharp Runtime is complete for its currently declared practical subset and enters maintenance
mode with this release. Compared with `0.1.0-alpha.1`, this beta closes the final internal audit,
DateTime-kind propagation, and cross-compiler build-hygiene work without expanding the declared
.NET compatibility scope.

### Fixed

- Reconciled all 364 historical audit findings against the implementation: **343 remediated**,
  **19 accepted deviations**, **2 false positives**, and **0 open**.
- Closed the remaining lifetime, arithmetic, undefined-behaviour, temporary-path, validator, and
  selective-component coverage defects, with permanent regression tests.
- Completed `DateTimeKind` propagation through `DateTime`, `DateTimeOffset`, `TimeZoneInfo`,
  `TimeZone`, and XML conversion while preserving the documented practical-subset boundaries.
- Restored warning-clean Clang builds, made the production Clang build a local and CI gate, and
  retained the warning-clean GCC build.
- Made Windows entropy chunking portable to i686 and restored hosted-runner component CI.

### Verification

- **17,840/17,840** tests pass across 38 executables, with 0 failed and 0 skipped.
- The module graph is **41 modules / 96 edges**; all 10 selective component configurations pass.
- GCC and Clang production builds complete with **0 warnings and 0 errors** under `-Werror`.
- The Doxygen no-regression gate passes at the documented baseline of **2,675 warnings**.
- Verified on Debian GNU/Linux 13 with GCC 14.2.0, glibc 2.41, CMake 3.31.6, tzdata 2026b,
  and zlib 1.3.1.

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

[Unreleased]: https://github.com/libcna/sharp-runtime/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/libcna/sharp-runtime/compare/v0.1.0-beta.1...v0.1.0
[0.1.0-beta.1]: https://github.com/libcna/sharp-runtime/compare/v0.1.0-alpha.1...v0.1.0-beta.1
[0.1.0-alpha.1]: https://github.com/libcna/sharp-runtime/releases/tag/v0.1.0-alpha.1
