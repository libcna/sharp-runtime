// SPDX-License-Identifier: MIT
//
// FileStream and File take their path as UTF-8. On Windows the narrow std::fstream and
// std::filesystem overloads convert through the process ANSI code page instead, so a path holding
// any character that code page cannot spell named a different file or no file at all -- while on
// POSIX the same call is a byte copy, which is why it never showed up on a Linux build.
//
// These tests pass on Linux both before and after that fix; their value is on Windows. Each one
// proves the file by reading a known payload back out of it, never by comparing path strings.
//
// Path text is spelled with hex escapes so this source file's own encoding is not under test. The
// concatenation breaks are required: a C++ hex escape is maximal-munch.

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/IO/File.hpp"
#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

namespace fs = std::filesystem;

namespace
{
    constexpr const char* kCzech = "\xc5\xbe" "lu\xc5\xa5" "ou\xc4\x8d" "k\xc3\xbd";
    constexpr const char* kJapanese = "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e";
    constexpr const char* kEmoji = "emoji-\xf0\x9f\x98\x80";

    std::string Utf8Of(const fs::path& p)
    {
        const std::u8string s = p.u8string();
        return {reinterpret_cast<const char*>(s.data()), s.size()};
    }

    fs::path FromUtf8(const std::string& s)
    {
        return fs::path(std::u8string(reinterpret_cast<const char8_t*>(s.data()), s.size()));
    }

    /// A temporary directory whose own name is non-ASCII, removed on destruction.
    class UnicodeScope
    {
    public:
        UnicodeScope()
        {
            std::error_code ec;
            root_ = fs::temp_directory_path()
                    / FromUtf8(std::string("sr-utf8-") + kCzech + "-" + std::to_string(counter_++));
            fs::remove_all(root_, ec);
            fs::create_directories(root_, ec);
        }

        ~UnicodeScope()
        {
            std::error_code ec;
            fs::remove_all(root_, ec);
        }

        UnicodeScope(const UnicodeScope&) = delete;
        UnicodeScope& operator=(const UnicodeScope&) = delete;

        /// The UTF-8 spelling of root/<name>, which is what the System::IO API takes.
        [[nodiscard]] std::string Utf8Child(const std::string& name) const
        {
            return Utf8Of(root_ / FromUtf8(name));
        }

        [[nodiscard]] const fs::path& Root() const { return root_; }

    private:
        fs::path root_;
        static inline int counter_ = 0;
    };
}

TEST(Utf8PathTest, FileWriteAllTextAndReadAllTextRoundTripThroughANonAsciiPath)
{
    const UnicodeScope scope;
    for (const char* name : {kCzech, kJapanese, kEmoji})
    {
        const std::string path = scope.Utf8Child(std::string(name) + ".txt");
        System::IO::File::WriteAllText(path, "payload-ok");
        EXPECT_TRUE(System::IO::File::Exists(path)) << name;
        EXPECT_EQ(System::IO::File::ReadAllText(path), "payload-ok") << name;

        // The file the API claims to have written is the file on disk, checked natively rather
        // than through the same API that wrote it.
        EXPECT_TRUE(fs::exists(scope.Root() / FromUtf8(std::string(name) + ".txt"))) << name;
    }
}

TEST(Utf8PathTest, FileWriteAllBytesAndReadAllBytesRoundTripThroughANonAsciiPath)
{
    const UnicodeScope scope;
    const std::string path = scope.Utf8Child(std::string(kJapanese) + ".bin");
    const std::vector<std::uint8_t> written{0x01, 0x02, 0x03, 0xff};

    System::IO::File::WriteAllBytes(path, written);
    EXPECT_EQ(System::IO::File::ReadAllBytes(path), written);
}

TEST(Utf8PathTest, FileExistsIsFalseRatherThanThrowingForTextThatIsNotUtf8)
{
    // Untrusted text reaches File::Exists; it answers rather than propagating.
    EXPECT_FALSE(System::IO::File::Exists("caf\xe9/never-created.txt"));
    EXPECT_FALSE(System::IO::File::Exists("\xff\xfe"));
}

TEST(Utf8PathTest, FileStreamCreatesAndReopensAFileUnderANonAsciiPath)
{
    const UnicodeScope scope;
    const std::string path = scope.Utf8Child(std::string(kEmoji) + ".dat");

    {
        System::IO::FileStream out(path, System::IO::FileMode::Create,
                                   System::IO::FileAccess::Write);
        SharpRuntime::bytecs bytes[] = {'s', 'r', '-', 'o', 'k'};
        out.Write(bytes, 0, 5);
        out.Close();
    }

    EXPECT_TRUE(fs::exists(scope.Root() / FromUtf8(std::string(kEmoji) + ".dat")));

    System::IO::FileStream in(path, System::IO::FileMode::Open, System::IO::FileAccess::Read);
    EXPECT_EQ(in.getLengthProperty(), 5);
    SharpRuntime::bytecs read[5] = {};
    EXPECT_EQ(in.Read(read, 0, 5), 5);
    EXPECT_EQ(std::string(read, read + 5), "sr-ok");
}

TEST(Utf8PathTest, FileStreamReportsLengthForAFileUnderANonAsciiPath)
{
    // FileStream queries file_size independently of the stream; that call took the narrow path
    // too, so the length came back 0 on Windows even when the open had somehow succeeded.
    const UnicodeScope scope;
    const std::string path = scope.Utf8Child(std::string(kCzech) + "-length.bin");
    System::IO::File::WriteAllText(path, "0123456789");

    System::IO::FileStream stream(path, System::IO::FileMode::Open, System::IO::FileAccess::Read);
    EXPECT_EQ(stream.getLengthProperty(), 10);
}

TEST(Utf8PathTest, OpeningAMissingFileUnderANonAsciiDirectoryStillRefusesTheSameWay)
{
    // The refusal must stay a FileNotFoundException, not become a filesystem_error escaping from
    // the conversion.
    const UnicodeScope scope;
    const std::string path = scope.Utf8Child(std::string(kJapanese) + "/nothing.bin");
    EXPECT_THROW(
        {
            System::IO::FileStream stream(path, System::IO::FileMode::Open,
                                          System::IO::FileAccess::Read);
        },
        std::exception);
}
