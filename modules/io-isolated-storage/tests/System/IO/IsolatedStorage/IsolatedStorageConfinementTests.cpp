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
// Every fixture atomically creates a process/run-unique temporary root and owns it with RAII.
// Parallel or interrupted runs therefore cannot pre-delete or reuse one another's state.  The
// "outside the store" target is always a sibling directory inside that same sandbox.

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "System/ArgumentException.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileNotFoundException.hpp"
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

    class ScopedTemporaryDirectory final
    {
    public:
        ScopedTemporaryDirectory()
        {
            const auto parent = fs::temp_directory_path();
            static std::atomic<std::uint64_t> sequence{0};
            std::random_device entropy;
            for (int attempt = 0; attempt < 128; ++attempt) {
                const auto random =
                    (static_cast<std::uint64_t>(entropy()) << 32U) ^ entropy();
                const auto suffix =
                    random ^ sequence.fetch_add(1, std::memory_order_relaxed);
                auto candidate = parent /
                    ("sharp-runtime-isolated-storage-" + std::to_string(suffix));
                std::error_code error;
                if (fs::create_directory(candidate, error)) {
                    path_ = std::move(candidate);
                    return;
                }
            }
            throw std::runtime_error(
                "could not create a unique IsolatedStorage-test directory");
        }

        ~ScopedTemporaryDirectory()
        {
            // remove_all removes a symbolic link as a link; it does not descend through one,
            // so a link planted by a symlink test can never delete outside this unique root.
            std::error_code error;
            fs::remove_all(path_, error);
        }

        ScopedTemporaryDirectory(const ScopedTemporaryDirectory&) = delete;
        ScopedTemporaryDirectory& operator=(const ScopedTemporaryDirectory&) = delete;

        [[nodiscard]] fs::path path(const fs::path& relative) const
        {
            return path_ / relative;
        }

    private:
        fs::path path_;
    };

    class IsolatedStorageConfinementTest : public ::testing::Test
    {
    protected:
        ScopedTemporaryDirectory temporary_;
        fs::path sandbox_;
        fs::path root_;
        fs::path outside_;

        void SetUp() override
        {
            sandbox_ = temporary_.path("sandbox");
            root_ = sandbox_ / "store";
            outside_ = sandbox_ / "outside";
            fs::create_directories(root_);
            fs::create_directories(outside_);
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
    // #2207 -- the residual TOCTOU, DECLARED AND ACCEPTED on 2026-08-19.
    // =====================================================================================

    TEST_F(IsolatedStorageConfinementTest, Decl2207_ConfinementDefeatsAccidentalEscapeNotARacingWriter)
    {
        // DECLARED LIMITATION. The resolver verifies containment with weakly_canonical and the
        // operation then runs on the path NAME, so a process able to write inside the store root
        // can swap a component for a symlink between the two. Closing it needs per-component
        // openat(O_NOFOLLOW) held open for the operation's duration, fd-relative *at operations,
        // an fd-accepting FileStream primitive this port does not have, an NtCreateFile equivalent
        // for Windows, and an Emscripten story -- a platform-policy change for the whole module.
        // That was offered and declined.
        //
        // THE THREAT BOUNDARY IS WHAT THIS CASE PINS, because it is what makes the limitation
        // acceptable rather than merely tolerated. Everything the confinement is FOR still holds.

        // 1. An absolute path still cannot escape.
        writeFile(outside_ / "secret.txt", "secret");
        EXPECT_FALSE(store().FileExists((outside_ / "secret.txt").string()));

        // 2. `..` still cannot climb out.
        EXPECT_THROW((void)store().FileExists("../outside/secret.txt"),
                     System::ArgumentException);

        // 3. A symlink PLANTED IN ADVANCE and pointing outside is still refused -- this is the
        //    case people assume the TOCTOU defeats, and it does not: the check sees the link
        //    because it is already there when the check runs.
        std::error_code ec;
        fs::create_directory_symlink(outside_, root_ / "escape", ec);
        if (!ec) {
            writeFile(outside_ / "via_link.txt", "secret");
            EXPECT_THROW((void)store().FileExists("escape/via_link.txt"),
                         System::ArgumentException)
                << "#2207: a pre-existing symlink out of the store is REFUSED; only a link "
                   "swapped in DURING the operation wins the race";
        } else {
            GTEST_SKIP() << "symlink creation unavailable here: " << ec.message();
        }
    }

    TEST_F(IsolatedStorageConfinementTest, Decl2207_TheRacingWriterAlreadyHasWhatTheRaceWouldWin)
    {
        // The second half of the boundary, and the reason the residual is narrow: the attacker
        // this does not stop must ALREADY be able to write inside the store root. Such an attacker
        // can already read and write every file in the store DIRECTLY -- so winning the race
        // widens their reach BEYOND the store rather than granting them access TO it.
        //
        // Asserted rather than argued: a file created directly under the root by anyone with write
        // access there is visible to the store with no race at all.
        writeFile(root_ / "planted.txt", "planted");
        EXPECT_TRUE(store().FileExists("planted.txt"))
            << "#2207: anyone who can write inside the root already reaches the store's contents "
               "without needing the race";
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
    // Ticket #2209 -- a directory-qualified search pattern.
    // =====================================================================================

    TEST_F(IsolatedStorageConfinementTest, Fix2209_ADirectoryQualifiedPatternEnumeratesThatDirectory)
    {
        // Before #2209 both doors iterated the root and globbed the WHOLE pattern against a bare
        // filename, so this returned nothing although the file exists. .NET's own source states
        // the contract in a comment above each method -- "foo\\abc*.txt will give all abc*.txt
        // files in foo directory" -- and implements it through
        // FileSystemEnumerableFactory.NormalizeInputs (FileSystemEnumerableFactory.cs:45-56).
        auto s = store();
        s.CreateDirectory("sub");
        s.CreateDirectory("sub/deeper");
        { auto st = s.CreateFile("sub/nested.dat"); st.Close(); }
        { auto st = s.CreateFile("sub/other.txt"); st.Close(); }
        { auto st = s.CreateFile("root.dat"); st.Close(); }

        EXPECT_EQ(s.GetFileNames("sub/*").size(), 2u);
        EXPECT_EQ(s.GetFileNames("sub/*.dat"), std::vector<std::string>{"nested.dat"});
        EXPECT_EQ(s.GetDirectoryNames("sub/*"), std::vector<std::string>{"deeper"});

        // THE RESULT IS A BARE NAME, not a sub-path. .NET maps each hit through
        // Path.GetFileName (IsolatedStorageFile.cs:177) precisely because the store hides its
        // own root, so returning "sub/nested.dat" would leak a layout the type exists to hide.
        const auto names = s.GetFileNames("sub/*.dat");
        ASSERT_EQ(names.size(), 1u);
        EXPECT_EQ(names[0].find('/'), std::string::npos);

        // A trailing separator means "everything in that directory" --
        // NormalizeInputs' own "we also allowed for expression to be \"foo\\\"".
        EXPECT_EQ(s.GetFileNames("sub/").size(), 2u);

        // An unqualified pattern is unchanged, and still does not descend.
        EXPECT_EQ(s.GetFileNames("*"), std::vector<std::string>{"root.dat"});
        EXPECT_EQ(s.GetDirectoryNames("*"), std::vector<std::string>{"sub"});

        // A directory that does not exist is empty, not an error -- the same answer the root
        // gives when the store has just been created.
        EXPECT_TRUE(s.GetFileNames("nosuchdir/*").empty());
    }

    TEST_F(IsolatedStorageConfinementTest, Fix2209_TheDirectoryHalfIsConfinedAndDotNetsIsNot)
    {
        // A DELIBERATE NARROWING, pinned so it is a decision rather than an accident. .NET does
        // not run the search pattern through its containment helper: GetFileNames and
        // GetDirectoryNames are the only two doors on IsolatedStorageFile that bypass
        // GetFullPath, so in .NET `GetFileNames("../*")` escapes the store and lists its parent.
        // This port resolves the directory half through the same fullPath() every other door
        // uses. Reproducing .NET here would mean opening a confinement hole to match a reference
        // that has one.
        auto s = store();
        { auto st = s.CreateFile("inside.dat"); st.Close(); }

        for (const char* escaping : {"../*", "sub/../../*", "../"}) {
            EXPECT_THROW((void)s.GetFileNames(escaping), System::ArgumentException) << escaping;
            EXPECT_THROW((void)s.GetDirectoryNames(escaping), System::ArgumentException) << escaping;
        }

        // A LEADING SEPARATOR IS NOT AN ESCAPE HERE, and that is a second, older divergence this
        // test records rather than introduces. .NET rejects a rooted search pattern outright --
        // NormalizeInputs opens with `if (Path.IsPathRooted(expression)) throw new
        // ArgumentException(SR.Arg_Path2IsRooted, ...)` (FileSystemEnumerableFactory.cs:29-30).
        // This port's fullPath() strips leading separators at EVERY door, so "/x" has always
        // meant "x relative to the store". Rejecting it only in the pattern would make the type
        // inconsistent with itself, which is worse than a divergence that is uniform.
        EXPECT_EQ(s.GetFileNames("/*"), std::vector<std::string>{"inside.dat"});
        EXPECT_TRUE(s.GetFileNames("/etc/*").empty());   // root/etc, which does not exist

        // ...and a harmless ".." that stays inside is fine, because containment is about where
        // the path LANDS, not about which characters it contains.
        s.CreateDirectory("a");
        { auto st = s.CreateFile("a/deep.dat"); st.Close(); }
        EXPECT_EQ(s.GetFileNames("a/../a/*"), std::vector<std::string>{"deep.dat"});
    }

    // =====================================================================================
    // #2208 -- the residual is closed. INVERTED from the pin that recorded it.
    // =====================================================================================

    TEST_F(IsolatedStorageConfinementTest, Fix2208_TheStreamConstructorIsNowAConfinementBoundary)
    {
        // INVERTED BY #2208 (2026-08-19). Its predecessor was called
        // Residual_FileStreamConstructorIsNotAConfinementBoundary, took an absolute path, and said
        // "if this test ever starts failing, #2208 shipped and this pin must be inverted." It did.
        //
        // The constructor used to open whatever path it was handed, ANYWHERE on the filesystem,
        // and create that path's missing parents on the way. That was a WIDER hole than #2207's
        // declared TOCTOU: no race and no privilege were needed, only the call.
        //
        // What confinement MEANS here is containment, not rejection: fullPath() strips leading
        // separators at every door, so an absolute path is REINTERPRETED as relative to the store
        // rather than refused. That rule is older than this ticket (#2209 recorded it) and applies
        // to OpenFile, CreateFile, DeleteFile and MoveFile alike; refusing it only at this one
        // door would make the type inconsistent with itself.
        const fs::path target = outside_ / "direct_stream.dat";

        {
            System::IO::IsolatedStorage::IsolatedStorageFileStream stream(
                target.string(), FileMode::Create, store());
            stream.Close();
        }

        EXPECT_FALSE(fs::exists(target))
            << "#2208: the outside path must not be created -- this is the whole repair";

        // ...and the write landed INSIDE the store, at the absolute path reinterpreted relative
        // to the root. Asserting where it went, not merely where it did not go, is what makes
        // this a containment test rather than a test that the call failed for some other reason.
        const fs::path contained = root_ / target.relative_path();
        EXPECT_TRUE(fs::exists(contained))
            << "#2208: the absolute path must be reinterpreted relative to the store root";
    }

    TEST_F(IsolatedStorageConfinementTest, Fix2208_TheStreamConstructorRefusesEveryEscapeItsSiblingsDo)
    {
        // The confinement now has ONE implementation: the constructor resolves through the store's
        // own fullPath(), the same resolver OpenFile/CreateFile use. So it must refuse exactly what
        // they refuse -- asserted rather than assumed, because two resolvers that drift apart is
        // the failure mode a shared one exists to prevent.
        writeFile(outside_ / "secret.txt", "secret");

        EXPECT_THROW((void)System::IO::IsolatedStorage::IsolatedStorageFileStream(
                         "../outside/secret.txt", FileMode::Open, store()),
                     System::ArgumentException) << "a `..` climb";

        // The refusal must come BEFORE anything is created. The constructor's one job on the way
        // in is to make the file's missing parents, so a check that ran after it would leave a
        // directory tree outside the store behind every refused call.
        EXPECT_THROW((void)System::IO::IsolatedStorage::IsolatedStorageFileStream(
                         "../outside/made2208/x.dat", FileMode::Create, store()),
                     System::ArgumentException);
        EXPECT_FALSE(fs::exists(outside_ / "made2208"))
            << "#2208: a refused path must not have had its parents created first";

        std::error_code ec;
        fs::create_directory_symlink(outside_, root_ / "escape2208", ec);
        if (!ec) {
            EXPECT_THROW((void)System::IO::IsolatedStorage::IsolatedStorageFileStream(
                             "escape2208/secret.txt", FileMode::Open, store()),
                         System::ArgumentException) << "a pre-existing symlink out of the store";
        }

        // An absolute path is contained rather than refused (see the sibling case), so what must
        // hold for it is that it cannot READ the outside file it names.
        EXPECT_THROW((void)System::IO::IsolatedStorage::IsolatedStorageFileStream(
                         (outside_ / "secret.txt").string(), FileMode::Open, store()),
                     System::IO::FileNotFoundException)
            << "an absolute path must resolve inside the store, where no such file exists";

        // The STORELESS constructor is .NET's `(path, mode)` and defaults to the domain store.
        // It must be confined too -- that it defaults its store rather than skipping one is the
        // whole reason .NET can publish it, and the reason #2208 did not have to remove it.
        // Only a refusal is asserted here: a success would write into the real user store root
        // rather than this fixture's temporary one.
        EXPECT_THROW((void)System::IO::IsolatedStorage::IsolatedStorageFileStream(
                         "../../../etc/passwd2208", FileMode::Create),
                     System::ArgumentException)
            << "#2208: the storeless constructor resolves against a default store, not the "
               "filesystem";

        // .NET validates the mode before touching the path, with this exact text
        // (SR.IsolatedStorage_FileOpenMode). Every named FileMode is legal, so only a value cast
        // in from outside the enumeration can reach it.
        try {
            (void)System::IO::IsolatedStorage::IsolatedStorageFileStream(
                "modecheck2208.dat", static_cast<FileMode>(99), store());
            ADD_FAILURE() << "#2208: an undefined FileMode must be rejected";
        } catch (const System::ArgumentException& ex) {
            EXPECT_STREQ(ex.what(), "Invalid mode, see System.IO.FileMode.");
        }
        EXPECT_FALSE(fs::exists(root_ / "modecheck2208.dat"))
            << "#2208: the mode is checked before the file is created, as .NET checks it first";

        // ...and a legitimate relative path still works, so the refusals above are not the
        // constructor simply refusing everything.
        {
            System::IO::IsolatedStorage::IsolatedStorageFileStream ok(
                "inside2208.dat", FileMode::Create, store());
            ok.Close();
        }
        EXPECT_TRUE(fs::exists(root_ / "inside2208.dat"));
    }

} // namespace
