// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <memory>
#include "System/ArgumentException.hpp"
#include "System/Uri.hpp"
#include "System/UriFormatException.hpp"

TEST(UriEmptyRelativeTest, EmptyAndWhitespaceReferencesRespectTheRequestedKind)
{
    for (const std::string text : {"", " ", "\t\r\n"})
    {
        SCOPED_TRACE(text.size());
        for (const auto kind : {System::UriKind::Relative, System::UriKind::RelativeOrAbsolute})
        {
            SCOPED_TRACE(static_cast<int>(kind));
            try
            {
                const System::Uri uri(text, kind);
                EXPECT_FALSE(uri.getIsAbsoluteUriProperty());
                EXPECT_EQ(uri.getOriginalStringProperty(), text);
                EXPECT_EQ(uri.ToString(), text);
            }
            catch (const std::exception& error) { ADD_FAILURE() << error.what(); }
            std::shared_ptr<System::Uri> result;
            EXPECT_TRUE(System::Uri::TryCreate(text, kind, result));
            if (result) {
                EXPECT_EQ(result->getOriginalStringProperty(), text);
            }
        }
        EXPECT_THROW((void)System::Uri(text), System::UriFormatException);
        EXPECT_THROW(System::Uri(text, System::UriKind::Absolute), System::UriFormatException);
        EXPECT_THROW(System::Uri(text, static_cast<System::UriKind>(99)), System::ArgumentException);
    }
}
