# Changelog

All notable changes to Sharp Runtime are documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project
adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html). While the major version is
0, the public API may change in any release — see the pre-1.0 note in
[`docs/releasing.md`](docs/releasing.md).

## [Unreleased]

## [0.3.0] — 2026-11-14

A compatibility release driven by CNA's XNA content pipeline and its ported games. Where CNA
observed `BinaryReader`, `BinaryWriter`, `XmlReader` or `StringComparer::OrdinalIgnoreCase`
behaving differently from .NET Framework 4.0, this release follows .NET. Each such change is
recorded in a `docs/Migration-*.md` note, linked below.

**One public source spelling is removed:** `System::Version`'s `Major`, `Minor`, `Build` and
`Revision` data members are private. This is why the release is `0.3.0` and not `0.2.1`. The
installed package is `SameMinorVersion`-compatible, so it refuses `find_package(SharpRuntime 0.2)`.
Object layouts are unchanged: `BinaryReader`'s decoder state reuses the slot of the two members
it replaces, and a layout test pins it.

### Added

- `System::Version::getMajorProperty()`, `getMinorProperty()`, `getBuildProperty()` and
  `getRevisionProperty()`. The constructors, `CompareTo` and the comparison operators are
  `constexpr`; the comparisons come from a defaulted `operator<=>` and are `noexcept`.
- `System::IO::BinaryWriter` overloads that write what .NET writes for the same argument:
  `Write(charcs)`, `Write(char)`, `Write(const char*)`, `Write(const std::vector<bytecs>&)`,
  `Write(const bytecs (&)[N])`, `Write(const std::vector<charcs>&)`,
  `Write(const charcs*, intcs, intcs)` and `Write(const charcs (&)[N])`. Also
  `Write(const System::Decimal&)`, which exists only where `SHARP_RUNTIME_HAS_NATIVE_INT128` is
  1, as `BinaryReader::ReadDecimal` already does, so MSVC consumers keep compiling. All are
  non-virtual. No vtable, `sizeof` or existing signature changes
  ([`docs/Migration-BinaryWriterCharacterOverloads.md`](docs/Migration-BinaryWriterCharacterOverloads.md)).
- `System::IO::BinaryReader::Read()`, which reads one character or returns `-1`.
- `System::Xml::XmlReader::getBaseURIProperty()`. It returns the URI the reader was created from.
- `System::Index` and `System::Range` are `constexpr`, including `Range::GetOffsetAndLength`.

### Changed

- **Source break:** `System::Version::Major`, `Minor`, `Build` and `Revision` are private, as
  .NET's fields are. Read them through the new `get…Property()` accessors, and replace a write with
  a new `Version`. All eight read and write spellings are rejected by sites in
  `test/consumer/core_version_private_components_negative.cpp`. Measured for this release, neither
  CNA nor mobile-eggbert reads or writes these members.
- `System::Version` parses as .NET's `Version.TryParseComponent` does: each component may carry
  leading and trailing whitespace (0x09–0x0D, 0x20) and a `+` sign. A negative component is
  `ArgumentOutOfRangeException`. A non-integer or empty component is `FormatException`, and a
  component beyond `Int32` is `OverflowException`. Fewer than two or more than four components
  are `ArgumentException`: an empty string, and a `.` after the fourth component. `TryParse`
  leaves its result untouched when it fails.
- `System::IO::BinaryReader` decodes as .NET Framework 4.0's does, transliterated from its IL.
  Changes include:
  - `Read7BitEncodedInt` keeps the low four bits of a fifth byte instead of refusing it.
  - Invalid UTF-8 becomes `U+FFFD` grouped as .NET 4.0 groups it.
  - An unfinished sequence waits for the next read.
  - `ReadChar` on a surrogate pair throws after seeking back.
  - `ReadString` beyond the end of a seekable stream reads what is there before it throws
    `EndOfStreamException`.

  See [`docs/Migration-BinaryReaderNet4Decoding.md`](docs/Migration-BinaryReaderNet4Decoding.md).
- `System::IO::BinaryWriter` writes a `char16_t` argument as its UTF-8 bytes. It used to convert
  to `Write(intcs)` and write four bytes, so `Write(u'A')` wrote `41 00 00 00` instead of `41`.
  A string literal, a byte array and a `char16_t` array used to convert to `Write(bool)` and
  write `01`. Each now writes what .NET writes for that argument. Existing calls with such
  arguments therefore write different bytes, which is what CNA's `PacketWriter` needed
  ([`docs/Migration-BinaryWriterCharacterOverloads.md`](docs/Migration-BinaryWriterCharacterOverloads.md)).
- `System::StringComparer::OrdinalIgnoreCase` folds every UTF-16 code unit through its invariant
  simple upper-case mapping, as .NET 4 does, so `Ä` and `ä` compare equal. Before, it folded only
  ASCII. Hashes stay consistent with equality, and invalid UTF-8 keeps a byte-exact fallback
  ([`docs/Migration-TQ-078-OrdinalIgnoreCase.md`](docs/Migration-TQ-078-OrdinalIgnoreCase.md)).
- `System::Xml::XmlReader` refuses malformed UTF-8 with `XmlException` when it reads, in text and
  in attributes. Before, it accepted it, and only the writer refused it later. The refused forms
  are:
  - lone and truncated continuation bytes;
  - overlong forms;
  - UTF-16 surrogates;
  - values above U+10FFFF.

  See [`docs/Migration-XmlReaderUtf8.md`](docs/Migration-XmlReaderUtf8.md).
- `System::Xml::XmlReader` records the 1-based UTF-16 line and position of each element, end
  element, attribute and text token, as .NET's `IXmlLineInfo` does, and `XmlException` messages
  carry them. tinyxml2's own syntax errors keep column 0
  ([`docs/Migration-XmlReaderSourceLocations.md`](docs/Migration-XmlReaderSourceLocations.md)).
- `System::Range::GetHashCode()` combines through `HashCode::Combine` and is no longer `noexcept`.

### Fixed

- `System::Range::GetHashCode()` no longer has undefined behavior: it used to multiply a signed
  hash by 397, which could overflow.
- `System::IO::BinaryReader::PeekChar` on a stream that cannot seek returns `-1` without reading,
  as .NET Framework 4.0's does (`PeekChar` `IL_000d`-`001b`), instead of throwing
  `NotSupportedException`. A closed reader still throws `ObjectDisposedException` first
  ([`docs/Migration-BinaryReaderNet4Decoding.md`](docs/Migration-BinaryReaderNet4Decoding.md) §4).
- `System/Decimal.hpp` no longer triggers `-Wpedantic` ("ISO C++ does not support '__int128'") in
  consumers that include it, such as CNA's `Game.hpp` through `ContentManager.hpp`.
- `System::Buffers::SearchValues<T>` hashes through an always-copyable wrapper that holds one
  `std::hash<T>` for a usable specialization. Under GCC 15's
  libstdc++, `std::unordered_set<T>` asserts in its class body that `std::hash<T>` is
  copy-constructible, so instantiating `SearchValues<T>` for an equality-only `T` failed there,
  and the constructors' explanatory `static_assert` was never reached.

### Dependency pins

Unchanged from `0.2.0`:
- `vendor/googletest` is pinned by its submodule gitlink
  (`7e2c425db2c2e024b2807bfe6d386f4ff068d0d6`).
- `vendor/nlohmann` (**3.10.4**), `vendor/tinyxml2` (**11.0.0**) and `vendor/miniz` (**11.3.1**)
  are checked in as source.
- **zlib** remains a system dependency, and **tzdata** still decides two `TimeZoneInfo` test
  expectations, so the tag selects neither.
- The external test prerequisites are the original Yacht SOAP service and the XNA Game Studio
  samples tree.

### Known limitations

- Everything listed under `0.2.0` still holds.
- The verified gate is Linux with GCC 14. The `SearchValues<T>` repair for GCC 15's libstdc++ was
  verified only on a CI probe branch under GCC 15.2. The MSVC repair of `BinaryWriter.hpp` was
  checked by compiling with `SHARP_RUNTIME_HAS_NATIVE_INT128=0`, not with MSVC.
- The `System::Version` break has a negative consumer fixture and a measured downstream impact
  (none), but no `docs/Migration-*.md` note of its own. This entry is its record.

### Verification

- **18,282/18,282** tests pass across **42** executables, with 0 failed and 0 skipped. That is 50
  more tests than `0.2.0`'s 18,232. The run includes the two live `ServiceModel` tests, against a
  private copy of the original Yacht SOAP service, and the five `XnaRealFixtureTests`, against the
  official XNA samples.
- The module graph is **44 modules / 109 edges**. All **11** selective component configurations
  pass with their own tests. **56** negative consumer fixtures reject all **292** sites, including
  the eight new `System::Version` sites. The 5 test-only seams have one definition each.
- GCC 14.2.0 builds with **0 warnings and 0 errors**. Clang 19.1.7 builds all 231 production
  translation units under `-Werror` with **0 warnings**.
- The Doxygen no-regression gate passes with **2,674 warnings**, against the ceiling of 2,675.
- The installed package, built from the full graph, relocates. It refuses requests for `99.0` and
  `0.2`, and passes both consumer tests (`scripts/check_installed_package.py --mode full`).
- Verified on Debian GNU/Linux 13 with GCC 14.2.0, Clang 19.1.7, glibc 2.41, CMake 3.31.6, tzdata
  **2026c**, zlib 1.3.1, Doxygen 1.9.8 and Mono 6.12.0.199. As for `0.2.0`, `vendor/googletest`
  was a plain copy (`GOOGLETEST_VERSION` 1.16.0), not an initialised submodule, in the verifying
  checkout, so its revision was not checked against the gitlink.

## [0.2.0] — 2026-10-28

A compatibility release driven by the ported XNA demos and the CNA engine. Where a ported game
observed file I/O, XML or `Uri` behaving differently from .NET, this release follows .NET —
measured against genuine .NET Framework 4/XNA 4.0 under Wine and .NET 8 on Linux, with the
oracles retained in the `docs/` notes linked below. It also adds an installed CMake package.

**`FileStream`'s object layout changes in this release** (`sizeof` 576 → 24 on Linux x86-64
GCC 14), which also affects `IsolatedStorageFileStream`: rebuild every consumer from source and do
not mix headers or objects from `0.1.0` with this library. No public source spelling is removed.

### Added

- **An installed, relocatable CMake package.** `cmake --install <build> --component SharpRuntime`
  installs the configured component graph, and a consumer without the source checkout uses
  `find_package(SharpRuntime 0.2 CONFIG REQUIRED COMPONENTS …)` and `SharpRuntime::<Component>`.
  `SHARP_RUNTIME_INSTALL` is on for a standalone build and off under `add_subdirectory`
  ([`docs/InstalledPackage.md`](docs/InstalledPackage.md)). The package version is
  `SameMinorVersion`-compatible, so a request for `0.1` does not find this release.
- **`FileStream(path, mode, access, share)`**, the `FileShare` overload (CNA-DEMOS-ISSUE-152).
- **`Uri::getIsFileProperty()` and `Uri::getLocalPathProperty()`** — the unescaped UTF-8 local
  path of a file URI, without query or fragment (CNA-DEMOS-ISSUE-101,
  [details](docs/Migration-UriLocalPathAndFileExists.md)).
- **`XmlSerializer` serializes read/write properties**, registered with
  `detail::MakeProperty(name, getter, setter)`, for types such as XNA's `Color` that have
  properties only. Leaf conversion also takes `uint8_t` and `uint32_t`.
- **`StoragePaths::SetIsolatedStorageRootResolver` and `GetDefaultIsolatedStorageRootPath`** —
  hosting extensions, not .NET APIs, that defer the isolated-storage root to its first use, so a
  missing writable directory no longer stops an application at start-up
  ([details](docs/MuseumLazyStoragePolicy.md)).

### Changed

- **`FileStream` honours `FileShare` natively** — one `CreateFileW` handle on Windows, one
  descriptor with a non-blocking `flock` on POSIX, as .NET does on each. The short constructors
  now use .NET's default `FileShare::Read`; `File::Create` uses `FileShare::None`. A refused
  `Create`/`Truncate` open leaves the existing bytes intact; write-only `Open`/`OpenOrCreate` no
  longer truncate; `Append` cannot seek or truncate before its starting end; explicit
  `Flush`/`Close` report write errors; invalid enum values throw `ArgumentOutOfRangeException`
  before any file is opened ([details](docs/Migration-FileStream-NativeSharing.md)).
- **Embedded NUL in XML character data throws `ArgumentException`**, not `XmlException`, from the
  `XmlWriter` content-writing members and from LINQ to XML serialization, as .NET does
  (CNA-DEMOS-ISSUE-147, [details](docs/Migration-XmlWriter-NulException.md)).
- **`File::Delete` deletes files only**: a directory throws `UnauthorizedAccessException` instead
  of `IOException` and is preserved, a missing parent throws `DirectoryNotFoundException`, and a
  symlink to a directory is unlinked without touching its target
  ([details](docs/FileDeletion-XnaStorage.md)).
- **`Directory::CreateDirectory`** reports an unwritable parent as
  `UnauthorizedAccessException` and an overlong component as `PathTooLongException`, instead of
  `IOException` for every failure (CNA-DEMOS-ISSUE-010,
  [details](docs/DirectoryStorage-CnaIssue010.md)).
- **`XmlReader::CreateFromString` and `XmlDocument::LoadXml` reject a leading U+FEFF** in an
  already-decoded string, as .NET does; loading from a file still accepts a byte-order mark
  (CNA-DEMOS-ISSUE-148, [details](docs/XmlCharacterInputBoundary.md)).
- **`XmlSerializer` reads members in document order**, each member's first element only, as .NET
  does. The order was invisible while every member was a separate field; it matters once one
  property's setter overwrites another's state.

### Fixed

- **XML processing instructions parse at every legal position**, not only before every other
  node, so whatever the writers emit reads back. This is a narrow repair of the vendored tinyxml2
  parser ([details](docs/VendoredTinyXml2.md)); the XML declaration must still come first.
- `FileStream` keeps its real position after a short or empty read at end of file, so relative
  seeks and later writes work (CNA-DEMOS-ISSUE-056, [details](docs/Migration-FileStream-EOF.md)).
- `FileStream` distinguishes an existing directory from a missing file and keeps .NET's
  mode/access validation and `CreateNew` exception precedence (CNA-DEMOS-ISSUE-145,
  [details](docs/Migration-FileStream-Directories.md)).
- `FileStream` rejects empty and embedded-NUL names before touching the filesystem, so a
  prefix-named file can no longer be opened or truncated by accident
  ([details](docs/Migration-FileStream-InvalidPaths.md)), and reports overlong or inaccessible
  paths with their native error instead of as missing files
  ([details](docs/Migration-FileStream-StatusErrors.md)).
- `File::Exists` returns `false` for a path containing NUL instead of answering for the existing
  prefix; `Directory::CreateDirectory`, `GetFiles` and `GetDirectories` reject embedded NUL with
  `ArgumentException` and forward UTF-8 paths on Windows.
- `Uri` accepts empty and whitespace-only relative references when `UriKind` permits them,
  preserving `OriginalString` ([details](docs/Migration-Uri-EmptyRelative.md)).
- `TypeConverter` refusals name the converter and value types readably instead of as
  Itanium-mangled symbols.
- `Net.Sockets` builds under Emscripten with `-Werror` again (`-Wunused-but-set-variable` in
  `Socket.cpp`).
- Three tests no longer fail intermittently on a correct implementation: the random-byte
  chunk-boundary oracle, which rejected a legitimate `0xA5` byte once in 256 runs
  ([details](docs/RandomEntropyTestOracle.md)); the `FileSystemWatcher` filter-crossing rename
  test, which did not wait for the deferred `Deleted` callback; and the concurrent `Process`
  reader test, whose child could exit before any reader ran. None of the three checks was
  weakened, and no production code changed for them.

### Dependency pins

Unchanged from `0.1.0`: `vendor/googletest` through its submodule gitlink
(`7e2c425db2c2e024b2807bfe6d386f4ff068d0d6`), and `vendor/nlohmann` (**3.10.4**),
`vendor/tinyxml2` (**11.0.0**, with the processing-instruction repair above recorded in
[`docs/VendoredTinyXml2.md`](docs/VendoredTinyXml2.md)) and `vendor/miniz` (**11.3.1**) checked in
as source. **zlib** remains a system dependency and **tzdata** still decides two `TimeZoneInfo`
test expectations, so the tag selects neither. The external test prerequisites are those of
`0.1.0`: the original Yacht SOAP service and the XNA Game Studio samples tree.

### Known limitations

- Everything listed under `0.1.0` still holds.
- The verified gate is Linux. The Windows file I/O changes ran as a Windows-targeted MinGW build
  under Wine ([`docs/WindowsFileIoValidation.md`](docs/WindowsFileIoValidation.md)), not on
  native Windows. The changed sources were compile-checked for Emscripten only, and no macOS
  check is recorded for this release.
- On Unix, `FileShare` is advisory, as in .NET: only `FileShare::None` is exclusive, and a tool
  that does not take the lock is not stopped.

### Verification

- **18,232/18,232** tests pass across **42** executables (one more than `0.1.0`: `Storage`), with
  0 failed and 0 skipped — including the two live `ServiceModel` tests against a private copy of
  the original Yacht SOAP service and the five `XnaRealFixtureTests` against the official XNA
  samples.
- The module graph is **44 modules / 109 edges**; all **11** selective component configurations
  pass with their own tests; **55** negative consumer fixtures reject all **284** sites; the 5
  test-only seams have one definition each.
- GCC 14.2.0 builds with **0 warnings and 0 errors**, and Clang 19.1.7 builds all 231 production
  translation units under `-Werror` with **0 warnings**.
- The Doxygen no-regression gate passes with **2,675 warnings** against the ceiling of 2,675.
- The installed package, built from the full graph, relocates, refuses requests for `99.0` and
  `0.1`, and passes both consumer tests (`scripts/check_installed_package.py --mode full`).
- Verified on Debian GNU/Linux 13 with GCC 14.2.0, Clang 19.1.7, glibc 2.41, CMake 3.31.6,
  tzdata **2026c**, zlib 1.3.1, Doxygen 1.9.8 and Mono 6.12.0.199. As for `0.1.0`,
  `vendor/googletest` was a plain copy (`GOOGLETEST_VERSION` 1.16.0) rather than an initialised
  submodule in the verifying checkout, so its revision was not checked against the gitlink.

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

[Unreleased]: https://github.com/libcna/sharp-runtime/compare/v0.3.0...HEAD
[0.3.0]: https://github.com/libcna/sharp-runtime/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/libcna/sharp-runtime/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/libcna/sharp-runtime/compare/v0.1.0-beta.1...v0.1.0
[0.1.0-beta.1]: https://github.com/libcna/sharp-runtime/compare/v0.1.0-alpha.1...v0.1.0-beta.1
[0.1.0-alpha.1]: https://github.com/libcna/sharp-runtime/releases/tag/v0.1.0-alpha.1
