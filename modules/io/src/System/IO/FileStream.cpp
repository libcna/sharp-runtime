// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/FileStream.hpp"
#include "Utf8Path.hpp"

#include <optional>
#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <utility>
#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/NotSupportedException.hpp"
#include "System/ObjectDisposedException.hpp"
#include "System/UnauthorizedAccessException.hpp"
#include "System/IO/DirectoryNotFoundException.hpp"
#include "System/IO/FileNotFoundException.hpp"
#include "System/IO/IOException.hpp"
#include "System/IO/PathTooLongException.hpp"

#include <filesystem>

#if defined(_WIN32)
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace System::IO
{
    namespace {
        bool HasFlag(FileAccess access, FileAccess flag) {
            return (access & flag) == flag;
        }

        // Verified against Interop.IOErrors.cs's GetExceptionForIoErrno: real .NET throws
        // DirectoryNotFoundException (not FileNotFoundException, and not a generic IOException)
        // whenever the *parent* directory of the target path doesn't exist -- "For Windows
        // compatibility, throw DirectoryNotFoundException instead of FileNotFoundException when
        // the parent folder does not exist."
        bool ParentDirectoryExists(const std::string& path) {
            const std::optional<std::filesystem::path> native = Detail::TryNativePath(path);
            if (!native) return false;
            std::filesystem::path parent = native->parent_path();
            if (parent.empty()) return true; // relative path with no directory component
            std::error_code ec;
            bool isDir = std::filesystem::is_directory(parent, ec);
            return !ec && isDir;
        }

        // .NET: FileStream(path, mode) defaults access to Write for Append, ReadWrite otherwise.
        FileAccess DefaultAccessFor(FileMode mode) {
            return mode == FileMode::Append ? FileAccess::Write : FileAccess::ReadWrite;
        }

        void ValidateModeAndAccess(FileMode mode, FileAccess access, FileShare share) {
            if (mode < FileMode::CreateNew || mode > FileMode::Append)
                throw System::ArgumentOutOfRangeException("mode", "Invalid file mode.");
            if (access < FileAccess::Read || access > FileAccess::ReadWrite)
                throw System::ArgumentOutOfRangeException("access", "Invalid file access.");
            // Strategies/FileStreamHelpers.cs:63-113 validates all enum domains before
            // checking mode/access combinations; Inheritable is outside the sharing mask.
            constexpr int validShare = static_cast<int>(FileShare::ReadWrite) | static_cast<int>(FileShare::Delete) |
                                       static_cast<int>(FileShare::Inheritable);
            if ((static_cast<int>(share) & ~validShare) != 0)
                throw System::ArgumentOutOfRangeException("share", "Invalid file sharing flags.");
            if (HasFlag(access, FileAccess::Read) && mode == FileMode::Append) {
                throw System::ArgumentException("FileMode.Append is invalid with FileAccess.Read.", "access");
            }
            if (!HasFlag(access, FileAccess::Write)) {
                if (mode == FileMode::Truncate || mode == FileMode::CreateNew ||
                    mode == FileMode::Create   || mode == FileMode::Append) {
                    throw System::ArgumentException(
                        "Combining FileMode and FileAccess values is invalid: this FileMode requires FileAccess.Write.",
                        "access");
                }
            }
        }

        [[noreturn]] void ThrowIo(const char* operation, int error) {
            throw IOException(std::string(operation) + ": " + std::generic_category().message(error));
        }

        [[noreturn]] void ThrowOpenError(const std::string& path, int error) {
            if (error == ENOENT || error == ENOTDIR) {
                if (!ParentDirectoryExists(path))
                    throw DirectoryNotFoundException("Could not find a part of the path '" + path + "'.");
                throw FileNotFoundException("Unable to find the specified file.", path);
            }
            if (error == EACCES || error == EPERM || error == EISDIR)
                throw System::UnauthorizedAccessException("Access to the path '" + path + "' is denied.");
            if (error == ENAMETOOLONG)
                throw PathTooLongException("The file name or path is too long.");
            ThrowIo("Failed to open file", error);
        }

        struct Descriptor {
            int value;
            ~Descriptor() {
                if (value < 0) return;
#if defined(_WIN32)
                _close(value);
#else
                close(value);
#endif
            }
        };

        int SeekFile(std::FILE* file, std::int64_t position, int origin) {
#if defined(_WIN32)
            return _fseeki64(file, position, origin);
#else
            return fseeko(file, static_cast<off_t>(position), origin);
#endif
        }

        std::int64_t TellFile(std::FILE* file) {
#if defined(_WIN32)
            const auto position = _ftelli64(file);
#else
            const auto position = ftello(file);
#endif
            if (position < 0) ThrowIo("Unable to query file position", errno);
            return position;
        }

        intcs StreamInteger(std::int64_t value) {
            // This port's Stream still exposes intcs, not .NET's Int64. Never wrap a native
            // length/position into a negative result; widening Stream is a separate API change.
            if (value > (std::numeric_limits<intcs>::max)())
                throw IOException("File length or position exceeds this Stream's integer range.");
            return static_cast<intcs>(value);
        }
    }

    struct FileStream::NativeFile {
        std::FILE* stream = nullptr;
        enum class Operation { None, Read, Write };
        Operation last = Operation::None;
        std::int64_t appendStart = -1;

        ~NativeFile() { if (stream) std::fclose(stream); }

        void FlushWrites() {
            if (last == Operation::Write) {
                if (std::fflush(stream) != 0) ThrowIo("Unable to flush file", errno);
                last = Operation::None;
            }
        }
        void PrepareRead() {
            FlushWrites();
            last = Operation::Read;
        }
        void PrepareWrite() {
            // C update streams require positioning between input and output. This also
            // discards buffered read-ahead without losing the logical position at EOF.
            if (last == Operation::Read && SeekFile(stream, 0, SEEK_CUR) != 0)
                ThrowIo("Unable to switch file to writing", errno);
            last = Operation::Write;
        }
    };

    FileStream::FileStream(const std::string& path)
        : FileStream(path, FileMode::Open) {}

    FileStream::FileStream(const std::string& path, FileMode mode)
        : FileStream(path, mode, DefaultAccessFor(mode)) {}

    FileStream::FileStream(const std::string& path, FileMode mode, FileAccess access)
        : FileStream(path, mode, access, FileShare::Read) {}

    FileStream::FileStream(const std::string& path, FileMode mode, FileAccess access, FileShare share)
        : canRead_(false), canWrite_(false)
    {
        // FileStreamHelpers.cs:64 rejects an empty name before mode/access validation.
        if (path.empty())
            throw System::ArgumentException("The path is empty.", "path");
        ValidateModeAndAccess(mode, access, share);
        // Path.Unix.cs:28-33 rejects embedded NUL before any native open. Otherwise
        // a C-string syscall silently names only the prefix, even for Create/Truncate.
        if (path.find('\0') != std::string::npos)
            throw System::ArgumentException("The path contains a null character.", "path");

        std::error_code ec;
        // The path is UTF-8 (Utf8Path.hpp). Everything below opens and stats THIS value, never
        // the narrow string: the narrow overloads convert through the ANSI code page on Windows.
        const std::filesystem::path nativePath = Detail::NativePath(path);
        const auto status = std::filesystem::status(nativePath, ec);
        // Interop.IOErrors.cs:114-152 distinguishes a genuinely absent path from
        // denied access and ENAMETOOLONG. Discarding stat errors as "not a file"
        // incorrectly reports these failures as FileNotFoundException.
        if (ec && ec != std::errc::no_such_file_or_directory && ec != std::errc::not_a_directory) {
            if (ec == std::errc::filename_too_long)
                throw PathTooLongException("The file name or path is too long.");
            if (ec == std::errc::permission_denied || ec == std::errc::operation_not_permitted)
                throw System::UnauthorizedAccessException("Access to the path '" + path + "' is denied.");
            throw IOException("Failed to inspect file '" + path + "': " + ec.message());
        }
        const bool exists = std::filesystem::is_regular_file(status);
        const bool isDirectory = std::filesystem::is_directory(status);

        // Existence preconditions that std::fstream's open-mode flags can't express directly.
        if (mode == FileMode::CreateNew && (exists || isDirectory)) {
            throw IOException("Cannot create '" + path + "' because a file or directory with the same name already exists.");
        }
        // .NET SafeFileHandle.Unix.cs:380-390 refuses directory handles with EACCES
        // (write opens fail with EISDIR). Both map to UnauthorizedAccessException.
        // Mode/access validation and CreateNew's existing-path error precede this.
        // Independently verified for all 18 mode/access pairs on .NET Framework 4.
        if (isDirectory) {
            throw System::UnauthorizedAccessException("Access to the path '" + path + "' is denied.");
        }
        if ((mode == FileMode::Open || mode == FileMode::Truncate) && !exists) {
            if (!ParentDirectoryExists(path)) {
                throw DirectoryNotFoundException("Could not find a part of the path '" + path + "'.");
            }
            throw FileNotFoundException("Unable to find the specified file.", path);
        }

        bool wantRead  = HasFlag(access, FileAccess::Read);
        bool wantWrite = HasFlag(access, FileAccess::Write);

        auto owned = std::make_unique<NativeFile>();
        const bool inheritable = (share & FileShare::Inheritable) != FileShare::None;
#if defined(_WIN32)
        const DWORD desiredAccess = (wantRead ? GENERIC_READ : 0) | (wantWrite ? GENERIC_WRITE : 0);
        const DWORD creation = mode == FileMode::Append ? OPEN_ALWAYS : static_cast<DWORD>(mode);
        SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, inheritable ? TRUE : FALSE};
        HANDLE handle = CreateFileW(nativePath.c_str(), desiredAccess,
            static_cast<DWORD>(share) & (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE),
            &security, creation, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            const auto error = GetLastError();
            if (error == ERROR_FILE_NOT_FOUND) ThrowOpenError(path, ENOENT);
            if (error == ERROR_PATH_NOT_FOUND)
                throw DirectoryNotFoundException("Could not find a part of the path '" + path + "'.");
            if (error == ERROR_ACCESS_DENIED) ThrowOpenError(path, EACCES);
            if (error == ERROR_FILENAME_EXCED_RANGE) ThrowOpenError(path, ENAMETOOLONG);
            throw IOException("Failed to open file '" + path + "': " +
                              std::system_category().message(static_cast<int>(error)));
        }
        const int flags = (wantRead ? (wantWrite ? _O_RDWR : _O_RDONLY) : _O_WRONLY) |
                          _O_BINARY | (inheritable ? 0 : _O_NOINHERIT);
        Descriptor descriptor{_open_osfhandle(reinterpret_cast<std::intptr_t>(handle), flags)};
        if (descriptor.value < 0) {
            const int error = errno;
            CloseHandle(handle);
            ThrowIo("Unable to own file handle", error);
        }
        owned->stream = _fdopen(descriptor.value, wantRead ? (wantWrite ? "r+b" : "rb") : "wb");
#else
        int flags = wantRead ? (wantWrite ? O_RDWR : O_RDONLY) : O_WRONLY;
        if (!inheritable) flags |= O_CLOEXEC;
        if (mode == FileMode::CreateNew) flags |= O_CREAT | O_EXCL;
        if (mode == FileMode::Create || mode == FileMode::OpenOrCreate || mode == FileMode::Append)
            flags |= O_CREAT;
        int opened;
        do { opened = open(nativePath.c_str(), flags, 0666); } while (opened < 0 && errno == EINTR);
        Descriptor descriptor{opened};
        if (opened < 0) ThrowOpenError(path, errno);

        // SafeFileHandle.Unix.cs:413-430 (local .NET11 source), independently measured
        // on .NET8 Linux: None is exclusive, other modes shared, only contention is fatal.
        // The owner selected native platform behavior, not emulated Windows sharing.
        int locked;
        do { locked = flock(opened, (share == FileShare::None ? LOCK_EX : LOCK_SH) | LOCK_NB); }
        while (locked < 0 && errno == EINTR);
        if (locked < 0 && (errno == EWOULDBLOCK || errno == EAGAIN))
            ThrowIo("The file is being used by another stream", errno);

        // Never pass O_TRUNC to open: a denied share must leave the existing data intact.
        // SafeFileHandle.Unix.cs:481-499 applies truncation only after acquiring the lock.
        if ((mode == FileMode::Create || mode == FileMode::Truncate) && ftruncate(opened, 0) != 0)
            ThrowIo("Unable to truncate file", errno);
        owned->stream = fdopen(opened, wantRead ? (wantWrite ? "r+b" : "rb") : "wb");
#endif
        // fdopen owns the SAME descriptor and does not open/truncate the path again:
        // POSIX fdopen specification; Microsoft _fdopen descriptor-ownership contract.
        if (!owned->stream) ThrowIo("Unable to buffer file", errno);
        descriptor.value = -1;
        if (mode == FileMode::Append) {
            if (SeekFile(owned->stream, 0, SEEK_END) != 0) ThrowIo("Unable to append to file", errno);
            owned->appendStart = TellFile(owned->stream);
        }
        canRead_  = wantRead;
        canWrite_ = wantWrite;
        file_ = std::move(owned);
    }

    // Destruction releases the resource without throwing; explicit Flush/Close report errors.
    FileStream::~FileStream() = default;

    // Ticket #2099 (SR-AUD-342's surviving half, cause I-A). One funnel, routed through by
    // every member that depends on the open file -- including Read/Write/WriteByte/SetLength,
    // which were already correct -- so the six that were wrong and the four that were right
    // cannot drift apart again. The message is the one those four already raised, unchanged.
    void FileStream::EnsureNotClosed() const
    {
        if (!file_)
            throw System::ObjectDisposedException("Cannot access a closed file.");
    }

    // The canRead_/canWrite_ tests in Read(), Write() and WriteByte() are ticket #1825.
    // Verified against Strategies/OSFileStreamStrategy.cs:208-217 and 232-241, whose
    // synchronous Read and Write both check the handle's closed state FIRST and the access
    // flags SECOND, throwing NotSupportedException(SR.NotSupported_UnreadableStream /
    // _UnwritableStream). That order is preserved here and pinned by a test: a stream that is
    // both closed and unwritable must report ObjectDisposedException, not NotSupportedException.
    //
    // Without them the failure was silent, not late. An std::fstream opened without
    // std::ios::out accepts file_.write(), sets badbit and returns; nothing inspected either
    // the flag or the stream state, so the bytes were dropped and the caller was told nothing.
    // Measured in build-probe/1825_prefix_defects.log: case 1 writes "XXXX" through a
    // FileAccess::Read handle and the file still reads "seed"; case 2 is the same loss through
    // WriteByte; case 3 reads a FileMode::Append handle and gets n=0, indistinguishable from
    // end-of-file. MemoryStream and UnmanagedMemoryStream in this module already threw exactly
    // these messages for exactly this condition; FileStream was the last stream that did not.
    intcs FileStream::Read(bytecs buffer[], intcs offset, intcs count)
    {
        EnsureNotClosed();
        if (!canRead_)
            throw System::NotSupportedException("Stream does not support reading.");
        if (buffer == nullptr)
            throw System::ArgumentNullException("buffer");
        if (offset < 0)
            throw System::ArgumentOutOfRangeException("offset", "Non-negative number required.");
        if (count < 0)
            throw System::ArgumentOutOfRangeException("count", "Non-negative number required.");
        if (count == 0) return 0;
        file_->PrepareRead();
        const auto bytesRead = std::fread(buffer + offset, 1, static_cast<std::size_t>(count), file_->stream);
        if (std::ferror(file_->stream)) ThrowIo("Unable to read file", errno);
        // .NET OSFileStreamStrategy.cs:220-223 advances by the actual read count;
        // a short EOF read does not poison position, relative seeks or later writes.
        // Clear only benign EOF, after checking errors, so a later read can see appended data.
        if (std::feof(file_->stream)) std::clearerr(file_->stream);
        return static_cast<intcs>(bytesRead);
    }

    void FileStream::Write(const bytecs buffer[], intcs offset, intcs count)
    {
        EnsureNotClosed();
        if (!canWrite_)
            throw System::NotSupportedException("Stream does not support writing.");
        if (buffer == nullptr)
            throw System::ArgumentNullException("buffer");
        if (offset < 0)
            throw System::ArgumentOutOfRangeException("offset", "Non-negative number required.");
        if (count < 0)
            throw System::ArgumentOutOfRangeException("count", "Non-negative number required.");
        if (count == 0) return;
        file_->PrepareWrite();
        if (std::fwrite(buffer + offset, 1, static_cast<std::size_t>(count), file_->stream) !=
            static_cast<std::size_t>(count))
            ThrowIo("Unable to write file", errno);
    }

    // WriteByte had NO validation at all -- not even the is_open() test its Write() sibling
    // already had -- so writing a byte to a closed FileStream was accepted in silence
    // (build-probe/1825_prefix_defects.log case 4). Ticket #1825's own description said all
    // three operations "check only file_.is_open()"; for this one that was too generous, and
    // the correction is recorded rather than quietly absorbed. .NET has no such gap because
    // OSFileStreamStrategy.cs:226-227 defines WriteByte as Write(ReadOnlySpan<byte>), so it
    // inherits both checks; this now matches by performing both in the same order.
    void FileStream::WriteByte(bytecs value)
    {
        Write(&value, 0, 1);
    }

    // Flushing a closed file silently did nothing, so a caller could not tell a successful
    // flush from a lost one. .NET's Stream.Flush is documented to throw once the stream is
    // disposed, and this module's UnmanagedMemoryStream::Flush already did (#2108).
    void FileStream::Flush()
    {
        EnsureNotClosed();
        file_->FlushWrites();
    }

    void FileStream::Close()
    {
        if (!file_) return;
        std::FILE* stream = std::exchange(file_->stream, nullptr);
        file_.reset();
        if (std::fclose(stream) != 0) ThrowIo("Unable to close file", errno);
    }

    intcs FileStream::getLengthProperty() const {
        // Query the owned handle, not its original path, which might have been renamed or
        // replaced. Buffered writes must reach that handle before querying its current size.
        EnsureNotClosed();
        file_->FlushWrites();
#if defined(_WIN32)
        LARGE_INTEGER size{};
        if (!GetFileSizeEx(reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(file_->stream))), &size))
            throw IOException("Unable to query file length: " +
                std::system_category().message(static_cast<int>(GetLastError())));
        return StreamInteger(size.QuadPart);
#else
        struct stat status{};
        if (fstat(fileno(file_->stream), &status) != 0) ThrowIo("Unable to query file length", errno);
        return StreamInteger(status.st_size);
#endif
    }

    bool FileStream::IsOpen() const { return file_ != nullptr; }

    // Before #2099 this returned tellg()/tellp() on a closed fstream, which is -1: a sentinel
    // where every sibling stream raises. Worse, the sentinel did not stay local -- Stream::Seek
    // adds it to the requested offset, so Seek(0, SeekOrigin::Current) on a closed FileStream
    // reported IOException("...before the beginning of the stream."), a diagnostic about the
    // seek target rather than about the closed file.
    intcs FileStream::getPositionProperty() const
    {
        EnsureNotClosed();
        return StreamInteger(TellFile(file_->stream));
    }

    // The closed test precedes the argument test: a closed stream reports ObjectDisposedException
    // whatever the argument is, which is the order Read/Write already used and the order #2108
    // established for this module after finding UnmanagedMemoryStream::SetLength reversed.
    void FileStream::setPositionProperty(intcs value)
    {
        EnsureNotClosed();
        if (value < 0)
            throw System::ArgumentOutOfRangeException("value", "Non-negative number required.");
        // OSFileStreamStrategy.cs:166-170 rejects seeks before the original append boundary.
        if (value < file_->appendStart)
            throw IOException("Cannot seek before the start of a file opened for append.");
        if (SeekFile(file_->stream, value, SEEK_SET) != 0) ThrowIo("Unable to seek file", errno);
        file_->last = NativeFile::Operation::None;
    }

    // Check open state before canWrite_: canWrite_ reflects the access mode requested at
    // construction and is never reset by Close(), so without this check SetLength() after
    // Close() would still resize the file on disk via path_ below despite the stream
    // claiming to be closed. #2099 additionally moved it before the value test, which was
    // the one place in this file where an argument check preceded the closed check.
    void FileStream::SetLength(intcs value)
    {
        EnsureNotClosed();
        if (value < 0)
            throw System::ArgumentOutOfRangeException("value", "Non-negative number required.");
        if (!canWrite_)
            throw System::NotSupportedException("Stream does not support writing.");

        if (value < file_->appendStart)
            throw IOException("Cannot truncate before the start of a file opened for append.");
        const auto position = TellFile(file_->stream);
        // Synchronize stdio with the descriptor before changing its size. POSIX fseek may
        // optimize a seek inside its read-ahead buffer; fflush(input) explicitly discards it.
        // The Windows CRT uses fseek to discard input (fflush(input) is a no-op there).
#if !defined(_WIN32)
        if (std::fflush(file_->stream) != 0) ThrowIo("Unable to synchronize file", errno);
#endif
        // Use this handle so a renamed file cannot resize an unrelated replacement.
        if (SeekFile(file_->stream, position, SEEK_SET) != 0) ThrowIo("Unable to seek file", errno);
#if defined(_WIN32)
        const auto error = _chsize_s(_fileno(file_->stream), value);
        if (error != 0) ThrowIo("Unable to set file length", error);
#else
        if (ftruncate(fileno(file_->stream), value) != 0) ThrowIo("Unable to set file length", errno);
#endif
        if (SeekFile(file_->stream, position > value ? value : position, SEEK_SET) != 0)
            ThrowIo("Unable to restore file position", errno);
        file_->last = NativeFile::Operation::None;
    }
}
