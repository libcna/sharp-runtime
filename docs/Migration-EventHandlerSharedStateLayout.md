<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# EventHandler shared state: rebuild every consumer

GSP-J1 added `EventHandler<TEventArgs>::Share()` so copies of a value-mapped owner can
intentionally share one subscriber list. Its implementation moved the vector of handlers,
subscription token and replay hook into a heap-allocated `State` behind `std::shared_ptr`.
Copies remain independent until `Share()` is called; no existing source spelling was removed.

This changed a **public C++ object layout**. On the verified Linux x86-64 GCC 14.2.0 /
libstdc++ baseline:

| Type | Before GSP-J1 | After GSP-J1 | Alignment |
|---|---:|---:|---:|
| `System::EventHandler<System::EventArgs>` | 64 bytes | 16 bytes | 8 bytes |
| `System::EventHandler<System::Timers::ElapsedEventArgs>` | 64 bytes | 16 bytes | 8 bytes |
| `System::Timers::Timer` | 112 bytes | 64 bytes | 8 bytes |

The before/after `EventHandler` sizes were measured by compiling the template from `88f6b11f^`
and from GSP-J1. The `Timer` before size was already pinned by #2155; its after size was
measured on GSP-J1 and is now pinned together with the `EventHandler` size in the component
tests. The `Timer` base class and vtable did not change in GSP-J1; the 48-byte decrease is
exactly the decrease of its public `Elapsed` member.

**Rebuild all translation units and static libraries that include `EventHandler.hpp` or store
`EventHandler<T>` by value.** Mixing objects built against old and new definitions is an ODR
violation: identical type and member names would denote different offsets and sizes. Reusing a
pre-GSP-J1 consumer build directory without recompiling its affected targets is unsafe.

Measured source impact on the local downstream checkouts on 2026-09-30:

- `libcna/cna`: `rg -l 'System::EventHandler\s*<' modules --glob '*.{hpp,h,cpp,cc,cxx}'`
  finds 72 files: 36 public headers, 25 other sources and 11 tests. They include runtime,
  audio, graphics, storage, phone and gamer-services owners. This is a rebuild obligation;
  the new `Share()` spelling is only needed by an owner that deliberately wants shared event
  identity.
- `openeggbert/mobile-eggbert`: no direct spelling in this search. It consumes CNA and
  therefore inherits CNA's rebuild obligation transitively.
- Within sharp-runtime itself, `System::Timers::Timer` is the only production class found
  storing `System::EventHandler<T>` directly. `XObject` and the observable collections use
  other handler representations and are not layout-affected by this change.

The downstream trees were inspected read-only. This migration note does not alter them.
No per-spelling negative consumer fixture is needed: `Share()` is additive and the previous
public spelling remains valid. The compatibility requirement is a complete rebuild, plus
the same source-level behavior for owners that never call `Share()`.

## Consumer rebuild, 2026-09-30

The `WindowsPhoneSpeedyBlupi` target in mobile-eggbert was configured in this
sharp-runtime worktree's `build-consumer/`, with
`CNA_SHARP_RUNTIME_ROOT` pointing at this worktree. A fresh, two-job build compiled
and linked **702 steps**, including CNA's runtime, content, gamer-services and
the mobile-eggbert executable. An incremental check after CNA reached clean HEAD
`39231e4ed` reported `ninja: no work to do`; mobile-eggbert was at `cb9ca51`.
Neither downstream source tree was edited by this verification. The build had
no errors; its 20 compiler warnings came from CNA's vendored Draco headers and
the system STL while building that vendor code. The independent sharp-runtime
GCC and Clang warning gates reported zero compiler warnings.
