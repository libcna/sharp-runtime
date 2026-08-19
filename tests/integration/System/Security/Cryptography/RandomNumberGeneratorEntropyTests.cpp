// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2398 -- System::Security::Cryptography::RandomNumberGenerator.
//
// The shipped coverage for this type was two cases that asserted `buffer.size()` AFTER filling a
// buffer whose size was fixed before the call, so both would have passed against a generator that
// wrote nothing at all. These cases assert what the type is for.
//
// The repair #2398 landed collapsed four platform arms into two: an `__EMSCRIPTEN__` arm that
// THREW `PlatformNotSupportedException` is gone (its premise -- "no secure random source wired up
// under Emscripten yet" -- was measured false by #2228 in this same repository, and .NET's
// `RandomNumberGeneratorImplementation.Browser.cs` does not refuse either), and the Linux-only
// `getrandom()` arm is gone with it. **What that buys is testability**: with one non-Windows arm,
// the code Emscripten takes is the code Linux takes, so the cases below exercise it on every gate
// run rather than describing an arm no gate compiles.
#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Security/Cryptography/RandomNumberGenerator.hpp"

#ifndef _WIN32
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

using SharpRuntime::bytecs;
using System::Security::Cryptography::RandomNumberGenerator;

namespace {

// A sentinel byte the generator would have to reproduce for a whole run to fool the tests below.
// Over the ranges used here that is 256^-n, which is not a flake this suite has to reason about.
constexpr bytecs kSentinel = 0xA5;

bool anyByteChanged(const std::vector<bytecs>& data, size_t from, size_t to) {
    for (size_t i = from; i < to; ++i)
        if (data[i] != kSentinel) return true;
    return false;
}

bool allBytesUnchanged(const std::vector<bytecs>& data, size_t from, size_t to) {
    for (size_t i = from; i < to; ++i)
        if (data[i] != kSentinel) return false;
    return true;
}

} // namespace

// ===========================================================================
// The 256-byte chunking loop
// ===========================================================================

// `getentropy()` has a HARD 256-byte-per-call maximum and reports `EIO` for more rather than
// truncating silently. A request larger than that therefore only succeeds if the loop chunks, and
// only fills the whole buffer if the loop advances. Before #2398 the Linux gate ran `getrandom()`,
// which has no such limit, so THIS PATH WAS NEVER EXERCISED HERE at all.
TEST(Rng2398ChunkingTests, ARequestLargerThanOneChunkIsFilledEndToEnd) {
    std::vector<bytecs> data(1000, kSentinel);
    ASSERT_NO_THROW(RandomNumberGenerator::Fill(data));
    // The tail lies entirely beyond the first chunk, so a single unchunked call -- or a loop that
    // does not advance past the first chunk -- leaves it exactly as it was.
    EXPECT_TRUE(anyByteChanged(data, 256, 1000))
        << "bytes 256..999 are untouched, so the request was not chunked past the first call";
    EXPECT_TRUE(anyByteChanged(data, 768, 1000))
        << "the final chunk is untouched, so the loop stopped early";
}

// The boundary itself, on both sides, because a `<` / `<=` slip in the chunk arithmetic shows up
// exactly here and nowhere else.
TEST(Rng2398ChunkingTests, TheChunkBoundaryIsHandledOnBothSides) {
    for (size_t n : {size_t{255}, size_t{256}, size_t{257}, size_t{512}, size_t{513}}) {
        std::vector<bytecs> data(n, kSentinel);
        ASSERT_NO_THROW(RandomNumberGenerator::Fill(data)) << "n=" << n;
        EXPECT_TRUE(anyByteChanged(data, 0, n)) << "n=" << n << " was not filled at all";
        if (n > 256) {
            EXPECT_TRUE(anyByteChanged(data, 256, n))
                << "n=" << n << " was filled only up to the first chunk";
        }
    }
}

// An empty request is a no-op rather than a call, which matters because `getentropy(ptr, 0)` on a
// buffer with no storage is not a call worth making.
TEST(Rng2398ChunkingTests, AnEmptyRequestIsANoOp) {
    std::vector<bytecs> empty;
    EXPECT_NO_THROW(RandomNumberGenerator::Fill(empty));
    EXPECT_TRUE(empty.empty());
}

// ===========================================================================
// The entropy actually is the platform's
// ===========================================================================

#ifndef _WIN32
// #2228's idiom, applied to the type it belongs to. A userspace PRNG has STATE and `fork()`
// duplicates it, so a parent and a child drawing after the fork produce the SAME bytes. A CSPRNG
// has no such state to inherit. This is the one observable difference between the two, and it is
// the reason the shipped size assertions could not have caught a weak source.
TEST(Rng2398EntropyTests, ParentAndChildDoNotDrawTheSameBytesAfterFork) {
    // Draw once BEFORE forking, so any lazily initialised generator state is already seeded and
    // therefore inheritable.
    std::vector<bytecs> before(16);
    RandomNumberGenerator::Fill(before);

    int pipeFds[2] = {-1, -1};
    ASSERT_EQ(::pipe(pipeFds), 0);

    const pid_t child = ::fork();
    ASSERT_GE(child, 0);

    if (child == 0) {
        ::close(pipeFds[0]);
        std::vector<bytecs> childBytes(16);
        RandomNumberGenerator::Fill(childBytes);
        const ssize_t written = ::write(pipeFds[1], childBytes.data(), childBytes.size());
        ::close(pipeFds[1]);
        ::_exit(written == static_cast<ssize_t>(childBytes.size()) ? 0 : 1);
    }

    ::close(pipeFds[1]);
    std::vector<bytecs> parentBytes(16);
    RandomNumberGenerator::Fill(parentBytes);

    std::vector<bytecs> received;
    bytecs buffer[64];
    ssize_t n = 0;
    while ((n = ::read(pipeFds[0], buffer, sizeof(buffer))) > 0)
        received.insert(received.end(), buffer, buffer + n);
    ::close(pipeFds[0]);

    int status = 0;
    ASSERT_EQ(::waitpid(child, &status, 0), child);
    ASSERT_TRUE(WIFEXITED(status));
    ASSERT_EQ(WEXITSTATUS(status), 0) << "the child could not report its bytes";
    ASSERT_EQ(received.size(), 16u);

    EXPECT_NE(received, parentBytes)
        << "parent and child produced the SAME bytes after fork(), which means this generator is "
           "drawing from inherited userspace state rather than from the platform CSPRNG";
    EXPECT_NE(received, before);
}
#endif

// Two successive draws differing is a far weaker property than the fork case, but it is the one
// that fails immediately if the generator stops writing at all.
TEST(Rng2398EntropyTests, TwoSuccessiveDrawsDiffer) {
    std::vector<bytecs> a(32, kSentinel), b(32, kSentinel);
    RandomNumberGenerator::Fill(a);
    RandomNumberGenerator::Fill(b);
    EXPECT_NE(a, b);
    EXPECT_NE(a, std::vector<bytecs>(32, kSentinel));
}

// `Create()` must hand back a working generator, not merely a non-null pointer.
TEST(Rng2398EntropyTests, CreateReturnsAGeneratorThatActuallyWrites) {
    auto rng = RandomNumberGenerator::Create();
    ASSERT_NE(rng, nullptr);
    std::vector<bytecs> data(64, kSentinel);
    rng->GetBytes(data);
    EXPECT_TRUE(anyByteChanged(data, 0, data.size()));
}

// ===========================================================================
// The windowed overload writes only its window
// ===========================================================================

// `RandomNumberGenerator.cs:36-54` fills exactly `[offset, offset + count)`. The bytes on either
// side are the assertion: a repair that filled the whole array would satisfy every length check.
TEST(Rng2398WindowTests, GetBytesWithOffsetAndCountLeavesTheRestAlone) {
    auto rng = RandomNumberGenerator::Create();
    std::vector<bytecs> data(64, kSentinel);
    rng->GetBytes(data, 16, 32);
    EXPECT_TRUE(allBytesUnchanged(data, 0, 16)) << "bytes before the window were written";
    EXPECT_TRUE(anyByteChanged(data, 16, 48)) << "the window itself was not written";
    EXPECT_TRUE(allBytesUnchanged(data, 48, 64)) << "bytes after the window were written";
}

// A zero count writes nothing at all -- `RandomNumberGenerator.cs:38` guards on `count > 0`.
TEST(Rng2398WindowTests, AZeroCountWritesNothing) {
    auto rng = RandomNumberGenerator::Create();
    std::vector<bytecs> data(16, kSentinel);
    rng->GetBytes(data, 4, 0);
    EXPECT_TRUE(allBytesUnchanged(data, 0, 16));
}

TEST(Rng2398WindowTests, GetNonZeroBytesContainsNoZero) {
    auto rng = RandomNumberGenerator::Create();
    std::vector<bytecs> data(128, kSentinel);
    rng->GetNonZeroBytes(data);
    EXPECT_EQ(std::count(data.begin(), data.end(), static_cast<bytecs>(0)), 0);
    EXPECT_TRUE(anyByteChanged(data, 0, data.size()));
}

// ===========================================================================
// The two messages #2398 corrected
// ===========================================================================

// `RandomNumberGenerator.cs:105-106` throws `ArgumentException` with
// `SR.Argument_InvalidRandomRange` (Strings.resx:126-128). The TYPE was already right -- .NET does
// not use `ArgumentOutOfRangeException` here and names no parameter, because the fault is the
// relationship between the two arguments rather than either one's range -- so only the text moved.
TEST(Rng2398MessageTests, GetInt32RejectsAnEmptyRangeWithDotNetsText) {
    try {
        (void)RandomNumberGenerator::GetInt32(5, 5);
        FAIL() << "an empty range was accepted";
    } catch (const System::ArgumentException& e) {
        EXPECT_EQ(std::string(e.getMessageProperty()),
                  "Range of random number does not contain at least one possibility.");
    }
    EXPECT_THROW((void)RandomNumberGenerator::GetInt32(9, 3), System::ArgumentException);
}

// `RandomNumberGenerator.cs:438-446` -- `SR.Argument_InvalidOffLen`. The text used to be truncated
// at "for the array.", which also left this file disagreeing with the rest of the port; Console.hpp
// already spells .NET's full sentence.
TEST(Rng2398MessageTests, AnOutOfBoundsWindowReportsDotNetsFullText) {
    auto rng = RandomNumberGenerator::Create();
    std::vector<bytecs> data(8);
    try {
        rng->GetBytes(data, 4, 8);
        FAIL() << "a window past the end of the buffer was accepted";
    } catch (const System::ArgumentException& e) {
        EXPECT_EQ(std::string(e.getMessageProperty()),
                  "Offset and length were out of bounds for the array or count is greater than "
                  "the number of elements from index to the end of the source collection.");
    }
}

// The two guards that DO name a parameter still do, and are `ArgumentOutOfRangeException` --
// asserted beside the case above so the two shapes cannot be conflated by a later repair.
TEST(Rng2398MessageTests, NegativeOffsetAndCountStayArgumentOutOfRange) {
    auto rng = RandomNumberGenerator::Create();
    std::vector<bytecs> data(8);
    EXPECT_THROW(rng->GetBytes(data, -1, 2), System::ArgumentOutOfRangeException);
    EXPECT_THROW(rng->GetBytes(data, 0, -1), System::ArgumentOutOfRangeException);
    EXPECT_THROW((void)RandomNumberGenerator::GetBytes(-1), System::ArgumentOutOfRangeException);
    EXPECT_THROW((void)RandomNumberGenerator::GetInt32(0), System::ArgumentOutOfRangeException);
}

// `GetInt32` must stay inside its half-open range. The single-possibility short-circuit
// (`RandomNumberGenerator.cs:114-117`) is asserted separately, because it is the one input for
// which no random draw happens at all.
TEST(Rng2398MessageTests, GetInt32StaysInRangeAndShortCircuitsASinglePossibility) {
    for (int i = 0; i < 200; ++i) {
        const auto v = RandomNumberGenerator::GetInt32(10, 20);
        ASSERT_GE(v, 10);
        ASSERT_LT(v, 20);
    }
    EXPECT_EQ(RandomNumberGenerator::GetInt32(7, 8), 7);
    EXPECT_EQ(RandomNumberGenerator::GetInt32(-3, -2), -3);
}
