// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2148 / SR-AUD-258 (cause C-B of docs/SystemIOCompressionNamespaceReviewPlan.md):
// the three compression stream types accepted a `CompressionMode` outside the enum's two declared
// members, and every public door kept answering silently after `Close()`.
//
// Typed over all three wrappers rather than one, because GZipStream and ZLibStream hold their own
// copies of these bodies rather than delegating to DeflateStream as .NET's do -- the same reason
// the #1841/#1828 capability matrix is twelve cases and not four.
//
// Measured before-matrix: build-probe/2148_probe1_before.log (behaviour) and
// build-probe/2148_probe2_before_after.log (the LeakSanitizer half).

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/IO/Compression/DeflateStream.hpp"
#include "System/IO/Compression/GZipStream.hpp"
#include "System/IO/Compression/ZLibStream.hpp"
#include "System/IO/IOException.hpp"
#include "System/IO/MemoryStream.hpp"
#include "System/NotSupportedException.hpp"
#include "System/ObjectDisposedException.hpp"

using namespace System::IO;
using namespace System::IO::Compression;
using SharpRuntime::bytecs;
using SharpRuntime::intcs;

namespace {

    std::vector<bytecs> samplePayload(std::size_t n = 4096) {
        std::vector<bytecs> v(n);
        for (std::size_t i = 0; i < n; ++i) v[i] = static_cast<bytecs>('a' + (i % 26));
        return v;
    }

    // ---------------------------------------------------------------------------------------
    // (a) mode domain
    // ---------------------------------------------------------------------------------------

    template <typename Wrapper>
    void expectModeRejected(int raw, const char* label) {
        MemoryStream inner;
        try {
            Wrapper w(&inner, static_cast<CompressionMode>(raw), /*leaveOpen=*/true);
            ADD_FAILURE() << label << ": mode " << raw << " was accepted";
        } catch (const System::ArgumentException& e) {
            // The base ArgumentException, not the derived ArgumentOutOfRangeException: the audit's
            // own managed probe for SR-AUD-258 recorded .NET's category for this call as
            // ArgumentException, and /rv is absent here to narrow it further.
            EXPECT_NE(std::string(e.what()).find("mode"), std::string::npos)
                << label << ": message must name the parameter, got: " << e.what();
        } catch (...) {
            ADD_FAILURE() << label << ": mode " << raw << " threw the wrong exception type";
        }
    }

    template <typename Wrapper>
    void expectModeDomainMatrix(const char* label) {
        // 2 is the first value past Compress = 1; 42 is the finding's own value; -1 and INTCS_MIN
        // cover the negative side, which a `mode <= Compress` style check would have let through.
        for (int raw : {2, 7, 42, -1, -42}) expectModeRejected<Wrapper>(raw, label);

        // Both declared members still construct, and still report their own capability.
        MemoryStream inner;
        Wrapper c(&inner, CompressionMode::Compress, true);
        EXPECT_TRUE(c.getCanWriteProperty()) << label;
        EXPECT_FALSE(c.getCanReadProperty()) << label;
        Wrapper d(&inner, CompressionMode::Decompress, true);
        EXPECT_TRUE(d.getCanReadProperty()) << label;
        EXPECT_FALSE(d.getCanWriteProperty()) << label;
    }

    // ORDER PIN: .NET's constructor runs ArgumentNullException.ThrowIfNull(stream) BEFORE the mode
    // switch, so a call that is wrong on both axes reports the null stream.
    template <typename Wrapper>
    void expectNullStreamBeatsInvalidMode(const char* label) {
        try {
            Wrapper w(nullptr, static_cast<CompressionMode>(42), true);
            ADD_FAILURE() << label << ": null stream + invalid mode was accepted";
        } catch (const System::ArgumentNullException&) {
            SUCCEED();
        } catch (const System::ArgumentException& e) {
            ADD_FAILURE() << label << ": the mode check ran before the null-stream check: " << e.what();
        }
    }

    // ---------------------------------------------------------------------------------------
    // (b) closed state
    // ---------------------------------------------------------------------------------------

    template <typename Wrapper>
    void expectClosedDoorsThrow(bool leaveOpen, const char* label) {
        const auto payload = samplePayload(64);
        {
            MemoryStream inner;
            Wrapper w(&inner, CompressionMode::Compress, leaveOpen);
            w.Close();
            EXPECT_THROW(w.Write(payload.data(), 0, static_cast<intcs>(payload.size())),
                         System::ObjectDisposedException) << label << " Write";
            EXPECT_THROW(w.Flush(), System::ObjectDisposedException) << label << " Flush";
        }
        {
            MemoryStream inner;
            Wrapper w(&inner, CompressionMode::Decompress, leaveOpen);
            w.Close();
            bytecs buf[16] = {};
            EXPECT_THROW((void)w.Read(buf, 0, 16), System::ObjectDisposedException) << label << " Read";
        }
    }

    // ORDER PIN: .NET's Read/Write validate the buffer arguments before EnsureNotDisposed, so a
    // closed stream given a null buffer still reports the null buffer.
    template <typename Wrapper>
    void expectArgumentChecksBeatClosedCheck(const char* label) {
        MemoryStream inner;
        Wrapper w(&inner, CompressionMode::Decompress, true);
        w.Close();
        EXPECT_THROW((void)w.Read(nullptr, 0, 4), System::ArgumentNullException) << label << " Read/null";
        bytecs buf[4] = {};
        EXPECT_THROW((void)w.Read(buf, -1, 4), System::ArgumentOutOfRangeException) << label << " Read/offset";
        EXPECT_THROW((void)w.Read(buf, 0, -1), System::ArgumentOutOfRangeException) << label << " Read/count";
        EXPECT_THROW(w.Write(nullptr, 0, 4), System::ArgumentNullException) << label << " Write/null";
        EXPECT_THROW(w.Write(buf, -1, 4), System::ArgumentOutOfRangeException) << label << " Write/offset";
        EXPECT_THROW(w.Write(buf, 0, -1), System::ArgumentOutOfRangeException) << label << " Write/count";
    }

    // ---------------------------------------------------------------------------------------
    // (c) controls -- everything the repair must NOT move
    // ---------------------------------------------------------------------------------------

    template <typename Wrapper>
    void expectControlsUnchanged(intcs expectedCompressedLength, const char* label) {
        // A whole valid compress cycle still produces the same byte count it produced before the
        // repair (build-probe/2148_probe1_before.log: 60 / 78 / 66). This is the control that
        // separates a state check from an output-changing edit.
        MemoryStream inner;
        const auto payload = samplePayload();
        {
            Wrapper w(&inner, CompressionMode::Compress, /*leaveOpen=*/true);
            w.Write(payload.data(), 0, static_cast<intcs>(payload.size()));
            w.Flush();
            w.Close();
        }
        EXPECT_EQ(inner.getLengthProperty(), expectedCompressedLength) << label;

        // Close() is still idempotent -- the destructor calls it unconditionally.
        MemoryStream other;
        Wrapper w(&other, CompressionMode::Compress, true);
        EXPECT_NO_THROW(w.Close());
        EXPECT_NO_THROW(w.Close());
        EXPECT_FALSE(w.getCanReadProperty()) << label;
        EXPECT_FALSE(w.getCanWriteProperty()) << label;
        EXPECT_THROW((void)w.getLengthProperty(), System::NotSupportedException) << label;
    }

} // namespace

// ---------------------------------------------------------------------------
// Mode domain
// ---------------------------------------------------------------------------

TEST(CompressionStreamModeDomainTests, DeflateStreamRejectsAnUndeclaredMode) {
    expectModeDomainMatrix<DeflateStream>("DeflateStream");
}
TEST(CompressionStreamModeDomainTests, GZipStreamRejectsAnUndeclaredMode) {
    expectModeDomainMatrix<GZipStream>("GZipStream");
}
TEST(CompressionStreamModeDomainTests, ZLibStreamRejectsAnUndeclaredMode) {
    expectModeDomainMatrix<ZLibStream>("ZLibStream");
}

TEST(CompressionStreamModeDomainTests, TheNullStreamCheckStillRunsFirst) {
    expectNullStreamBeatsInvalidMode<DeflateStream>("DeflateStream");
    expectNullStreamBeatsInvalidMode<GZipStream>("GZipStream");
    expectNullStreamBeatsInvalidMode<ZLibStream>("ZLibStream");
}

TEST(CompressionStreamModeDomainTests, ARejectedModeAllocatesNoZlibState) {
    // The check sits ahead of deflateInit2/inflateInit2 precisely so a rejected construction
    // allocates nothing to leak. Before ticket #2148 an out-of-domain mode initialised a DEFLATE
    // stream that Close() released with inflateEnd -- zlib rejects that with Z_STREAM_ERROR and
    // frees nothing, so LeakSanitizer reported 5,952 direct + 65,536 indirect bytes leaked per
    // object, in all three types (build-probe/2148_probe2_before_after.log).
    //
    // A unit test cannot observe the heap, so this asserts the property that makes the leak
    // impossible: the rejection happens at construction, so no object exists to leak from.
    for (int i = 0; i < 64; ++i) {
        MemoryStream inner;
        EXPECT_THROW(DeflateStream(&inner, static_cast<CompressionMode>(42), true),
                     System::ArgumentException);
        EXPECT_THROW(GZipStream(&inner, static_cast<CompressionMode>(42), true),
                     System::ArgumentException);
        EXPECT_THROW(ZLibStream(&inner, static_cast<CompressionMode>(42), true),
                     System::ArgumentException);
    }
}

// ---------------------------------------------------------------------------
// Closed state
// ---------------------------------------------------------------------------

TEST(CompressionStreamClosedStateTests, DeflateStreamThrowsOnEveryDoorAfterClose) {
    for (bool leaveOpen : {true, false})
        expectClosedDoorsThrow<DeflateStream>(leaveOpen, "DeflateStream");
}
TEST(CompressionStreamClosedStateTests, GZipStreamThrowsOnEveryDoorAfterClose) {
    for (bool leaveOpen : {true, false})
        expectClosedDoorsThrow<GZipStream>(leaveOpen, "GZipStream");
}
TEST(CompressionStreamClosedStateTests, ZLibStreamThrowsOnEveryDoorAfterClose) {
    for (bool leaveOpen : {true, false})
        expectClosedDoorsThrow<ZLibStream>(leaveOpen, "ZLibStream");
}

TEST(CompressionStreamClosedStateTests, BufferArgumentsAreStillValidatedFirst) {
    expectArgumentChecksBeatClosedCheck<DeflateStream>("DeflateStream");
    expectArgumentChecksBeatClosedCheck<GZipStream>("GZipStream");
    expectArgumentChecksBeatClosedCheck<ZLibStream>("ZLibStream");
}

TEST(CompressionStreamClosedStateTests, AZeroCountWriteOnAClosedStreamAlsoThrows) {
    // The closed check is deliberately placed BEFORE the `count == 0` early return, so "nothing to
    // write" cannot launder a use-after-close into a success.
    MemoryStream inner;
    DeflateStream w(&inner, CompressionMode::Compress, true);
    w.Close();
    bytecs buf[1] = {};
    EXPECT_THROW(w.Write(buf, 0, 0), System::ObjectDisposedException);
    EXPECT_THROW((void)w.Read(buf, 0, 0), System::ObjectDisposedException);
}

// ---------------------------------------------------------------------------
// Controls
// ---------------------------------------------------------------------------

TEST(CompressionStreamStateControlTests, TheValidCycleIsByteStable) {
    expectControlsUnchanged<DeflateStream>(60, "DeflateStream");
    expectControlsUnchanged<GZipStream>(78, "GZipStream");
    expectControlsUnchanged<ZLibStream>(66, "ZLibStream");
}

TEST(CompressionStreamStateControlTests, RoundTripStillWorksForEveryWrapper) {
    const auto payload = samplePayload(8192);

    auto roundTrip = [&](auto tag) {
        using Wrapper = decltype(tag);
        MemoryStream compressed;
        {
            Wrapper w(&compressed, CompressionMode::Compress, /*leaveOpen=*/true);
            w.Write(payload.data(), 0, static_cast<intcs>(payload.size()));
            w.Close();
        }
        const auto bytes = compressed.ToArray();
        MemoryStream src(bytes.data(), static_cast<intcs>(bytes.size()));
        Wrapper r(&src, CompressionMode::Decompress, /*leaveOpen=*/true);
        std::vector<bytecs> out(payload.size());
        const intcs n = r.Read(out.data(), 0, static_cast<intcs>(out.size()));
        out.resize(static_cast<std::size_t>(n));
        EXPECT_EQ(out, payload);
    };

    // Constructed only to select the template; the wrappers are cheap and immediately discarded.
    MemoryStream scratch;
    roundTrip(DeflateStream(&scratch, CompressionMode::Compress, true));
    roundTrip(GZipStream(&scratch, CompressionMode::Compress, true));
    roundTrip(ZLibStream(&scratch, CompressionMode::Compress, true));
}

// ---------------------------------------------------------------------------
// PINS for the two doors ticket #2148 deliberately did NOT change (new ticket #2152)
// ---------------------------------------------------------------------------

TEST(CompressionStreamStatePinTests, ReadOnACompressModeStreamStillAnswersZero_SeeTicket2152) {
    // .NET throws InvalidOperationException("Reading is not supported on this stream.") here. This
    // port answers 0 -- a silent wrong answer -- and this test pins that, rather than changing it
    // on recollection: the exception type cannot be confirmed with /rv absent, and SR-AUD-258 does
    // not name this door. Ticket #2152 owns it.
    MemoryStream inner;
    DeflateStream w(&inner, CompressionMode::Compress, true);
    bytecs buf[16] = {};
    EXPECT_EQ(w.Read(buf, 0, 16), 0);
}

TEST(CompressionStreamStatePinTests, WriteOnADecompressModeStreamStillThrowsIOException_SeeTicket2152) {
    // Same door, opposite direction: this one does throw, but with zlib's raw Z_STREAM_ERROR
    // surfaced as an IOException ("deflate error -2") rather than .NET's InvalidOperationException.
    MemoryStream inner;
    DeflateStream w(&inner, CompressionMode::Decompress, true);
    const auto payload = samplePayload();
    EXPECT_THROW(w.Write(payload.data(), 0, static_cast<intcs>(payload.size())),
                 System::IO::IOException);
}
