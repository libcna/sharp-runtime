// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>

#include <iostream>
#include <sstream>

#include "System/Diagnostics/Trace.hpp"

using System::Diagnostics::Trace;

namespace {
class CerrCapture final {
    std::streambuf* previous_;
    std::ostringstream captured_;

public:
    CerrCapture() : previous_(std::cerr.rdbuf(captured_.rdbuf())) {}
    ~CerrCapture() { std::cerr.rdbuf(previous_); }

    [[nodiscard]] std::string getCaptured() const { return captured_.str(); }
};
} // namespace

TEST(TraceConditionalWriteTest, FalseConditionsDoNotWrite) {
    CerrCapture capture;

    Trace::WriteIf(false, "hidden");
    Trace::WriteLineIf(false, "also hidden");
    Trace::Flush();

    EXPECT_TRUE(capture.getCaptured().empty());
}

TEST(TraceConditionalWriteTest, WriteIfWritesWithoutTrailingNewline) {
    CerrCapture capture;

    Trace::WriteIf(true, "visible");
    Trace::Flush();

    EXPECT_EQ(capture.getCaptured(), "visible");
}

TEST(TraceConditionalWriteTest, WriteLineIfWritesWithTrailingNewline) {
    CerrCapture capture;

    Trace::WriteLineIf(true, "visible");
    Trace::Flush();

    EXPECT_EQ(capture.getCaptured(), "visible\n");
}
