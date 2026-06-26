// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <stdexcept>
#include "System/Index.hpp"

using System::Index;

TEST(IndexTests2, DefaultCtor_IsFromStart_ValueZero) {
    Index idx;
    EXPECT_FALSE(idx.getIsFromEndProperty());
    EXPECT_EQ(idx.getValueProperty(), 0);
}

TEST(IndexTests2, FromStart_CorrectOffset) {
    Index idx = Index::FromStart(2);
    EXPECT_EQ(idx.GetOffset(5), 2);
    EXPECT_FALSE(idx.getIsFromEndProperty());
}

TEST(IndexTests2, FromEnd_CorrectOffset) {
    Index idx = Index::FromEnd(1);
    EXPECT_TRUE(idx.getIsFromEndProperty());
    EXPECT_EQ(idx.GetOffset(5), 4);
}

TEST(IndexTests2, Start_IsZeroFromStart) {
    Index idx = Index::Start();
    EXPECT_EQ(idx.GetOffset(10), 0);
}

TEST(IndexTests2, End_IsZeroFromEnd) {
    Index idx = Index::End();
    EXPECT_EQ(idx.GetOffset(5), 5);
}

TEST(IndexTests2, NegativeValue_Throws) {
    EXPECT_THROW(Index(-1), std::out_of_range);
}

TEST(IndexTests2, GetOffset_OutOfRange_Throws) {
    Index idx = Index::FromStart(10);
    EXPECT_THROW(idx.GetOffset(5), std::out_of_range);
}
