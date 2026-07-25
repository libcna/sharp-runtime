<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Core ownership

`SharpRuntime::Core.Base` is the acyclic foundation used by the physical
runtime modules. `SharpRuntime::Core` is a compatibility umbrella over
`Core.Base`, `Console`, `Uri`, and `TimeZone`; internal modules must depend on
the narrow physical targets instead of the umbrella.

Public include spellings and `System::*` namespaces do not determine physical
ownership. Ownership follows dependency direction: a type remains in
`Core.Base` when moving it to a higher module would make foundational
primitives depend upward or create a static-library cycle.

## Foundation groups

| Group | Representative types | Ownership reason |
|---|---|---|
| Primitive/value types | `Boolean`, integer and floating-point types, `Decimal`, `Guid`, `DateOnly`, `DateTime`, `TimeOnly`, `TimeSpan` | Used throughout every higher layer. |
| Object and string model | `Object`, `ValueType`, `String`, `StringComparer`, arrays, tuples and nullable types | Defines the common public vocabulary of the runtime. |
| Memory/span model | `Span`, `ReadOnlySpan`, `Memory`, `ReadOnlyMemory`, `ArraySegment`, `SequencePosition` | Required by primitive parsing, text and buffers. |
| Base exceptions | `Exception`, `SystemException`, argument/format/overflow and platform exceptions | Higher modules expose these exceptions from public APIs. |
| Attributes and delegate helpers | `Attribute`, `Action`, `Func`, `EventHandler`, `Delegate`, `MulticastAction` | Shared header-level contracts with no valid higher owner. |
| Environment/runtime compatibility | `Environment`, `OperatingSystem`, `AppDomain`, GC/reflection stubs | Root `System` compatibility surface; implementations depend only on the foundation. |

## Extracted optional clusters

| Component | Owned surface | Direct dependency |
|---|---|---|
| `SharpRuntime::Console` | `Console*` headers and `Console.cpp` | `Core.Base` |
| `SharpRuntime::Uri` | `Uri*` headers, URI enums/exceptions and `Uri.cpp` | `Core.Base` |
| `SharpRuntime::TimeZone` | `TimeZone`, `TimeZoneInfo`, their rules and exceptions | `Core.Base` |

These components retain their original include paths. The compatibility
`SharpRuntime::Core` target enables all three for existing consumers.

## Intentional cross-namespace foundation types

| Include path owned by Core.Base | Reason |
|---|---|
| `System/Buffers/MemoryHandle.hpp` | `System::Memory` and `ReadOnlyMemory` expose it; making Core depend on Buffers would reverse the base dependency. |
| `System/Collections/Generic/IEqualityComparer.hpp` | Primitive and string equality infrastructure uses it; the full Collections component depends on Core. |
| `System/Diagnostics/Stopwatch.hpp` | `TimeProvider` and timing primitives need the lightweight clock abstraction without the process/debugging component. |
| `System/Globalization/CharUnicodeInfo.hpp`, `NumberStyles.hpp`, `UnicodeCategory.hpp` | Character and numeric primitives expose these types; the full Globalization component depends on Core. |
| `System/IO/IOException.hpp`, `DirectoryNotFoundException.hpp` | `Environment` throws `DirectoryNotFoundException`; moving them to IO creates `Core.Base -> IO -> Core.Base`. |
| `System/Numerics/BFloat16.hpp` | `BitConverter` exposes the value type while Numerics depends on Core and Collections. |
| `System/Security/Cryptography/CryptographicException.hpp` | Root conversion/parsing helpers and cryptography APIs need one base exception without reversing the cryptography dependency. |
| `System/Text/NormalizationForm.hpp` | Root string normalization APIs expose the enum while Text depends on Core and Buffers. |
| `System/Threading/LazyThreadSafetyMode.hpp` | `System::Lazy<T>` exposes the enum while Threading depends on Core. |

New cross-namespace ownership in `Core.Base` requires a concrete cycle or
public-foundation rationale in this table and must pass
`scripts/validate_module_boundaries.py`.
