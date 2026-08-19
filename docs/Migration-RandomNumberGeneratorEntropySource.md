<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `RandomNumberGenerator`'s entropy source and two messages (ticket #2398)

*2026-08-19.* `System::Security::Cryptography::RandomNumberGenerator` **threw
`PlatformNotSupportedException` on Emscripten**, on a premise this repository had already measured
to be false. It no longer does. Two exception messages also moved onto .NET's exact text.

Landed under **SA-5**. No layout, vtable, signature or `noexcept` change, and **no outlawed
spelling** — the repair is confined to one `.cpp` body and two string literals.

**Downstream, measured:** **zero** `RandomNumberGenerator` and **zero** `RNGCryptoServiceProvider`
sites in `cna` and in `mobile-eggbert`.

---

## 1. Emscripten gets a real CSPRNG, because it always had one

The old body had four platform arms. The second was:

```cpp
#elif defined(__EMSCRIPTEN__)
    // No secure random source wired up under Emscripten yet — throw a clear exception
    // rather than silently produce weak randomness (e.g. from an unseeded PRNG).
    throw System::PlatformNotSupportedException(
        "RandomNumberGenerator is not implemented on Emscripten in this runtime.");
```

**That comment's premise was measured false by #2228, in this same repository.** `Guid.cpp:377-388`
records it in terms: Emscripten's libc declares `getentropy()` in `<unistd.h>` and implements it as
`__wasi_random_get()` (`system/lib/libc/musl/src/misc/getentropy.c`), which the runtime backs with
the host's `crypto.getRandomValues`. **So `System::Guid::NewGuid()` has been drawing real entropy on
Emscripten through exactly the call this file refused to make** — two answers to one question inside
one runtime, and the type whose entire purpose is cryptographic randomness was the one refusing.

.NET does not refuse either: `RandomNumberGeneratorImplementation.Browser.cs` forwards to
`Interop.GetCryptographicallySecureRandomBytes`, whose `__EMSCRIPTEN__` arm is `SystemJS_RandomBytes`
(`src/native/minipal/random.c:83-93`).

### 1.1 Four arms became two, and that is what makes the repair verifiable

The Linux-only `getrandom()` arm went with the Emscripten one. The file now has `Guid.cpp`'s shape:
Windows uses `BCryptGenRandom`, and **every other platform uses one chunked `getentropy()` loop**.

That is not tidying. `getrandom()` is Linux-only — undeclared on Apple/BSD, and Emscripten declares
it but backs `getentropy` with `__wasi_random_get` — so the old file had **one arm per platform, and
the Linux gate compiled exactly one of them**. With a single non-Windows arm, **the code Emscripten
takes is the code Linux takes**, so the gate exercises it on every run. An arm no gate could compile
became an arm every gate run executes.

### 1.2 What a caller has to change

**On Emscripten:** a `try`/`catch` around `RandomNumberGenerator::Create()`,
`RandomNumberGenerator::Fill()`, `GetBytes()` or `GetInt32()` written to handle
`PlatformNotSupportedException` will no longer see one. Nothing else moves; the call now succeeds.

**On Linux:** the syscall behind the call changed from `getrandom(buf, n, 0)` to a loop of
`getentropy(buf, ≤256)`. Both are the same kernel CSPRNG — glibc implements `getentropy()` on top of
`getrandom()` — and both block until the pool is initialised. No caller-visible behaviour changes.
This is also the call `Guid::NewGuid()` has already been making on Linux since #2228.

**On Windows and Apple/BSD:** nothing changes.

### 1.3 Failure still throws, and that differs from `Guid` deliberately

`Guid::NewGuid()` retries on failure rather than throwing, because callers treat it as infallible and
an escaping exception would reach `std::terminate`. This member has no such constraint, and **.NET
throws too**: `Interop.GetRandomBytes.cs:22-26` is
`if (Sys.GetCryptographicallySecureRandomBytes(...) != 0) throw new CryptographicException();`.
Neither ever falls back to a weaker source, which is the property both exist to hold.

### 1.4 The limitation was also undeclared

`CLAUDE.md`'s platform-limitation table lists `Net::Sockets`, `IO::RandomAccess`, `AppDomain`,
`TimeZoneInfo`, `Diagnostics::Process`, `PosixSignal`, `NetworkInterface` and `FileSystemWatcher`.
**It never listed `RandomNumberGenerator.`** So this limitation was not only wrong, it was
unrecorded — a caller reading the table would have concluded the type worked everywhere. After this
change that conclusion is correct, so no row is added.

---

## 2. Two messages are now .NET's exact text

| Site | Before | After (= .NET) |
|---|---|---|
| `GetInt32(from, to)` with `from >= to` | `"fromInclusive must be less than toExclusive."` | `"Range of random number does not contain at least one possibility."` |
| `verifyGetBytes` window past the end | `"Offset and length were out of bounds for the array."` | `"Offset and length were out of bounds for the array or count is greater than the number of elements from index to the end of the source collection."` |

`RandomNumberGenerator.cs:105-106` (`SR.Argument_InvalidRandomRange`, `Strings.resx:126-128`) and
`RandomNumberGenerator.cs:438-446` (`SR.Argument_InvalidOffLen`).

**The exception types were already right and did not move.** .NET throws `ArgumentException` for the
empty range rather than `ArgumentOutOfRangeException`, and names **no parameter**, because the fault
is the *relationship* between the two arguments rather than either one's range. The negative-`offset`
and negative-`count` guards remain `ArgumentOutOfRangeException` with their parameter names, and a
test asserts the two shapes side by side so a later repair cannot conflate them.

**The second message also ended an inconsistency inside this port**: `Console.hpp:140` already spells
.NET's full sentence verbatim. Only this file truncated it.

---

## 3. Testing, and the two things that could not be tested

The shipped coverage was two cases that asserted `buffer.size()` **after** filling a buffer whose
size was fixed before the call — both would have passed against a generator that wrote nothing.
Thirteen cases replace them, including the `fork()` distinctness case #2228 established for `Guid`
(a userspace PRNG has state and `fork()` duplicates it; a CSPRNG does not) and the 256-byte chunk
boundary, **which the Linux gate could not reach before this change at all** because `getrandom()`
has no such limit.

Eight mutations, six caught. **Two are not caught and both are stated rather than papered over:**

- **Restoring the `__EMSCRIPTEN__` throw is not caught here, and cannot be.** That arm is not
  compiled on this platform, and there is no Emscripten toolchain in this container. What *is*
  verified is stronger than it was: the arm Emscripten now takes is the arm Linux runs on every
  gate. The residual unverified claim is that `getentropy()` exists on Emscripten — #2228's
  measurement, which shipped code has already relied on since that ticket.
- **Ignoring a `getentropy()` failure is not observable in isolation.** With chunking intact the
  call does not fail on a working system, so no test can drive the branch. It becomes observable
  only in combination with a chunking mutation, which is separately caught.

One mutation was **invalid as first written and was reformulated rather than counted**: removing the
chunk cap left `maxChunk` unused, so `-Werror=unused-variable` rejected it at compile time and the
verdict said nothing about the tests.
