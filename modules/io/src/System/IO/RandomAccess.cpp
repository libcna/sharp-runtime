// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/RandomAccess.hpp"

#include <cerrno>
#include <cstddef>
#include <limits>
#include <string>
#include <system_error>

#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/IO/IOException.hpp"

#if defined(_WIN32)
#  include <windows.h>
#  include <io.h>       // _get_osfhandle, _chsize_s
namespace {
    // CRT fd → Win32 HANDLE
    inline HANDLE hOf(int fd) { return reinterpret_cast<HANDLE>(_get_osfhandle(fd)); }
}
#elif defined(__EMSCRIPTEN__)
#  include "System/PlatformNotSupportedException.hpp"
#else
#  include <unistd.h>   // pread, pwrite, lseek, ftruncate, fsync
#endif

namespace System::IO {

namespace {

// ---------------------------------------------------------------------------
// Ticket #2100 (SR-AUD-340, cause I-B: a public argument domain is unchecked, or its rejection
// is untyped). Every public member routes its argument-domain checks through these helpers so
// the eight doors -- four raw-pointer, four std::vector -- cannot drift apart the way
// RandomAccess::Write and RandomAccess::Read already had.
//
// The exception TYPE and paramName below are THIS PORT'S CHOICE, recorded as such in
// docs/SystemIONamespaceReviewPlan.md §16: /rv/tmp/runtime/ is absent, so no reference evidence
// pins what .NET raises here. The choice follows the shape .NET uses everywhere else for a
// negative count -- ArgumentOutOfRangeException carrying the offending parameter's name and its
// actual value.
// ---------------------------------------------------------------------------

void CheckCount(intcs count) {
    if (count < 0)
        throw System::ArgumentOutOfRangeException(
            "count", std::to_string(count), "Non-negative number required.");
}

void CheckFileOffset(longcs fileOffset) {
    if (fileOffset < 0)
        throw System::ArgumentOutOfRangeException(
            "fileOffset", std::to_string(fileOffset), "Non-negative number required.");
}

void CheckLength(longcs length) {
    if (length < 0)
        throw System::ArgumentOutOfRangeException(
            "length", std::to_string(length), "Non-negative number required.");
}

/// A null buffer is only an error when bytes would actually be transferred: a zero-length
/// transfer through a null pointer is well defined here and stays accepted.
void CheckBuffer(const void* buffer, intcs count) {
    if (buffer == nullptr && count > 0)
        throw System::ArgumentNullException("buffer");
}

/// The std::vector overloads forward to the raw-pointer ones, whose count is an `intcs`. A
/// vector longer than INTCS_MAX cannot be expressed in that parameter, and the previous
/// unchecked static_cast turned it into an implementation-defined -- possibly negative -- count.
/// Rejecting it is the only representable answer.
intcs CheckedVectorCount(std::size_t size) {
    if (size > static_cast<std::size_t>((std::numeric_limits<intcs>::max)()))
        throw System::ArgumentOutOfRangeException(
            "buffer", std::to_string(size),
            "Buffer length exceeds the maximum number of bytes a single operation can transfer.");
    return static_cast<intcs>(size);
}

// Every caller of the two helpers below sits in a _WIN32 or POSIX branch, and each of those
// three-way #if blocks answers Emscripten with PlatformNotSupportedException instead. So on that
// target the helpers genuinely have no caller, and Clang -- which Emscripten uses, where GCC does
// not diagnose this -- reports "unused function 'ThrowNative'" against -Werror and the build stops.
// Compiling them only where they are used fixes it without a suppression, and keeps the dead code
// out of the wasm binary. NativeDetail has exactly one caller, ThrowNative, so both must be inside
// the same guard: excluding only ThrowNative moves the same error one line up.
#if !defined(__EMSCRIPTEN__)

/// The native reason a syscall failed, appended to the IOException message. Before #2100 every
/// failure surfaced as a bare "RandomAccess::<member> failed" with the reason discarded, so a
/// caller could not tell EBADF from EINVAL from ENOSPC.
///
/// std::system_category().message() is used rather than std::strerror() because these are static
/// members with no synchronisation of their own and strerror() returns a shared buffer.
std::string NativeDetail(int code) {
    return std::system_category().message(code);
}

[[noreturn]] void ThrowNative(const char* member, int code) {
    throw IOException(std::string(member) + " failed: " + NativeDetail(code));
}

#endif // !defined(__EMSCRIPTEN__)

} // namespace

int64_t RandomAccess::GetLength(int fd) {
#if defined(__EMSCRIPTEN__)
    (void)fd;
    throw System::PlatformNotSupportedException("RandomAccess is not supported on Emscripten.");
#elif defined(_WIN32)
    LARGE_INTEGER sz{};
    if (!GetFileSizeEx(hOf(fd), &sz))
        ThrowNative("RandomAccess::GetLength", static_cast<int>(GetLastError()));
    return static_cast<int64_t>(sz.QuadPart);
#else
    // Before #2100 all three lseek() results were discarded, so a descriptor that is invalid
    // (EBADF) or simply not seekable (a pipe, ESPIPE) made this RETURN the sentinel -1 -- a
    // value no caller could distinguish from a real length and that .NET can never produce.
    // The Windows branch above already threw for the same inputs; this is a POSIX-only defect.
    //
    // The mechanism is deliberately unchanged: lseek(SEEK_END) still supplies the answer, and
    // the original position is still restored, so every descriptor that worked before returns
    // exactly the same number. Switching to fstat() would also answer for pipes, but that would
    // be a behaviour change on a guess with the reference tree absent.
    const off_t pos = lseek(fd, 0, SEEK_CUR);
    if (pos < 0) ThrowNative("RandomAccess::GetLength", errno);
    const off_t len = lseek(fd, 0, SEEK_END);
    if (len < 0) ThrowNative("RandomAccess::GetLength", errno);  // the position never moved
    if (lseek(fd, pos, SEEK_SET) < 0) ThrowNative("RandomAccess::GetLength", errno);
    return static_cast<int64_t>(len);
#endif
}

void RandomAccess::SetLength(int fd, int64_t length) {
    CheckLength(length);
#if defined(__EMSCRIPTEN__)
    (void)fd;
    throw System::PlatformNotSupportedException("RandomAccess is not supported on Emscripten.");
#elif defined(_WIN32)
    LARGE_INTEGER li{};
    li.QuadPart = length;
    if (!SetFilePointerEx(hOf(fd), li, nullptr, FILE_BEGIN) || !SetEndOfFile(hOf(fd)))
        ThrowNative("RandomAccess::SetLength", static_cast<int>(GetLastError()));
#else
    if (ftruncate(fd, static_cast<off_t>(length)) != 0)
        ThrowNative("RandomAccess::SetLength", errno);
#endif
}

intcs RandomAccess::Read(int fd, std::vector<bytecs>& buffer, int64_t fileOffset) {
    return Read(fd, buffer.data(), CheckedVectorCount(buffer.size()), fileOffset);
}

intcs RandomAccess::Read(int fd, bytecs* buffer, intcs count, int64_t fileOffset) {
    // Argument validation precedes the platform dispatch, so an out-of-domain argument gets the
    // same answer on every platform and is never masked by a PlatformNotSupportedException.
    CheckCount(count);
    CheckFileOffset(fileOffset);
    CheckBuffer(buffer, count);
#if defined(__EMSCRIPTEN__)
    (void)fd;
    throw System::PlatformNotSupportedException("RandomAccess is not supported on Emscripten.");
#elif defined(_WIN32)
    OVERLAPPED ov{};
    ov.Offset     = static_cast<DWORD>(fileOffset & 0xFFFFFFFFLL);
    ov.OffsetHigh = static_cast<DWORD>(fileOffset >> 32);
    DWORD nRead = 0;
    if (!ReadFile(hOf(fd), buffer, static_cast<DWORD>(count), &nRead, &ov)) {
        // A read that reaches end-of-file through an OVERLAPPED handle reports ERROR_HANDLE_EOF
        // rather than a zero-byte success; that is the end of the file, not a failure.
        const DWORD err = GetLastError();
        if (err == ERROR_HANDLE_EOF) return 0;
        ThrowNative("RandomAccess::Read", static_cast<int>(err));
    }
    return static_cast<intcs>(nRead);
#else
    for (;;) {
        const ssize_t n =
            pread(fd, buffer, static_cast<size_t>(count), static_cast<off_t>(fileOffset));
        if (n >= 0) return static_cast<intcs>(n);
        if (errno == EINTR) continue;  // the same idiom FileSystemWatcher.cpp already uses
        ThrowNative("RandomAccess::Read", errno);
    }
#endif
}

void RandomAccess::Write(int fd, const std::vector<bytecs>& buffer, int64_t fileOffset) {
    Write(fd, buffer.data(), CheckedVectorCount(buffer.size()), fileOffset);
}

void RandomAccess::Write(int fd, const bytecs* buffer, intcs count, int64_t fileOffset) {
    // THE SHARP HALF of SR-AUD-340 (plan §6.3). A negative count made the `while (count > 0)`
    // loops below fall straight through, so Write(fd, buffer, -1, 0) SUCCEEDED SILENTLY: no
    // bytes written, no error, and a void return giving the caller nothing to inspect.
    CheckCount(count);
    CheckFileOffset(fileOffset);
    CheckBuffer(buffer, count);
#if defined(__EMSCRIPTEN__)
    (void)fd;
    throw System::PlatformNotSupportedException("RandomAccess is not supported on Emscripten.");
#elif defined(_WIN32)
    // WriteFile can also complete a "short write"; loop the same as the POSIX branch below.
    while (count > 0) {
        OVERLAPPED ov{};
        ov.Offset     = static_cast<DWORD>(fileOffset & 0xFFFFFFFFLL);
        ov.OffsetHigh = static_cast<DWORD>(fileOffset >> 32);
        DWORD nWritten = 0;
        if (!WriteFile(hOf(fd), buffer, static_cast<DWORD>(count), &nWritten, &ov))
            ThrowNative("RandomAccess::Write", static_cast<int>(GetLastError()));
        // The same zero-progress guard as the POSIX branch below: SR-AUD-340's report names
        // BOTH platform branches as missing it, and without it this loop never terminates.
        if (nWritten == 0)
            throw IOException("RandomAccess::Write failed: the write made no progress");
        buffer += nWritten;
        count -= static_cast<intcs>(nWritten);
        fileOffset += nWritten;
    }
#else
    // Verified against RandomAccess.Unix.cs's WriteAtOffset: real .NET loops "while
    // (!buffer.IsEmpty)", advancing the buffer and file offset by however many bytes were
    // actually written each call, until the whole buffer has been written. pwrite() can
    // legitimately return fewer bytes than requested (a "short write" -- e.g. after a signal
    // interruption, or writing to certain non-regular files); this previously issued a single
    // pwrite() call and silently discarded any bytes it didn't cover, with no error and no way
    // for the void-returning caller to detect the loss.
    while (count > 0) {
        const ssize_t n =
            pwrite(fd, buffer, static_cast<size_t>(count), static_cast<off_t>(fileOffset));
        // An interrupted pwrite() previously threw from the middle of the loop, leaving the
        // bytes already written in place with no way for a void-returning caller to learn how
        // many. Retrying is the same idiom FileSystemWatcher.cpp already uses for EINTR.
        if (n < 0) {
            if (errno == EINTR) continue;
            ThrowNative("RandomAccess::Write", errno);
        }
        // SR-AUD-340's report also names this: "neither platform branch detects zero-byte write
        // progress". A pwrite() that reports success while transferring nothing would spin this
        // loop forever, because `count` never decreases. Failing is the only terminating answer.
        if (n == 0)
            throw IOException("RandomAccess::Write failed: the write made no progress");
        buffer += n;
        count -= static_cast<intcs>(n);
        fileOffset += n;
    }
#endif
}

void RandomAccess::FlushToDisk(int fd) {
#if defined(__EMSCRIPTEN__)
    (void)fd;
    throw System::PlatformNotSupportedException("RandomAccess is not supported on Emscripten.");
#elif defined(_WIN32)
    if (!FlushFileBuffers(hOf(fd)))
        ThrowNative("RandomAccess::FlushToDisk", static_cast<int>(GetLastError()));
#else
    if (fsync(fd) != 0) ThrowNative("RandomAccess::FlushToDisk", errno);
#endif
}

} // namespace System::IO
