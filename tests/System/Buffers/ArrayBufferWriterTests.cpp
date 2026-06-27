// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Buffers/ArrayBufferWriter.hpp"

using System::Buffers::ArrayBufferWriter;

TEST(ArrayBufferWriterTest, DefaultCtor) {
    ArrayBufferWriter<int> w;
    EXPECT_EQ(w.getWrittenCountProperty(), 0);
    EXPECT_GE(w.getCapacityProperty(), ArrayBufferWriter<int>::DefaultInitialBufferSize);
}

TEST(ArrayBufferWriterTest, CtorWithCapacity) {
    ArrayBufferWriter<int> w(128);
    EXPECT_GE(w.getCapacityProperty(), 128);
}

TEST(ArrayBufferWriterTest, CtorZeroCapacityThrows) {
    EXPECT_THROW(ArrayBufferWriter<int>(0), std::invalid_argument);
}

TEST(ArrayBufferWriterTest, AdvanceIncreasesWrittenCount) {
    ArrayBufferWriter<int> w;
    w.GetSpan(10);
    w.Advance(5);
    EXPECT_EQ(w.getWrittenCountProperty(), 5);
}

TEST(ArrayBufferWriterTest, FreeCapacityDecreases) {
    ArrayBufferWriter<int> w;
    int before = w.getFreeCapacityProperty();
    w.GetSpan(4);
    w.Advance(4);
    EXPECT_EQ(w.getFreeCapacityProperty(), before - 4);
}

TEST(ArrayBufferWriterTest, Clear) {
    ArrayBufferWriter<int> w;
    w.GetSpan(3);
    w.Advance(3);
    w.Clear();
    EXPECT_EQ(w.getWrittenCountProperty(), 0);
}

TEST(ArrayBufferWriterTest, WrittenMemorySize) {
    ArrayBufferWriter<char> w;
    auto sp = w.GetSpan(5);
    sp[0] = 'A'; sp[1] = 'B';
    w.Advance(2);
    EXPECT_EQ(w.getWrittenMemoryProperty().getLengthProperty(), 2);
}
