// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/UriCreationOptions.hpp"

using System::UriCreationOptions;

TEST(UriCreationOptionsTest, DefaultCtor) {
    UriCreationOptions opts;
    EXPECT_FALSE(opts.DangerousDisablePathAndQueryCanonicalization);
}

TEST(UriCreationOptionsTest, SetFlag) {
    UriCreationOptions opts;
    opts.DangerousDisablePathAndQueryCanonicalization = true;
    EXPECT_TRUE(opts.DangerousDisablePathAndQueryCanonicalization);
}

TEST(UriCreationOptionsTest, TwoInstancesIndependent) {
    UriCreationOptions a;
    UriCreationOptions b;
    b.DangerousDisablePathAndQueryCanonicalization = true;
    EXPECT_FALSE(a.DangerousDisablePathAndQueryCanonicalization);
    EXPECT_TRUE(b.DangerousDisablePathAndQueryCanonicalization);
}

// =============================================================================================
// Ticket #1997 group A-3 (SR-AUD-149) — the two consumer overloads.
//
// Before this group, a UriCreationOptions could be constructed and read but could NEVER REACH A
// URI OPERATION: neither Uri(string, UriCreationOptions) nor an options-bearing TryCreate
// existed. That was the finding, and the type's own header said so.
//
// The overloads are transcribed from the reference, INCLUDING their kind:
//   Uri(string, in UriCreationOptions)  -> CreateThis(uriString, false, UriKind.Absolute, ...)
//                                                                          (Uri.cs:476-480)
//   TryCreate(string, in UriCreationOptions, out Uri?)
//                                       -> CreateHelper(uriString, false, UriKind.Absolute, ...)
//                                                                          (UriExt.cs:236-240)
// =============================================================================================
#include <memory>
#include "System/Uri.hpp"
#include "System/UriFormatException.hpp"

TEST(UriCreationOptionsTest, Fix1997A3_TheConstructorOverloadExistsAndParses) {
    UriCreationOptions opts;
    System::Uri uri("http://example.com/path", opts);
    EXPECT_EQ(uri.getHostProperty(), "example.com");
    EXPECT_TRUE(uri.getIsAbsoluteUriProperty());
}

TEST(UriCreationOptionsTest, Fix1997A3_TheTryCreateOverloadExists) {
    UriCreationOptions opts;
    std::shared_ptr<System::Uri> result;
    EXPECT_TRUE(System::Uri::TryCreate("http://example.com/path", opts, result));
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getHostProperty(), "example.com");
}

TEST(UriCreationOptionsTest, Decl1997A3_BothOverloadsResolveAgainstAbsoluteNotRelativeOrAbsolute) {
    // .NET passes UriKind.Absolute in BOTH bodies, so a relative string throws through the
    // constructor and fails through TryCreate.
    UriCreationOptions opts;
    EXPECT_THROW(System::Uri("/relative/path", opts), System::UriFormatException);

    std::shared_ptr<System::Uri> result;
    EXPECT_FALSE(System::Uri::TryCreate("/relative/path", opts, result));
    EXPECT_EQ(result, nullptr);

    // UPDATED BY #2393. This case used to end by asserting that the ONE-ARGUMENT constructor
    // still ACCEPTS "/relative/path", and called that "what makes the distinction observable
    // rather than theoretical". It was observable, and it was a DEFECT rather than a
    // distinction: .NET's one-argument constructor is CreateThis(uriString, false,
    // UriKind.Absolute) (Uri.cs:424-429), the same grammar TryCreate uses, so it rejects a
    // relative string too. #2393 made the two agree, and the assertion is inverted.
    EXPECT_THROW((void)System::Uri("/relative/path"), System::UriFormatException)
        << "#2393: the one-argument constructor is UriKind::Absolute, as .NET's is";

    // What genuinely IS a distinction, and what this case is really for: the options overloads
    // resolve against Absolute rather than RelativeOrAbsolute. Asserted against the kind that
    // WOULD have accepted it, so the case still discriminates now that the sibling agrees.
    EXPECT_NO_THROW((void)System::Uri("/relative/path", System::UriKind::RelativeOrAbsolute));
}

TEST(UriCreationOptionsTest, Decl1997A3_TheFlagIsInertAndTheResultIsIdentical) {
    // THE HALF SR-AUD-149 DOES NOT CLOSE, asserted rather than merely documented. .NET's flag
    // disables path/query canonicalisation; this port performs none, so setting it changes
    // nothing. If this ever starts failing, the port has grown canonicalisation and the header's
    // disclosure must be revisited.
    UriCreationOptions off;
    UriCreationOptions on;
    on.DangerousDisablePathAndQueryCanonicalization = true;

    for (const char* text : {"http://example.com/a/./b/../c?q=1&r=%2F",
                             "http://example.com/%zz/trailing%",
                             "https://example.com:443/path#frag"}) {
        System::Uri a(text, off);
        System::Uri b(text, on);
        EXPECT_EQ(a.getOriginalStringProperty(), b.getOriginalStringProperty()) << text;
        EXPECT_EQ(a.getAbsolutePathProperty(),   b.getAbsolutePathProperty())   << text;
        EXPECT_EQ(a.getQueryProperty(),          b.getQueryProperty())          << text;
        EXPECT_EQ(a.getHostProperty(),           b.getHostProperty())           << text;
    }
}

TEST(UriCreationOptionsTest, Decl1997A3_TheOptionsAreNotMutatedByEitherOverload) {
    // The caller's object is theirs; neither overload may write through it.
    UriCreationOptions opts;
    opts.DangerousDisablePathAndQueryCanonicalization = true;
    System::Uri uri("http://example.com/", opts);
    EXPECT_TRUE(opts.DangerousDisablePathAndQueryCanonicalization);

    std::shared_ptr<System::Uri> result;
    (void)System::Uri::TryCreate("http://example.com/", opts, result);
    EXPECT_TRUE(opts.DangerousDisablePathAndQueryCanonicalization);
}
