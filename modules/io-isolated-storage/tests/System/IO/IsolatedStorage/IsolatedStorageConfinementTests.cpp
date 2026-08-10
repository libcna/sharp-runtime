// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Root-confinement suite for System::IO::IsolatedStorage (#2204, SR-AUD-241).
//
// The store's defining promise is that every caller-supplied path is relative to the storage
// root.  Before #2204 that promise was false: `fullPath()` was `rootDirectory_ / relativePath`,
// and std::filesystem's operator/ DISCARDS the root when the right operand is absolute, so all
// four effect classes -- read, create/write, delete and rename -- escaped the store.  `..`
// traversal and symbolic links escaped too.
//
// Every test builds its own sandbox under the process working directory (the gate runs test
// executables from `build/`), never /tmp, /var/tmp, /dev/shm or $HOME, and removes it again.
// The "outside the store" target is always a sibling directory inside that same sandbox.

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <limits>
#include <vector>

#include "System/ArgumentException.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/IsolatedStorage/IsolatedStorageException.hpp"
#include "System/IO/IsolatedStorage/IsolatedStorageFile.hpp"
#include "System/IO/IsolatedStorage/IsolatedStorageFileStream.hpp"
#include "System/IO/IsolatedStorage/IsolatedStorageScope.hpp"
#include "System/ObjectDisposedException.hpp"

namespace fs = std::filesystem;

using System::IO::FileMode;
using System::IO::IsolatedStorage::IsolatedStorageException;
using System::IO::IsolatedStorage::IsolatedStorageFile;
using System::IO::IsolatedStorage::IsolatedStorageScope;

namespace {

    // The one directory every fixture lives under, relative to the process working directory.
    const char* const kSandboxRoot = "sharp_rt_iso_confinement";

    // Removes the shared parent once the whole suite has finished, so a successful run leaves
    // no artifact behind at all -- not even an empty directory.
    class SandboxCleanup : public ::testing::Environment
    {
    public:
        void TearDown() override
        {
            std::error_code ec;
            fs::remove_all(fs::current_path() / kSandboxRoot, ec);
        }
    };

    const auto* const kSandboxCleanupRegistration =
        ::testing::AddGlobalTestEnvironment(new SandboxCleanup());

    class IsolatedStorageConfinementTest : public ::testing::Test
    {
    protected:
        fs::path sandbox_;
        fs::path root_;
        fs::path outside_;

        void SetUp() override
        {
            const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
            sandbox_ = fs::current_path() / kSandboxRoot
                     / (std::string(info->test_suite_name()) + "." + info->name());
            std::error_code ec;
            fs::remove_all(sandbox_, ec);
            root_ = sandbox_ / "store";
            outside_ = sandbox_ / "outside";
            fs::create_directories(root_);
            fs::create_directories(outside_);
        }

        void TearDown() override
        {
            // remove_all removes a symbolic link as a link; it does not descend through one,
            // so a link planted by a symlink test can never delete outside the sandbox.
            std::error_code ec;
            fs::remove_all(sandbox_, ec);
        }

        [[nodiscard]] IsolatedStorageFile store() const
        {
            return IsolatedStorageFile(root_, IsolatedStorageScope::None);
        }

        static void writeFile(const fs::path& p, const std::string& content)
        {
            fs::create_directories(p.parent_path());
            std::ofstream out(p, std::ios::binary | std::ios::trunc);
            out << content;
        }

        // Every entry beneath `dir`, relative and sorted -- a snapshot for the
        // "a rejected call has no partial effect" assertions.
        static std::vector<std::string> snapshot(const fs::path& dir)
        {
            std::vector<std::string> entries;
            std::error_code ec;
            for (fs::recursive_directory_iterator it(dir, ec), end; it != end; it.increment(ec))
                entries.push_back(it->path().lexically_relative(dir).string());
            std::sort(entries.begin(), entries.end());
            return entries;
        }

        // Asserts the operation is refused as an out-of-contract argument, naming the public
        // parameter it arrived in.  Written as a helper so every door pins the same contract.
        template <typename F>
        static void expectRejected(F&& op, const char* paramName)
        {
            try {
                op();
                ADD_FAILURE() << "expected ArgumentException for parameter " << paramName;
            } catch (const System::ArgumentException& e) {
                EXPECT_EQ(e.getParamNameProperty(), std::string(paramName));
            } catch (const std::exception& e) {
                ADD_FAILURE() << "expected ArgumentException, got: " << e.what();
            }
        }
    };

    // =====================================================================================
    // The control: what SR-AUD-241 actually was.
    //
    // This does not exercise the store at all -- it pins the std::filesystem behaviour the
    // shipped `fullPath()` relied on.  Without it, every "rejected" verdict below could be
    // explained by the escape never having been possible in this environment.
    // =====================================================================================

    TEST_F(IsolatedStorageConfinementTest, Control_RawJoinStillDiscardsTheRootForAnAbsolutePath)
    {
        const fs::path victim = outside_ / "control.dat";
        EXPECT_EQ(root_ / victim.string(), victim)
            << "operator/ no longer discards an absolute right operand; the premise of "
               "SR-AUD-241 changed and this suite must be re-derived";
        EXPECT_NE((root_ / fs::path("../outside/control.dat")).lexically_normal(),
                  (root_ / fs::path("outside/control.dat")).lexically_normal())
            << "a lexical `..` no longer climbs; the traversal premise changed";
    }

    // =====================================================================================
    // SR-AUD-241, exactly as the audit recorded it.
    // =====================================================================================

    TEST_F(IsolatedStorageConfinementTest, AuditProbe_CreateDirectoryWithAbsolutePath_StaysInsideTheStore)
    {
        // The audit's probe printed `escaped_exists=1 root_child_exists=0`: the directory was
        // created outside the store and nothing appeared inside it.  Both halves invert here.
        auto s = store();
        const fs::path escapeTarget = outside_ / "escaped_dir";
        s.CreateDirectory(escapeTarget.string());

        EXPECT_FALSE(fs::exists(escapeTarget)) << "the store escaped its root";
        EXPECT_TRUE(fs::exists(root_ / escapeTarget.relative_path()))
            << "a rooted path must be reinterpreted as store-relative, matching .NET's "
               "IsolatedStorageFile.GetFullPath";
    }

    // =====================================================================================
    // Absolute paths -- one test per escaping argument measured pre-repair.
    // =====================================================================================

    TEST_F(IsolatedStorageConfinementTest, AbsolutePath_FileExists_DoesNotSeeOutsideTheStore)
    {
        writeFile(outside_ / "secret.txt", "secret");
        EXPECT_FALSE(store().FileExists((outside_ / "secret.txt").string()));
    }

    TEST_F(IsolatedStorageConfinementTest, AbsolutePath_DirectoryExists_DoesNotSeeOutsideTheStore)
    {
        fs::create_directories(outside_ / "visible_dir");
        EXPECT_FALSE(store().DirectoryExists((outside_ / "visible_dir").string()));
    }

    TEST_F(IsolatedStorageConfinementTest, AbsolutePath_OpenFile_CreatesNothingOutsideTheStore)
    {
        auto s = store();
        const fs::path target = outside_ / "opened.dat";
        { auto stream = s.OpenFile(target.string(), FileMode::Create); stream.Close(); }
        EXPECT_FALSE(fs::exists(target));
    }

    TEST_F(IsolatedStorageConfinementTest, AbsolutePath_CreateFile_CreatesNothingOutsideTheStore)
    {
        auto s = store();
        const fs::path target = outside_ / "created.dat";
        { auto stream = s.CreateFile(target.string()); stream.Close(); }
        EXPECT_FALSE(fs::exists(target));
    }

    TEST_F(IsolatedStorageConfinementTest, AbsolutePath_DeleteFile_LeavesTheOutsideVictimAlone)
    {
        const fs::path victim = outside_ / "victim.dat";
        writeFile(victim, "keep me");
        auto s = store();
        // Reinterpreted store-relative, the target does not exist; the door must not reach out.
        try { s.DeleteFile(victim.string()); } catch (const IsolatedStorageException&) { }
        EXPECT_TRUE(fs::exists(victim)) << "an outside file was deleted through the store";
    }

    TEST_F(IsolatedStorageConfinementTest, AbsolutePath_DeleteDirectory_LeavesTheOutsideVictimAlone)
    {
        const fs::path victim = outside_ / "victim_dir";
        fs::create_directories(victim);
        auto s = store();
        try { s.DeleteDirectory(victim.string()); } catch (const IsolatedStorageException&) { }
        EXPECT_TRUE(fs::exists(victim)) << "an outside directory was deleted through the store";
    }

    TEST_F(IsolatedStorageConfinementTest, AbsolutePath_CopyFileDestination_WritesNothingOutsideTheStore)
    {
        auto s = store();
        { auto stream = s.CreateFile("inside.dat"); stream.Close(); }
        const fs::path target = outside_ / "copied_out.dat";
        // Reinterpreted store-relative the destination lands deep inside the store where no
        // parent exists, so the copy fails -- with the module's own exception, not by escaping.
        try { s.CopyFile("inside.dat", target.string()); }
        catch (const IsolatedStorageException&) { }
        EXPECT_FALSE(fs::exists(target));
    }

    TEST_F(IsolatedStorageConfinementTest, AbsolutePath_CopyFileSource_ImportsNothingFromOutside)
    {
        writeFile(outside_ / "secret.txt", "secret");
        auto s = store();
        try { s.CopyFile((outside_ / "secret.txt").string(), "leaked.dat"); }
        catch (const IsolatedStorageException&) { }
        EXPECT_FALSE(fs::exists(root_ / "leaked.dat"))
            << "outside content was imported into the store";
    }

    TEST_F(IsolatedStorageConfinementTest, AbsolutePath_MoveFileDestination_MovesNothingOutOfTheStore)
    {
        auto s = store();
        { auto stream = s.CreateFile("movable.dat"); stream.Close(); }
        const fs::path target = outside_ / "moved_out.dat";
        try { s.MoveFile("movable.dat", target.string()); }
        catch (const IsolatedStorageException&) { }
        EXPECT_FALSE(fs::exists(target));
        EXPECT_TRUE(s.FileExists("movable.dat")) << "the source left the store";
    }

    TEST_F(IsolatedStorageConfinementTest, AbsolutePath_MoveDirectorySource_ImportsNothingFromOutside)
    {
        fs::create_directories(outside_ / "movable_dir");
        auto s = store();
        try { s.MoveDirectory((outside_ / "movable_dir").string(), "pulled_in"); }
        catch (const IsolatedStorageException&) { }
        EXPECT_FALSE(fs::exists(root_ / "pulled_in"));
        EXPECT_TRUE(fs::exists(outside_ / "movable_dir")) << "an outside directory was moved";
    }

    TEST_F(IsolatedStorageConfinementTest, LeadingSeparator_IsReinterpretedNotHonoured)
    {
        // .NET's GetFullPath strips leading separators before Path.Combine, so this names a
        // file inside the store rather than one at the filesystem root.
        auto s = store();
        { auto stream = s.CreateFile("/data.bin"); stream.Close(); }
        EXPECT_TRUE(fs::exists(root_ / "data.bin"));
        EXPECT_TRUE(s.FileExists("/data.bin"));
        EXPECT_TRUE(s.FileExists("data.bin"));
    }

    // =====================================================================================
    // Lexical `..` traversal -- rejected, with the declared parameter name.
    // =====================================================================================

    TEST_F(IsolatedStorageConfinementTest, Traversal_CreateDirectory_IsRejected)
    {
        auto s = store();
        expectRejected([&] { s.CreateDirectory("../outside/traversed"); }, "relativePath");
        EXPECT_FALSE(fs::exists(outside_ / "traversed"));
    }

    TEST_F(IsolatedStorageConfinementTest, Traversal_CreateFile_IsRejected)
    {
        auto s = store();
        expectRejected([&] { auto st = s.CreateFile("../outside/traversed.dat"); st.Close(); },
                       "relativePath");
        EXPECT_FALSE(fs::exists(outside_ / "traversed.dat"));
    }

    TEST_F(IsolatedStorageConfinementTest, Traversal_OpenFile_IsRejected)
    {
        auto s = store();
        expectRejected(
            [&] { auto st = s.OpenFile("../outside/traversed.dat", FileMode::Create); st.Close(); },
            "relativePath");
        EXPECT_FALSE(fs::exists(outside_ / "traversed.dat"));
    }

    TEST_F(IsolatedStorageConfinementTest, Traversal_FileExists_IsRejected)
    {
        writeFile(outside_ / "secret.txt", "secret");
        auto s = store();
        expectRejected([&] { (void)s.FileExists("../outside/secret.txt"); }, "relativePath");
    }

    TEST_F(IsolatedStorageConfinementTest, Traversal_DirectoryExists_IsRejected)
    {
        auto s = store();
        expectRejected([&] { (void)s.DirectoryExists(".."); }, "relativePath");
        expectRejected([&] { (void)s.DirectoryExists("../outside"); }, "relativePath");
    }

    TEST_F(IsolatedStorageConfinementTest, Traversal_DeleteFile_IsRejectedAndTheVictimSurvives)
    {
        const fs::path victim = outside_ / "victim.dat";
        writeFile(victim, "keep me");
        auto s = store();
        expectRejected([&] { s.DeleteFile("../outside/victim.dat"); }, "relativePath");
        EXPECT_TRUE(fs::exists(victim));
    }

    TEST_F(IsolatedStorageConfinementTest, Traversal_DeleteDirectory_IsRejectedAndTheVictimSurvives)
    {
        const fs::path victim = outside_ / "victim_dir";
        fs::create_directories(victim);
        auto s = store();
        expectRejected([&] { s.DeleteDirectory("../outside/victim_dir"); }, "relativePath");
        EXPECT_TRUE(fs::exists(victim));
    }

    TEST_F(IsolatedStorageConfinementTest, Traversal_MixedSegments_IsRejected)
    {
        auto s = store();
        expectRejected([&] { s.CreateDirectory("a/../../outside/mixed"); }, "relativePath");
        EXPECT_FALSE(fs::exists(outside_ / "mixed"));
    }

    TEST_F(IsolatedStorageConfinementTest, Traversal_RepeatedDotDot_IsRejected)
    {
        auto s = store();
        expectRejected([&] { s.CreateDirectory("../../../../../../etc/evil"); }, "relativePath");
    }

    TEST_F(IsolatedStorageConfinementTest, Traversal_CopyFile_NamesTheOffendingParameter)
    {
        auto s = store();
        { auto st = s.CreateFile("inside.dat"); st.Close(); }
        expectRejected([&] { s.CopyFile("../outside/secret.txt", "dst.dat"); }, "sourceFileName");
        expectRejected([&] { s.CopyFile("inside.dat", "../outside/dst.dat"); },
                       "destinationFileName");
        EXPECT_FALSE(fs::exists(outside_ / "dst.dat"));
    }

    TEST_F(IsolatedStorageConfinementTest, Traversal_MoveFile_NamesTheOffendingParameter)
    {
        auto s = store();
        { auto st = s.CreateFile("inside.dat"); st.Close(); }
        expectRejected([&] { s.MoveFile("../outside/secret.txt", "dst.dat"); }, "sourceFileName");
        expectRejected([&] { s.MoveFile("inside.dat", "../outside/dst.dat"); },
                       "destinationFileName");
        EXPECT_TRUE(s.FileExists("inside.dat")) << "the source was disturbed by a rejected move";
        EXPECT_FALSE(fs::exists(outside_ / "dst.dat"));
    }

    TEST_F(IsolatedStorageConfinementTest, Traversal_MoveDirectory_NamesTheOffendingParameter)
    {
        auto s = store();
        s.CreateDirectory("inside_dir");
        expectRejected([&] { s.MoveDirectory("..", "stolen"); }, "sourceDirectoryName");
        expectRejected([&] { s.MoveDirectory("inside_dir", "../outside/stolen"); },
                       "destinationDirectoryName");
        EXPECT_TRUE(s.DirectoryExists("inside_dir"));
        EXPECT_FALSE(fs::exists(outside_ / "stolen"));
    }

    // =====================================================================================
    // Over-rejection guard: legitimate relative paths must keep working.
    // =====================================================================================

    TEST_F(IsolatedStorageConfinementTest, LegitimatePaths_NestedRelativePathStillWorks)
    {
        auto s = store();
        s.CreateDirectory("a/b/c");
        EXPECT_TRUE(s.DirectoryExists("a/b/c"));
        { auto st = s.CreateFile("a/b/c/leaf.dat"); st.Close(); }
        EXPECT_TRUE(s.FileExists("a/b/c/leaf.dat"));
        EXPECT_TRUE(fs::exists(root_ / "a" / "b" / "c" / "leaf.dat"));
    }

    TEST_F(IsolatedStorageConfinementTest, LegitimatePaths_CancellingDotDotIsNotAnEscape)
    {
        // `a/../b` normalises to `b`, which is inside; rejecting it would be over-rejection.
        auto s = store();
        s.CreateDirectory("a/../b");
        EXPECT_TRUE(fs::exists(root_ / "b"));
        EXPECT_TRUE(s.DirectoryExists("a/../b"));
    }

    TEST_F(IsolatedStorageConfinementTest, LegitimatePaths_NameBeginningWithDotDotIsNotTraversal)
    {
        auto s = store();
        s.CreateDirectory("..hidden");
        EXPECT_TRUE(fs::exists(root_ / "..hidden"));
        { auto st = s.CreateFile("..config"); st.Close(); }
        EXPECT_TRUE(s.FileExists("..config"));
    }

    TEST_F(IsolatedStorageConfinementTest, LegitimatePaths_SeparatorAndDotSegmentForms)
    {
        auto s = store();
        s.CreateDirectory("a//b");             // repeated separator
        EXPECT_TRUE(fs::exists(root_ / "a" / "b"));
        s.CreateDirectory("trailing/");        // trailing separator
        EXPECT_TRUE(fs::exists(root_ / "trailing"));
        s.CreateDirectory("./dot_segment");    // leading "." segment
        EXPECT_TRUE(fs::exists(root_ / "dot_segment"));
        s.CreateDirectory("x/./y");            // interior "." segment
        EXPECT_TRUE(fs::exists(root_ / "x" / "y"));
    }

    TEST_F(IsolatedStorageConfinementTest, LegitimatePaths_UnusualButValidNames)
    {
        auto s = store();
        { auto st = s.CreateFile(".hidden_dotfile"); st.Close(); }
        EXPECT_TRUE(s.FileExists(".hidden_dotfile"));
        { auto st = s.CreateFile("name with spaces.dat"); st.Close(); }
        EXPECT_TRUE(s.FileExists("name with spaces.dat"));
        { auto st = s.CreateFile("üñîćøde.dat"); st.Close(); }
        EXPECT_TRUE(s.FileExists("üñîćøde.dat"));
        { auto st = s.CreateFile("a.b.c.tar.gz"); st.Close(); }
        EXPECT_TRUE(s.FileExists("a.b.c.tar.gz"));
    }

    TEST_F(IsolatedStorageConfinementTest, LegitimatePaths_LongComponentAndLongPath)
    {
        auto s = store();
        const std::string longComponent(200, 'n');   // under NAME_MAX (255)
        { auto st = s.CreateFile(longComponent); st.Close(); }
        EXPECT_TRUE(s.FileExists(longComponent));

        std::string deep;
        for (int i = 0; i < 20; ++i) deep += "dir" + std::to_string(i) + "/";
        s.CreateDirectory(deep);
        EXPECT_TRUE(s.DirectoryExists(deep));
    }

    // =====================================================================================
    // Degenerate inputs.
    // =====================================================================================

    TEST_F(IsolatedStorageConfinementTest, Degenerate_EmptyPathIsRejectedAtEveryDoor)
    {
        auto s = store();
        expectRejected([&] { (void)s.FileExists(""); }, "relativePath");
        expectRejected([&] { (void)s.DirectoryExists(""); }, "relativePath");
        expectRejected([&] { s.CreateDirectory(""); }, "relativePath");
        expectRejected([&] { s.DeleteFile(""); }, "relativePath");
        expectRejected([&] { s.DeleteDirectory(""); }, "relativePath");
        expectRejected([&] { auto st = s.OpenFile("", FileMode::Create); st.Close(); },
                       "relativePath");
    }

    TEST_F(IsolatedStorageConfinementTest, Degenerate_SeparatorOnlyPathsAreRejected)
    {
        auto s = store();
        // These strip to nothing, so they name the store root itself.
        expectRejected([&] { s.DeleteFile("/"); }, "relativePath");
        expectRejected([&] { s.DeleteDirectory("///"); }, "relativePath");
        expectRejected([&] { (void)s.DirectoryExists("/"); }, "relativePath");
    }

    TEST_F(IsolatedStorageConfinementTest, Degenerate_DotPathNamesTheRootAndIsRejected)
    {
        auto s = store();
        expectRejected([&] { (void)s.DirectoryExists("."); }, "relativePath");
        expectRejected([&] { s.DeleteDirectory("."); }, "relativePath");
        expectRejected([&] { s.DeleteDirectory("a/.."); }, "relativePath");
    }

    TEST_F(IsolatedStorageConfinementTest, Degenerate_EmbeddedNulIsRejectedNotTruncated)
    {
        // Every native filesystem call truncates at the NUL, so accepting this would operate
        // on a different object than the caller named.
        std::string withNul("truncate_me");
        withNul.push_back('\0');
        withNul += "tail";

        auto s = store();
        expectRejected([&] { s.CreateDirectory(withNul); }, "relativePath");
        EXPECT_FALSE(fs::exists(root_ / "truncate_me"));
        expectRejected([&] { auto st = s.CreateFile(withNul); st.Close(); }, "relativePath");
        expectRejected([&] { s.CopyFile(withNul, "dst.dat"); }, "sourceFileName");
    }

    // =====================================================================================
    // The store root itself must survive every path-taking door.
    // =====================================================================================

    TEST_F(IsolatedStorageConfinementTest, RootProtection_DeleteFileEmptyDoesNotDeleteTheRoot)
    {
        // Measured pre-repair: fullPath("") was the root, std::filesystem::remove removes an
        // empty directory, and the store deleted itself.
        auto s = store();
        expectRejected([&] { s.DeleteFile(""); }, "relativePath");
        EXPECT_TRUE(fs::exists(root_)) << "the store root was deleted";
        EXPECT_TRUE(fs::is_directory(root_));
    }

    TEST_F(IsolatedStorageConfinementTest, RootProtection_DeleteDirectoryEmptyDoesNotDeleteTheRoot)
    {
        auto s = store();
        expectRejected([&] { s.DeleteDirectory(""); }, "relativePath");
        expectRejected([&] { s.DeleteDirectory("/"); }, "relativePath");
        EXPECT_TRUE(fs::is_directory(root_));
    }

    TEST_F(IsolatedStorageConfinementTest, RootProtection_MoveDirectoryCannotRenameTheRoot)
    {
        auto s = store();
        expectRejected([&] { s.MoveDirectory("", "renamed"); }, "sourceDirectoryName");
        expectRejected([&] { s.MoveDirectory(".", "renamed"); }, "sourceDirectoryName");
        EXPECT_TRUE(fs::is_directory(root_));
    }

    // =====================================================================================
    // Symbolic links, planted before the call.  Containment is judged on the resolved
    // location, so a link is not forbidden -- leaving the root is.
    // =====================================================================================

    TEST_F(IsolatedStorageConfinementTest, Symlink_InsideToInsideIsAllowed)
    {
        auto s = store();
        s.CreateDirectory("real_dir");
        std::error_code ec;
        fs::create_directory_symlink(root_ / "real_dir", root_ / "link_dir", ec);
        ASSERT_FALSE(ec) << "symbolic links are unavailable here: " << ec.message();

        { auto st = s.CreateFile("link_dir/through_link.dat"); st.Close(); }
        EXPECT_TRUE(fs::exists(root_ / "real_dir" / "through_link.dat"))
            << "a link whose target is inside the root must not be rejected";
    }

    TEST_F(IsolatedStorageConfinementTest, Symlink_FinalComponentToOutsideFileIsRejected)
    {
        const fs::path secret = outside_ / "linked_secret.txt";
        writeFile(secret, "secret");
        std::error_code ec;
        fs::create_symlink(secret, root_ / "filelink", ec);
        ASSERT_FALSE(ec) << ec.message();

        auto s = store();
        expectRejected([&] { (void)s.FileExists("filelink"); }, "relativePath");
        expectRejected([&] { s.DeleteFile("filelink"); }, "relativePath");
        EXPECT_TRUE(fs::exists(secret)) << "the link's outside target was deleted";
    }

    TEST_F(IsolatedStorageConfinementTest, Symlink_IntermediateComponentToOutsideDirectoryIsRejected)
    {
        std::error_code ec;
        fs::create_directory_symlink(outside_, root_ / "dirlink", ec);
        ASSERT_FALSE(ec) << ec.message();

        auto s = store();
        expectRejected([&] { auto st = s.CreateFile("dirlink/written.dat"); st.Close(); },
                       "relativePath");
        EXPECT_FALSE(fs::exists(outside_ / "written.dat"));
        expectRejected([&] { s.CreateDirectory("dirlink/made"); }, "relativePath");
        EXPECT_FALSE(fs::exists(outside_ / "made"));
        expectRejected([&] { (void)s.DirectoryExists("dirlink"); }, "relativePath");
    }

    TEST_F(IsolatedStorageConfinementTest, Symlink_ChainToOutsideIsRejected)
    {
        std::error_code ec;
        fs::create_directory_symlink(outside_, root_ / "hop1", ec);
        ASSERT_FALSE(ec) << ec.message();
        fs::create_directory_symlink(root_ / "hop1", root_ / "hop2", ec);
        ASSERT_FALSE(ec) << ec.message();

        auto s = store();
        expectRejected([&] { auto st = s.CreateFile("hop2/chained.dat"); st.Close(); },
                       "relativePath");
        EXPECT_FALSE(fs::exists(outside_ / "chained.dat"));
    }

    TEST_F(IsolatedStorageConfinementTest, Symlink_DanglingLinkInsideTheRootIsNotAnEscape)
    {
        std::error_code ec;
        fs::create_symlink(root_ / "never_created.dat", root_ / "dangling", ec);
        ASSERT_FALSE(ec) << ec.message();

        auto s = store();
        // Contained but non-existent: a plain false, not a rejection.
        EXPECT_FALSE(s.FileExists("dangling"));
    }

    TEST_F(IsolatedStorageConfinementTest, Symlink_CopyAndMoveDestinationsAreCheckedToo)
    {
        std::error_code ec;
        fs::create_directory_symlink(outside_, root_ / "dirlink", ec);
        ASSERT_FALSE(ec) << ec.message();

        auto s = store();
        { auto st = s.CreateFile("inside.dat"); st.Close(); }
        expectRejected([&] { s.CopyFile("inside.dat", "dirlink/copied.dat"); },
                       "destinationFileName");
        expectRejected([&] { s.MoveFile("inside.dat", "dirlink/moved.dat"); },
                       "destinationFileName");
        EXPECT_FALSE(fs::exists(outside_ / "copied.dat"));
        EXPECT_FALSE(fs::exists(outside_ / "moved.dat"));
        EXPECT_TRUE(s.FileExists("inside.dat"));
    }

    // =====================================================================================
    // A rejected call must have no partial effect, inside or outside.
    // =====================================================================================

    TEST_F(IsolatedStorageConfinementTest, Atomicity_RejectedCallsLeaveBothTreesByteForByteIdentical)
    {
        auto s = store();
        s.CreateDirectory("existing/nested");
        { auto st = s.CreateFile("existing/nested/kept.dat"); st.Close(); }
        writeFile(outside_ / "kept_outside.dat", "keep me");

        const auto insideBefore = snapshot(root_);
        const auto outsideBefore = snapshot(outside_);

        expectRejected([&] { s.CreateDirectory("../outside/new_dir"); }, "relativePath");
        expectRejected([&] { auto st = s.CreateFile("../outside/new.dat"); st.Close(); },
                       "relativePath");
        expectRejected([&] { s.DeleteFile("../outside/kept_outside.dat"); }, "relativePath");
        expectRejected([&] { s.CopyFile("existing/nested/kept.dat", "../outside/copy.dat"); },
                       "destinationFileName");
        expectRejected([&] { s.MoveFile("existing/nested/kept.dat", "../outside/move.dat"); },
                       "destinationFileName");
        expectRejected([&] { s.DeleteDirectory(""); }, "relativePath");

        EXPECT_EQ(snapshot(root_), insideBefore) << "a rejected call changed the store";
        EXPECT_EQ(snapshot(outside_), outsideBefore) << "a rejected call changed the outside tree";
    }

    TEST_F(IsolatedStorageConfinementTest, Atomicity_RejectedOpenCreatesNoParentDirectories)
    {
        // IsolatedStorageFileStream creates missing parents; validation must run first, or a
        // rejected open would still have left a directory behind.
        auto s = store();
        const auto before = snapshot(root_);
        expectRejected(
            [&] { auto st = s.OpenFile("../outside/deep/nested/file.dat", FileMode::Create); st.Close(); },
            "relativePath");
        EXPECT_FALSE(fs::exists(outside_ / "deep"));
        EXPECT_EQ(snapshot(root_), before);
    }

    // =====================================================================================
    // Validation order.
    // =====================================================================================

    TEST_F(IsolatedStorageConfinementTest, Order_DisposedStoreIsReportedBeforeTheContainmentVerdict)
    {
        // A closed store never looked at the argument, so it must not report an argument
        // problem it did not diagnose.
        auto s = store();
        s.Close();
        EXPECT_THROW((void)s.FileExists("../outside/x"), System::ObjectDisposedException);
        EXPECT_THROW(s.CreateDirectory("../outside/x"), System::ObjectDisposedException);
        EXPECT_THROW(s.DeleteFile(""), System::ObjectDisposedException);
    }

    TEST_F(IsolatedStorageConfinementTest, Order_TheFirstOffendingParameterIsTheOneNamed)
    {
        auto s = store();
        // Both arguments escape; the source is validated first, so the source is named.
        expectRejected([&] { s.CopyFile("../outside/a.dat", "../outside/b.dat"); },
                       "sourceFileName");
        expectRejected([&] { s.MoveDirectory("../outside/a", "../outside/b"); },
                       "sourceDirectoryName");
    }

    TEST_F(IsolatedStorageConfinementTest, Order_EachRejectionReasonHasItsOwnStableMessage)
    {
        // The checks are layered on purpose, so removing any one of them usually leaves a
        // lower layer rejecting the same input -- which is good engineering and bad for
        // mutation coverage.  Pinning the message is what makes each layer separately
        // observable: an empty path is diagnosed as empty, not merely as uncontained.
        auto s = store();
        const auto messageOf = [&](const std::string& path) {
            try { s.CreateDirectory(path); return std::string("<no exception>"); }
            catch (const System::ArgumentException& e) { return e.getMessageProperty(); }
        };

        EXPECT_EQ(messageOf(""),
                  "Path must not be empty. (Parameter 'relativePath')");
        EXPECT_EQ(messageOf("/"),
                  "Path must not be empty. (Parameter 'relativePath')");
        EXPECT_EQ(messageOf("../escape"),
                  "Path must be relative to the isolated storage root. (Parameter 'relativePath')");
        EXPECT_EQ(messageOf("."),
                  "Path must be relative to the isolated storage root. (Parameter 'relativePath')");

        std::string withNul("nul");
        withNul.push_back('\0');
        withNul += "tail";
        EXPECT_EQ(messageOf(withNul),
                  "Path must not contain an embedded NUL character. (Parameter 'relativePath')");
    }

    // =====================================================================================
    // Disposed-state coverage (#2205).
    //
    // Ten members already threw ObjectDisposedException on a closed store; Remove and the
    // three space properties silently kept working, and Remove still deleted the tree.
    // =====================================================================================

    TEST_F(IsolatedStorageConfinementTest, Disposed_SpacePropertiesThrowObjectDisposedException)
    {
        auto s = store();
        s.Close();
        EXPECT_THROW((void)s.getAvailableFreeSpaceProperty(), System::ObjectDisposedException);
        EXPECT_THROW((void)s.getUsedSizeProperty(), System::ObjectDisposedException);
        EXPECT_THROW((void)s.getQuotaProperty(), System::ObjectDisposedException);
    }

    TEST_F(IsolatedStorageConfinementTest, Disposed_RemoveThrowsAndDoesNotDeleteTheTreeAgain)
    {
        auto s = store();
        { auto st = s.CreateFile("payload.dat"); st.Close(); }
        s.Close();

        EXPECT_THROW(s.Remove(), System::ObjectDisposedException);
        EXPECT_TRUE(fs::exists(root_ / "payload.dat"))
            << "Remove ran on a closed store and deleted its contents";
    }

    TEST_F(IsolatedStorageConfinementTest, Disposed_RemoveIsNotIdempotent)
    {
        auto s = store();
        s.Remove();
        EXPECT_FALSE(fs::exists(root_));
        // Remove() closes the store, so a second call is a use-after-close like any other.
        EXPECT_THROW(s.Remove(), System::ObjectDisposedException);
    }

    TEST_F(IsolatedStorageConfinementTest, Disposed_TheAlreadyGuardedMembersAreUnchanged)
    {
        auto s = store();
        s.Dispose();
        EXPECT_THROW((void)s.FileExists("x"), System::ObjectDisposedException);
        EXPECT_THROW((void)s.DirectoryExists("x"), System::ObjectDisposedException);
        EXPECT_THROW(s.CreateDirectory("x"), System::ObjectDisposedException);
        EXPECT_THROW(s.DeleteDirectory("x"), System::ObjectDisposedException);
        EXPECT_THROW(s.DeleteFile("x"), System::ObjectDisposedException);
        EXPECT_THROW((void)s.GetFileNames(), System::ObjectDisposedException);
        EXPECT_THROW((void)s.GetDirectoryNames(), System::ObjectDisposedException);
        EXPECT_THROW({ auto st = s.OpenFile("x", FileMode::Create); st.Close(); },
                     System::ObjectDisposedException);
        EXPECT_THROW({ auto st = s.CreateFile("x"); st.Close(); },
                     System::ObjectDisposedException);
        EXPECT_THROW(s.CopyFile("a", "b"), System::ObjectDisposedException);
        EXPECT_THROW(s.MoveFile("a", "b"), System::ObjectDisposedException);
        EXPECT_THROW(s.MoveDirectory("a", "b"), System::ObjectDisposedException);
    }

    TEST_F(IsolatedStorageConfinementTest, Disposed_ALiveStoreStillAnswersAllFour)
    {
        // The guard must not fire on an open store: an over-eager version of #2205 would
        // break every legitimate caller instead of only the closed ones.
        auto s = store();
        { auto st = s.CreateFile("sized.dat"); st.Close(); }
        EXPECT_GT(s.getAvailableFreeSpaceProperty(), 0);
        EXPECT_GE(s.getUsedSizeProperty(), 0);
        EXPECT_EQ(s.getQuotaProperty(), std::numeric_limits<SharpRuntime::longcs>::max());
        EXPECT_NO_THROW(s.Remove());
    }

    // =====================================================================================
    // No native std:: exception may cross a public door (#2206).
    //
    // The constructor and the three iterator-backed members used throwing std::filesystem
    // entry points, so an unusable root surfaced a std::filesystem_error rather than the
    // module's own IsolatedStorageException.  A regular file standing where the root should
    // be is the reproduction: every one of the four fails, and each must fail in-contract.
    // =====================================================================================

    TEST_F(IsolatedStorageConfinementTest, NativeErrors_ConstructorTranslatesAnUnusableRoot)
    {
        const fs::path rootIsAFile = sandbox_ / "root_is_a_file";
        writeFile(rootIsAFile, "not a directory");

        try {
            IsolatedStorageFile s(rootIsAFile, IsolatedStorageScope::None);
            ADD_FAILURE() << "expected IsolatedStorageException";
        } catch (const IsolatedStorageException&) {
            SUCCEED();
        } catch (const fs::filesystem_error& e) {
            ADD_FAILURE() << "a native std::filesystem_error crossed the public API: " << e.what();
        }
    }

    TEST_F(IsolatedStorageConfinementTest, NativeErrors_EnumerationAndAccountingTranslate)
    {
        auto s = store();
        // Replace the root with a regular file behind the store's back.
        fs::remove_all(root_);
        writeFile(root_, "not a directory");

        const auto expectInContract = [](auto&& op, const char* what) {
            try {
                op();
                ADD_FAILURE() << what << ": expected IsolatedStorageException";
            } catch (const IsolatedStorageException&) {
                SUCCEED();
            } catch (const fs::filesystem_error& e) {
                ADD_FAILURE() << what << ": native std::filesystem_error escaped: " << e.what();
            }
        };

        expectInContract([&] { (void)s.GetFileNames("*"); }, "GetFileNames");
        expectInContract([&] { (void)s.GetDirectoryNames("*"); }, "GetDirectoryNames");
        expectInContract([&] { (void)s.getUsedSizeProperty(); }, "getUsedSizeProperty");
    }

    TEST_F(IsolatedStorageConfinementTest, NativeErrors_TheHealthyPathsAreUnaffected)
    {
        // The translation must not turn a working enumeration into a failure.
        auto s = store();
        { auto st = s.CreateFile("one.dat"); st.Close(); }
        { auto st = s.CreateFile("two.dat"); st.Close(); }
        s.CreateDirectory("a_dir");
        s.CreateDirectory("b_dir");
        { auto st = s.CreateFile("a_dir/nested.dat"); st.Close(); }

        EXPECT_EQ(s.GetFileNames("*").size(), 2u);
        EXPECT_EQ(s.GetDirectoryNames("*").size(), 2u);
        EXPECT_EQ(s.GetFileNames("one*"), std::vector<std::string>{"one.dat"});
        EXPECT_GE(s.getUsedSizeProperty(), 0);   // recurses into a_dir without throwing
    }

    // =====================================================================================
    // The residual this repair deliberately does not close (#2208).
    // =====================================================================================

    TEST_F(IsolatedStorageConfinementTest, Residual_FileStreamConstructorIsNotAConfinementBoundary)
    {
        // Pinned, not celebrated: IsolatedStorageFileStream's public constructor takes a full
        // path and opens it anywhere.  Confining it is a public signature change (#2208).  If
        // this test ever starts failing, #2208 shipped and this pin must be inverted.
        const fs::path target = outside_ / "direct_stream.dat";
        {
            System::IO::IsolatedStorage::IsolatedStorageFileStream stream(target, FileMode::Create);
            stream.Close();
        }
        EXPECT_TRUE(fs::exists(target))
            << "IsolatedStorageFileStream became confined; invert this pin and close #2208";
    }

} // namespace
