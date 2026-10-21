// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <tuple>
#include "System/Uri.hpp"
#include "System/InvalidOperationException.hpp"

namespace {
using Row = std::tuple<std::string, bool, bool, std::string>;
class UriLocalPathTest : public testing::TestWithParam<Row> {};

TEST_P(UriLocalPathTest, MatchesMeasuredNet8ClassificationAndLocalPath) {
    const auto& [input, absolute, file, localPath] = GetParam();
    SCOPED_TRACE(input);
    System::Uri uri(input, System::UriKind::RelativeOrAbsolute);
    ASSERT_EQ(uri.getIsAbsoluteUriProperty(), absolute);
    if (!absolute) {
        EXPECT_THROW((void)uri.getIsFileProperty(), System::InvalidOperationException);
        EXPECT_THROW((void)uri.getLocalPathProperty(), System::InvalidOperationException);
    } else {
        EXPECT_EQ(uri.getIsFileProperty(), file);
        std::string expectedPath = localPath;
#ifdef _WIN32
        // The oracle table was measured on .NET8/Linux. File syntax on Windows
        // converts backslashes even in slash-rooted paths (Uri.cs:2077,4197).
        if (file && expectedPath.starts_with('/') && !expectedPath.starts_with("//"))
            std::replace(expectedPath.begin(), expectedPath.end(), '\\', '/');
#endif
        EXPECT_EQ(uri.getLocalPathProperty(), expectedPath);
    }
}

INSTANTIATE_TEST_SUITE_P(CnaIssue101, UriLocalPathTest, testing::Values(
    Row{std::string("file:///tmp/movie.ogv",21),true,true,std::string("/tmp/movie.ogv",14)},
    Row{std::string("FILE:///tmp/%63hroma%20clip.mkv",31),true,true,std::string("/tmp/chroma clip.mkv",20)},
    Row{std::string("file:///tmp/a+b%23c%3Fd%25e",27),true,true,std::string("/tmp/a+b#c?d%e",14)},
    Row{std::string("file:///tmp/%C4%8D%F0%9F%98%80",30),true,true,std::string("/tmp/č😀",11)},
    Row{std::string("file:///tmp/%FF/%C0%AF/%E2%82/%ED%A0%80",39),true,true,std::string("/tmp/%FF/%C0%AF/%E2%82/%ED%A0%80",32)},
    Row{std::string("file:///tmp/%00.mkv",19),true,true,std::string("/tmp/\0.mkv",10)},
    Row{std::string("file:///tmp/a/../movie.ogv",26),true,true,std::string("/tmp/movie.ogv",14)},
    Row{std::string("file:///tmp/a/%2E%2E/movie.ogv",30),true,true,std::string("/tmp/movie.ogv",14)},
    Row{std::string("file:///tmp/a%2Fb%5Cc",21),true,true,std::string("/tmp/a/b\\c",10)},
    Row{std::string("file:///tmp/a//b",16),true,true,std::string("/tmp/a//b",9)},
    Row{std::string("file://localhost/tmp/clip.mkv",29),true,true,std::string("\\\\localhost\\tmp\\clip.mkv",24)},
    Row{std::string("file://LOCALHOST/tmp/clip.mkv",29),true,true,std::string("\\\\localhost\\tmp\\clip.mkv",24)},
    Row{std::string("file://server/share/a%20b.mkv",29),true,true,std::string("\\\\server\\share\\a b.mkv",22)},
    Row{std::string("file:///C:/dir/a%20b.mkv",24),true,true,std::string("C:\\dir\\a b.mkv",14)},
    Row{std::string("file:///C|/dir/a.mkv",20),true,true,std::string("C:\\dir\\a.mkv",12)},
    Row{std::string("file:/tmp/clip.mkv",18),false,false,std::string("",0)},
    Row{std::string("file:relative.mkv",17),false,false,std::string("",0)},
    Row{std::string("file:////server/share/clip.mkv",30),true,true,std::string("\\\\server\\share\\clip.mkv",23)},
    Row{std::string("file:///tmp/%252F/%zz/%",23),true,true,std::string("/tmp/%2F/%zz/%",14)},
    Row{std::string("file:///tmp/clip.mkv?query#fragment",35),true,true,std::string("/tmp/clip.mkv",13)},
    Row{std::string("https://example.invalid/a%20b?query#fragment",44),true,false,std::string("/a b",4)},
    Row{std::string("https://example.invalid/a/../b",30),true,false,std::string("/b",2)},
    Row{std::string("https://example.invalid/a%2Fb%5Cc",33),true,false,std::string("/a/b\\c",6)},
    Row{std::string("mailto:user@example.invalid",27),true,false,std::string("",0)},
    Row{std::string("urn:clip:a%20b",14),true,false,std::string("clip:a b",8)},
    Row{std::string("assets/video.mkv",16),false,false,std::string("",0)},
    Row{std::string("",0),false,false,std::string("",0)},
    Row{std::string("/tmp/clip.mkv",13),false,false,std::string("",0)},
    Row{std::string("C:\\dir\\clip.mkv",15),true,true,std::string("C:\\dir\\clip.mkv",15)},
    Row{std::string("file:///tmp/a\\b",15),true,true,std::string("/tmp/a\\b",8)},
    Row{std::string("file://server/share/a/../../b",29),true,true,std::string("\\\\server\\b",10)},
    Row{std::string("file:///C:/a/../../b",20),true,true,std::string("C:\\b",4)},
    Row{std::string("file:///tmp/%E2%82%AC%FF%41",27),true,true,std::string("/tmp/€%FFA",12)},
    Row{std::string("file:///tmp/%C4x%8D",19),true,true,std::string("/tmp/%C4x%8D",12)}
));
} // namespace

