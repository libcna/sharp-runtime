// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for the compatible tickets of the `modules/io` namespace review
// (docs/SystemIONamespaceReviewPlan.md, owning ticket #2097).
//
//   #2101 / SR-AUD-347, cause I-C — a raw std::filesystem_error escaped FileInfo("") and
//          DirectoryInfo(""), a std:: exception crossing a System-shaped public API.
//   #2103 / SR-AUD-345, cause I-F — FileInfo::Delete DELETED a directory while its sibling
//          File::Delete threw on the same input. The only finding in this namespace that
//          destroys user data.
//
// Reference tree absent: every exception type and paramName asserted here is recorded as this
// port's choice in the plan, not as a verified match to .NET.
#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "System/ArgumentException.hpp"
#include "System/IO/Directory.hpp"
#include "System/IO/DirectoryInfo.hpp"
#include "System/IO/File.hpp"
#include "System/IO/FileInfo.hpp"
#include "System/IO/IOException.hpp"

using namespace System::IO;

namespace {

/// A unique directory under the repository-local build tree — never /tmp, per the build-resource
/// policy, and never a fixed path that two runs could collide on.
class IoReviewFixture : public ::testing::Test {
protected:
    std::filesystem::path root;

    void SetUp() override {
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        root = std::filesystem::path("build-tmp") / "io_review" /
               (std::string(info->test_suite_name()) + "_" + info->name());
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root);
    }
    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }
    [[nodiscard]] std::string under(const std::string& leaf) const {
        return (root / leaf).string();
    }
};

} // namespace

// ===========================================================================
// #2101 / SR-AUD-347 — no std:: exception may escape a System-shaped API
// ===========================================================================

TEST_F(IoReviewFixture, EmptyPathFileInfoThrowsASystemException) {
    // Before #2101 this threw std::filesystem::filesystem_error("cannot make absolute path"),
    // which a caller writing `catch (const System::Exception&)` never saw.
    EXPECT_THROW(FileInfo(""), System::ArgumentException);
}

TEST_F(IoReviewFixture, EmptyPathDirectoryInfoThrowsASystemException) {
    EXPECT_THROW(DirectoryInfo(""), System::ArgumentException);
}

TEST_F(IoReviewFixture, TheEmptyPathRejectionNamesItsParameter) {
    try {
        FileInfo fi("");
        FAIL() << "an empty path must be rejected";
    } catch (const System::ArgumentException& e) {
        const std::string message = e.what();
        EXPECT_NE(message.find("path"), std::string::npos) << message;
        EXPECT_NE(message.find("empty"), std::string::npos) << message;
    }
}

TEST_F(IoReviewFixture, NoStdExceptionEscapesAnEmptyPathConstruction) {
    // The load-bearing assertion: it is not enough that *some* exception is thrown — it must
    // not be a std:: one. Catching System::Exception first and std::exception second
    // distinguishes them.
    for (int which = 0; which < 2; ++which) {
        bool systemException = false, stdException = false;
        try {
            if (which == 0) { FileInfo fi(""); (void)fi.getExistsProperty(); }
            else            { DirectoryInfo di(""); (void)di.getExistsProperty(); }
        } catch (const System::Exception&) {
            systemException = true;
        } catch (const std::exception&) {
            stdException = true;
        }
        EXPECT_TRUE(systemException) << "which=" << which;
        EXPECT_FALSE(stdException) << "a std:: exception must not cross this API; which=" << which;
    }
}

TEST_F(IoReviewFixture, AValidPathIsCompletelyUnaffected) {
    const std::string file = under("ok.txt");
    File::WriteAllText(file, "hello");
    FileInfo fi(file);
    EXPECT_TRUE(fi.getExistsProperty());
    EXPECT_EQ(fi.getLengthProperty(), 5);
    DirectoryInfo di(root.string());
    EXPECT_TRUE(di.getExistsProperty());
    // A relative path still resolves, and ToString() still returns the ORIGINAL text rather
    // than the resolved one — the pre-existing contract, unchanged by #2101.
    FileInfo relative("build-tmp");
    EXPECT_EQ(relative.ToString(), "build-tmp");
}

TEST_F(IoReviewFixture, AWhitespaceOnlyPathIsStillAcceptedDeliberately) {
    // PIN of a deliberate non-narrowing. A whitespace-only path resolves cleanly on POSIX and
    // no repository evidence says .NET rejects it at this door, so #2101 does NOT narrow it —
    // narrowing on a guess is what the review's deferred-verification rule exists to prevent.
    // If this ever starts throwing, that was a decision and it must be recorded.
    EXPECT_NO_THROW({ FileInfo fi("   "); (void)fi.getExistsProperty(); });
}

TEST_F(IoReviewFixture, TheAlreadyGuardedNeighbouringDoorsStayGuarded) {
    // Measured during the review (build-probe/2101_probe2_before.log): the sites a grep for
    // throwing std::filesystem calls flagged — DirectoryInfo's enumeration and the Copy/Move
    // destination handling — were ALREADY guarded and threw System exceptions. #2101's real
    // scope was one door, not seven. These assertions keep that true.
    DirectoryInfo missing(under("no_such_dir"));
    EXPECT_THROW((void)missing.GetFiles(), System::Exception);
    EXPECT_THROW((void)missing.GetDirectories(), System::Exception);

    const std::string file = under("src.txt");
    File::WriteAllText(file, "x");
    FileInfo fi(file);
    EXPECT_THROW((void)fi.CopyTo(""), System::Exception);
}

// ===========================================================================
// #2103 / SR-AUD-345 — FileInfo::Delete must not delete a directory
// ===========================================================================

TEST_F(IoReviewFixture, FileInfoDeleteOverAnEmptyDirectoryThrowsAndTheDirectorySurvives) {
    // Before #2103 this SILENTLY DELETED the directory: FileInfo::Delete called
    // std::filesystem::remove, which happily removes an empty directory. The surviving
    // directory is the whole point of the assertion — a throw alone would not prove the data
    // was not destroyed.
    const std::string dir = under("adir");
    std::filesystem::create_directories(dir);
    FileInfo fi(dir);
    EXPECT_THROW(fi.Delete(), System::Exception);
    EXPECT_TRUE(std::filesystem::exists(dir)) << "the directory must still exist";
    EXPECT_TRUE(std::filesystem::is_directory(dir));
}

TEST_F(IoReviewFixture, FileInfoDeleteOverANonEmptyDirectoryThrowsAndItsContentsSurvive) {
    const std::string dir = under("full");
    std::filesystem::create_directories(dir);
    File::WriteAllText(dir + "/inner.txt", "keep me");
    FileInfo fi(dir);
    EXPECT_THROW(fi.Delete(), System::Exception);
    EXPECT_TRUE(std::filesystem::exists(dir));
    EXPECT_TRUE(std::filesystem::exists(dir + "/inner.txt"));
    EXPECT_EQ(File::ReadAllText(dir + "/inner.txt"), "keep me");
}

TEST_F(IoReviewFixture, FileInfoDeleteOverARealFileStillWorks) {
    const std::string file = under("gone.txt");
    File::WriteAllText(file, "bye");
    FileInfo fi(file);
    EXPECT_NO_THROW(fi.Delete());
    EXPECT_FALSE(std::filesystem::exists(file));
}

TEST_F(IoReviewFixture, FileInfoDeleteAndFileDeleteNowAgreeOnEveryInput) {
    // The sharper framing the review found (plan §6.4): the finding names only FileInfo, but
    // the defect is that two APIs .NET documents as equivalent gave OPPOSITE answers. File::Delete
    // was already correct, so it is the reference — the repair target already existed in the
    // module rather than needing to be invented.
    const std::string dir = under("agree_dir");
    std::filesystem::create_directories(dir);

    bool fileDeleteThrew = false, fileInfoDeleteThrew = false;
    try { File::Delete(dir); } catch (const System::Exception&) { fileDeleteThrew = true; }
    try { FileInfo(dir).Delete(); } catch (const System::Exception&) { fileInfoDeleteThrew = true; }

    EXPECT_TRUE(fileDeleteThrew) << "File::Delete was already correct";
    EXPECT_EQ(fileInfoDeleteThrew, fileDeleteThrew) << "the two siblings must agree";
    EXPECT_TRUE(std::filesystem::exists(dir)) << "neither may have deleted it";
}

TEST_F(IoReviewFixture, DirectoryInfoDeleteStillDeletesADirectory) {
    // The repair must not leak onto the type whose job this actually is.
    const std::string dir = under("di_dir");
    std::filesystem::create_directories(dir);
    DirectoryInfo di(dir);
    EXPECT_NO_THROW(di.Delete());
    EXPECT_FALSE(std::filesystem::exists(dir));
}
