// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Diagnostics/Debug.hpp"
#include "System/Diagnostics/Trace.hpp"

using System::Diagnostics::Debug;
using System::Diagnostics::Trace;

// ---------------------------------------------------------------------------
// Debug tests
// Note: Debug methods compile to no-ops in NDEBUG builds — both branches must
// be safe. Assert(false) / Fail() intentionally omitted (they call assert()).
// ---------------------------------------------------------------------------

TEST(DebugTraceTests, Debug_Write_CString_DoesNotThrow) {
    EXPECT_NO_THROW(Debug::Write("hello"));
}

TEST(DebugTraceTests, Debug_Write_String_DoesNotThrow) {
    EXPECT_NO_THROW(Debug::Write(std::string("hello")));
}

TEST(DebugTraceTests, Debug_Write_EmptyString_DoesNotThrow) {
    EXPECT_NO_THROW(Debug::Write(""));
}

TEST(DebugTraceTests, Debug_WriteLine_NoArg_DoesNotThrow) {
    EXPECT_NO_THROW(Debug::WriteLine());
}

TEST(DebugTraceTests, Debug_WriteLine_CString_DoesNotThrow) {
    EXPECT_NO_THROW(Debug::WriteLine("line"));
}

TEST(DebugTraceTests, Debug_WriteLine_String_DoesNotThrow) {
    EXPECT_NO_THROW(Debug::WriteLine(std::string("line")));
}

TEST(DebugTraceTests, Debug_Assert_True_DoesNotThrow) {
    EXPECT_NO_THROW(Debug::Assert(true));
}

TEST(DebugTraceTests, Debug_Assert_True_WithCStringMessage_DoesNotThrow) {
    EXPECT_NO_THROW(Debug::Assert(true, "ok"));
}

TEST(DebugTraceTests, Debug_Assert_True_WithStringMessage_DoesNotThrow) {
    EXPECT_NO_THROW(Debug::Assert(true, std::string("ok")));
}

// ---------------------------------------------------------------------------
// Trace tests
// Trace is always active (no NDEBUG guard). Assert(false) and Fail() only
// write to std::cerr — they do NOT abort — so they are safe to call here.
// ---------------------------------------------------------------------------

TEST(DebugTraceTests, Trace_Write_String_DoesNotThrow) {
    EXPECT_NO_THROW(Trace::Write(std::string("hello")));
}

TEST(DebugTraceTests, Trace_Write_EmptyString_DoesNotThrow) {
    EXPECT_NO_THROW(Trace::Write(std::string("")));
}

TEST(DebugTraceTests, Trace_WriteLine_String_DoesNotThrow) {
    EXPECT_NO_THROW(Trace::WriteLine(std::string("line")));
}

TEST(DebugTraceTests, Trace_WriteLine_NoArg_DoesNotThrow) {
    EXPECT_NO_THROW(Trace::WriteLine());
}

TEST(DebugTraceTests, Trace_TraceInformation_DoesNotThrow) {
    EXPECT_NO_THROW(Trace::TraceInformation("info message"));
}

TEST(DebugTraceTests, Trace_TraceWarning_DoesNotThrow) {
    EXPECT_NO_THROW(Trace::TraceWarning("warn message"));
}

TEST(DebugTraceTests, Trace_TraceError_DoesNotThrow) {
    EXPECT_NO_THROW(Trace::TraceError("error message"));
}

TEST(DebugTraceTests, Trace_Assert_True_DoesNotThrow) {
    EXPECT_NO_THROW(Trace::Assert(true));
}

TEST(DebugTraceTests, Trace_Assert_True_WithMessage_DoesNotThrow) {
    EXPECT_NO_THROW(Trace::Assert(true, "all good"));
}

TEST(DebugTraceTests, Trace_Assert_False_WritesToCerrNoAbort) {
    // Trace::Assert(false) only emits to std::cerr — it does NOT call assert()
    EXPECT_NO_THROW(Trace::Assert(false, "intentional failure message"));
}

TEST(DebugTraceTests, Trace_Assert_False_NoMessage_DoesNotThrow) {
    EXPECT_NO_THROW(Trace::Assert(false));
}

TEST(DebugTraceTests, Trace_Fail_DoesNotThrow) {
    // Trace::Fail() only writes to std::cerr — it does NOT abort
    EXPECT_NO_THROW(Trace::Fail("intentional fail"));
}

TEST(DebugTraceTests, Trace_Flush_DoesNotThrow) {
    EXPECT_NO_THROW(Trace::Flush());
}
