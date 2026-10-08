<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# sharp-runtime on macOS (Apple silicon)

Measured on a Mac mini M4 (arm64), macOS 27.0.1, Xcode 27.0, Apple clang 21, CMake 4.4,
during the CNA Apple-silicon campaign (CNA `plans/plan_apple_m4.md`, task IDs `AM4-*`).
The CI gate in `.github/workflows/components.yml` is Linux/GCC only; this page is the macOS
counterpart and records what differs.

## Build and run

```bash
cmake -S . -B ~/Desktop/build/sharp-runtime-macos-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.3 \
  -DSHARP_RUNTIME_COMPONENTS=All \
  -DSHARP_RUNTIME_BUILD_TESTS=ON \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
cmake --build ~/Desktop/build/sharp-runtime-macos-debug --target SharpRuntimeTests --parallel 3
SHARP_RUNTIME_ALLOW_SKIPS=1 scripts/run_component_tests.sh ~/Desktop/build/sharp-runtime-macos-debug
```

The runner fails on any skipped test by default, which is the CI gate's rule. On macOS 25 tests
skip by design (listed under "Known gaps" below), so `SHARP_RUNTIME_ALLOW_SKIPS=1` lets skips be
reported without failing the run; failures still fail it.

`13.3` is CNA's macOS floor. At that deployment target Apple's libc++ has no usable
floating-point `std::from_chars`, so `Double`/`Single` parsing runs through
`SharpRuntime::PortableFromCharsFloat` (the `strtod` fallback) -- the configuration CNA ships,
and therefore the one worth testing. Configured without a deployment target, the build targets
the host OS and uses the native overload instead.

The whole test build compiles with `-Werror` under Apple clang. The linker prints
`ignoring duplicate libraries` notes: CMake repeats static libraries on purpose for link
cycles, and Apple's linker reports the repetition. They are not compiler warnings.

## What differs from Linux, and why

| Area | macOS behaviour | Where |
|---|---|---|
| CSPRNG | `arc4random_buf()` (no public `getentropy` on iOS; macOS declares it only in `<sys/random.h>`) | `Guid.cpp`, `RandomNumberGenerator.cpp` |
| Floating `from_chars` | libc++ declares it at every target but makes it strictly unavailable below 26.0; `FromCharsFloat` consults `_LIBCPP_AVAILABILITY_HAS_FROM_CHARS_FLOATING_POINT` | `PortableFromChars.hpp` |
| `Environment.ProcessPath` | `_NSGetExecutablePath` + `realpath`, as .NET's `minipal_getexepath` | `Environment.cpp` |
| Dates before 1900 | Darwin's `timegm`/`mktime` refuse them; civil-date arithmetic and a `localtime_r` fallback replace them | `TimeZonePosixSupport.hpp` |
| `Process.Kill(true)` tree walk | sysctl `KERN_PROC_ALL` parent links instead of `/proc` | `Process.cpp` |
| `Ping` | Darwin's ICMP datagram socket returns the IPv4 header; it is skipped | `Ping.cpp` |
| `NetworkInterface` | `getifaddrs` with `AF_LINK`/`sockaddr_dl`; loopback found by `IFF_LOOPBACK` (`lo0`) | `NetworkInterface.cpp` |
| `FileSystemWatcher` | kqueue (`EVFILT_VNODE` on the directory and on each entry, `EVFILT_USER` to stop) instead of inotify | `FileSystemWatcher.cpp` |
| `Regex` | libc++'s ECMAScript parser is strict: lone `]`, `}` and non-quantifier `{` are escaped (literals in .NET); resumed searches state `match_not_bol`/`match_not_bow` because libc++ ignores `match_prev_avail` for `^` | `Regex.hpp` |
| IPv6 sockets | dual-mode by default (`net.inet6.ip6.v6only = 0`) | tests ask the socket |
| Object sizes | libc++'s `std::mutex`/`std::function`/`std::string` differ from libstdc++, so layout pins record both via `layoutPin(reference, appleLibcxx)` | `*Tests.cpp` |

## Known gaps on macOS

- `FileSystemWatcher` (AM4-055, hardened by AM4-101) watches through kqueue: the directory's
  vnode for added, removed and renamed entries (re-listed and diffed; a vanished inode under a new
  name is one `Renamed`, and a rename onto an existing name is that `Renamed` alone, as on Linux)
  and each regular file's or subdirectory's vnode for content and attribute changes.
  - kqueue needs one descriptor per watched file. All watchers in a process together hold at most
    half of `RLIMIT_NOFILE`'s soft limit (256 for a process launchd starts); entries beyond that
    still raise `Created`/`Deleted`/`Renamed` but not `Changed`, and `Error` reports them with an
    `InternalBufferOverflowException`. Raise the limit (`setrlimit`) to watch larger directories.
  - FIFOs, sockets, devices and symbolic links are never opened (opening a FIFO blocks), so they
    raise no `Changed`; a symbolic link is not followed.
  - A watched directory that can no longer be listed (removed, renamed away, unreadable) raises
    `Error` once instead of reporting its entries as deleted.
  - `NotifyFilters.LastAccess` is not observable (kqueue reports no reads), and
    `IncludeSubdirectories` is unimplemented on every platform.
- The `HashCode` per-process-seed and `PosixSignal` re-exec tests are `#ifdef __linux__`
  (they re-exec through `/proc/self/exe`), so they do not run on macOS.
- `PortableFromCharsTests`' agreement checks against native `std::from_chars` report SKIPPED at
  deployment targets below 26.0, where that native oracle does not exist.
- Live SOAP tests skip without `SHARP_RUNTIME_SOAP_ENDPOINT`, exactly as on Linux.
