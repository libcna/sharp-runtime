// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/IsolatedStorage/IsolatedStorageFile.hpp"

#include <algorithm>
#include <filesystem>
#include <numeric>

#include <limits>

#include "SharpRuntime/Storage/StoragePaths.hpp"
#include "System/ArgumentException.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/IsolatedStorage/IsolatedStorageException.hpp"
#include "System/IO/IsolatedStorage/IsolatedStorageFileStream.hpp"
#include "System/ObjectDisposedException.hpp"

namespace System::IO::IsolatedStorage
{
    // Simple glob: only '*' wildcard supported (matches any sequence of chars).
    static bool globMatch(const std::string& pattern, const std::string& name)
    {
        if (pattern == "*") return true;
        size_t p = 0, n = 0;
        size_t starP = std::string::npos, starN = 0;
        while (n < name.size()) {
            if (p < pattern.size() && (pattern[p] == name[n] || pattern[p] == '?')) {
                ++p; ++n;
            } else if (p < pattern.size() && pattern[p] == '*') {
                starP = p++; starN = n;
            } else if (starP != std::string::npos) {
                p = starP + 1; n = ++starN;
            } else {
                return false;
            }
        }
        while (p < pattern.size() && pattern[p] == '*') ++p;
        return p == pattern.size();
    }

    IsolatedStorageFile::IsolatedStorageFile(const std::filesystem::path& rootDirectory, IsolatedStorageScope scope)
        : rootDirectory_(rootDirectory)
    {
        scope_ = scope;
        std::filesystem::create_directories(rootDirectory_);
    }

    namespace {
        constexpr const char* ContainmentMessage =
            "Path must be relative to the isolated storage root.";

        bool isDirectorySeparator(char c)
        {
#ifdef _WIN32
            return c == '/' || c == '\\';
#else
            // On POSIX a backslash is an ordinary name character, so treating it as a
            // separator here would corrupt legitimate file names.
            return c == '/';
#endif
        }

        // True when `candidate` is a proper descendant of `root`.  A string-prefix test would
        // be wrong in both directions: it accepts a sibling sharing the root's spelling
        // ("<root>xyz/evil") and rejects contained paths whenever the root lacks a trailing
        // separator.  lexically_relative answers the question the invariant actually asks.
        bool isWithin(const std::filesystem::path& root, const std::filesystem::path& candidate)
        {
            const std::filesystem::path relative = candidate.lexically_relative(root);
            if (relative.empty()) return false;         // no relation at all
            const std::filesystem::path first = *relative.begin();
            if (first == "..") return false;            // climbs out of the root
            if (first == ".") return false;             // the root itself, not a descendant
            return true;
        }
    }

    // Verified against IsolatedStorageFile.cs's GetFullPath(): real .NET removes leading
    // directory separators before Path.Combine, so a rooted caller path is reinterpreted as
    // store-relative.  This port previously returned `rootDirectory_ / relativePath` with no
    // handling at all, and std::filesystem::path::operator/ DISCARDS the left operand when the
    // right one is absolute -- so every file and directory operation escaped the store for an
    // absolute path (SR-AUD-241).  .NET's strip alone is not sufficient here: it leaves `..`
    // traversal and symbolic links pointing out of the store, both measured escaping, so the
    // resolver also enforces lexical and link-resolved containment.
    std::filesystem::path IsolatedStorageFile::fullPath(const std::string& relativePath,
                                                        const char* paramName) const
    {
        // An embedded NUL is truncated by every native filesystem call, so the object operated
        // on would not be the one the caller named.
        if (relativePath.find('\0') != std::string::npos)
            throw System::ArgumentException("Path must not contain an embedded NUL character.",
                                            paramName);

        std::size_t start = 0;
        while (start < relativePath.size() && isDirectorySeparator(relativePath[start]))
            ++start;
        const std::string stripped = relativePath.substr(start);

        // "" and "/" both resolve to the storage root itself; remove() would delete it.
        if (stripped.empty())
            throw System::ArgumentException("Path must not be empty.", paramName);

        const std::filesystem::path relative(stripped);

        // A Windows drive or UNC root survives the separator strip because it is a root_name,
        // not a leading separator.  This port has no drive semantics to reinterpret it into,
        // so it is rejected rather than invented.  On POSIX no such path has a root here.
        if (relative.has_root_path())
            throw System::ArgumentException(ContainmentMessage, paramName);

        const std::filesystem::path normalizedRoot = rootDirectory_.lexically_normal();
        const std::filesystem::path candidate = (normalizedRoot / relative).lexically_normal();
        if (!isWithin(normalizedRoot, candidate))
            throw System::ArgumentException(ContainmentMessage, paramName);

        // Link-resolved containment.  weakly_canonical resolves every symbolic link in the
        // existing prefix and appends a not-yet-existing remainder literally, which is exactly
        // what create and move destinations need.  The error_code overload is mandatory: the
        // throwing one would put a std::filesystem_error across a public door.  When the root
        // itself cannot be canonicalized the lexical verdict above already stands.
        std::error_code rootEc;
        const std::filesystem::path canonicalRoot =
            std::filesystem::weakly_canonical(normalizedRoot, rootEc);
        if (!rootEc) {
            std::error_code candidateEc;
            const std::filesystem::path canonicalCandidate =
                std::filesystem::weakly_canonical(candidate, candidateEc);
            if (!candidateEc && canonicalCandidate != canonicalRoot
                    && !isWithin(canonicalRoot, canonicalCandidate))
                throw System::ArgumentException(ContainmentMessage, paramName);
        }

        // The operation runs on the lexically normalized path rather than the canonical one, so
        // a final component that is a symbolic link keeps its meaning: DeleteFile removes the
        // link, not whatever it points at.
        return candidate;
    }

    // Verified against IsolatedStorageFile.cs's EnsureStoreIsValid(): real .NET checks this at
    // the top of every file/directory operation. This port set disposed_ in Close()/Remove()/
    // Dispose() but never checked it anywhere, so every operation remained silently usable on a
    // closed/removed store.
    void IsolatedStorageFile::throwIfDisposed() const
    {
        if (disposed_)
            throw System::ObjectDisposedException("IsolatedStorageFile", "Store must be open for this operation.");
    }

    IsolatedStorageFile IsolatedStorageFile::GetUserStoreForApplication()
    {
        return IsolatedStorageFile(SharpRuntime::Storage::StoragePaths::GetIsolatedStorageRoot(),
                                    IsolatedStorageScope::Application | IsolatedStorageScope::User);
    }

    IsolatedStorageFile IsolatedStorageFile::GetUserStoreForAssembly()
    {
        return IsolatedStorageFile(SharpRuntime::Storage::StoragePaths::GetIsolatedStorageRoot(),
                                    IsolatedStorageScope::Assembly | IsolatedStorageScope::User);
    }

    // --- File operations ---

    bool IsolatedStorageFile::FileExists(const std::string& relativePath) const
    {
        throwIfDisposed();
        const auto fp = fullPath(relativePath, "relativePath");
        return std::filesystem::exists(fp) && std::filesystem::is_regular_file(fp);
    }

    IsolatedStorageFileStream IsolatedStorageFile::OpenFile(
        const std::string& relativePath,
        System::IO::FileMode mode) const
    {
        throwIfDisposed();
        return IsolatedStorageFileStream(fullPath(relativePath, "relativePath"), mode);
    }

    IsolatedStorageFileStream IsolatedStorageFile::CreateFile(const std::string& relativePath) const
    {
        return OpenFile(relativePath, System::IO::FileMode::Create);
    }

    void IsolatedStorageFile::DeleteFile(const std::string& relativePath) const
    {
        throwIfDisposed();
        std::error_code ec;
        std::filesystem::remove(fullPath(relativePath, "relativePath"), ec);
        if (ec)
            throw IsolatedStorageException("Failed to delete isolated storage file: " + relativePath);
    }

    void IsolatedStorageFile::CopyFile(const std::string& src, const std::string& dst) const
    {
        CopyFile(src, dst, false);
    }

    void IsolatedStorageFile::CopyFile(const std::string& src, const std::string& dst, bool overwrite) const
    {
        System::ArgumentException::ThrowIfNullOrEmpty(src, "sourceFileName");
        System::ArgumentException::ThrowIfNullOrEmpty(dst, "destinationFileName");
        throwIfDisposed();
        auto opts = overwrite
            ? std::filesystem::copy_options::overwrite_existing
            : std::filesystem::copy_options::none;
        // Resolved into locals, in declared parameter order: C++ leaves the evaluation order
        // of call arguments unspecified, so validating inside the call expression would make
        // which parameter gets named on a double violation compiler-dependent.
        const auto srcPath = fullPath(src, "sourceFileName");
        const auto dstPath = fullPath(dst, "destinationFileName");
        std::error_code ec;
        std::filesystem::copy_file(srcPath, dstPath, opts, ec);
        if (ec)
            throw IsolatedStorageException("Failed to copy isolated storage file: " + src);
    }

    void IsolatedStorageFile::MoveFile(const std::string& src, const std::string& dst) const
    {
        System::ArgumentException::ThrowIfNullOrEmpty(src, "sourceFileName");
        System::ArgumentException::ThrowIfNullOrEmpty(dst, "destinationFileName");
        throwIfDisposed();
        const auto srcPath = fullPath(src, "sourceFileName");
        const auto dstPath = fullPath(dst, "destinationFileName");
        std::error_code ec;
        std::filesystem::rename(srcPath, dstPath, ec);
        if (ec)
            throw IsolatedStorageException("Failed to move isolated storage file: " + src);
    }

    std::vector<std::string> IsolatedStorageFile::GetFileNames(const std::string& searchPattern) const
    {
        throwIfDisposed();
        std::vector<std::string> names;
        if (!std::filesystem::exists(rootDirectory_)) return names;
        for (const auto& entry : std::filesystem::directory_iterator(rootDirectory_)) {
            if (!entry.is_regular_file()) continue;
            std::string name = entry.path().filename().string();
            if (globMatch(searchPattern, name))
                names.push_back(name);
        }
        std::sort(names.begin(), names.end());
        return names;
    }

    // --- Directory operations ---

    bool IsolatedStorageFile::DirectoryExists(const std::string& relativePath) const
    {
        throwIfDisposed();
        const auto fp = fullPath(relativePath, "relativePath");
        return std::filesystem::exists(fp) && std::filesystem::is_directory(fp);
    }

    void IsolatedStorageFile::CreateDirectory(const std::string& relativePath) const
    {
        throwIfDisposed();
        std::error_code ec;
        std::filesystem::create_directories(fullPath(relativePath, "relativePath"), ec);
        if (ec)
            throw IsolatedStorageException("Failed to create isolated storage directory: " + relativePath);
    }

    void IsolatedStorageFile::DeleteDirectory(const std::string& relativePath) const
    {
        throwIfDisposed();
        // Verified against IsolatedStorageFile.cs's DeleteDirectory: real .NET calls
        // Directory.Delete(fullPath, recursive: false) -- non-recursive, fails on a non-empty
        // directory. This port previously used remove_all (recursive), silently deleting an
        // entire subtree instead of matching .NET's "must be empty" contract.
        std::error_code ec;
        std::filesystem::remove(fullPath(relativePath, "relativePath"), ec);
        if (ec)
            throw IsolatedStorageException("Failed to delete isolated storage directory: " + relativePath);
    }

    void IsolatedStorageFile::MoveDirectory(const std::string& src, const std::string& dst) const
    {
        System::ArgumentException::ThrowIfNullOrEmpty(src, "sourceDirectoryName");
        System::ArgumentException::ThrowIfNullOrEmpty(dst, "destinationDirectoryName");
        throwIfDisposed();
        const auto srcPath = fullPath(src, "sourceDirectoryName");
        const auto dstPath = fullPath(dst, "destinationDirectoryName");
        std::error_code ec;
        std::filesystem::rename(srcPath, dstPath, ec);
        if (ec)
            throw IsolatedStorageException("Failed to move isolated storage directory: " + src);
    }

    std::vector<std::string> IsolatedStorageFile::GetDirectoryNames(const std::string& searchPattern) const
    {
        throwIfDisposed();
        std::vector<std::string> names;
        if (!std::filesystem::exists(rootDirectory_)) return names;
        for (const auto& entry : std::filesystem::directory_iterator(rootDirectory_)) {
            if (!entry.is_directory()) continue;
            std::string name = entry.path().filename().string();
            if (globMatch(searchPattern, name))
                names.push_back(name);
        }
        std::sort(names.begin(), names.end());
        return names;
    }

    // --- Store lifecycle ---

    void IsolatedStorageFile::Remove()
    {
        std::error_code ec;
        std::filesystem::remove_all(rootDirectory_, ec);
        disposed_ = true;
    }

    void IsolatedStorageFile::Close()
    {
        disposed_ = true;
    }

    void IsolatedStorageFile::Dispose()
    {
        Close();
    }

    // --- Space properties ---

    SharpRuntime::longcs IsolatedStorageFile::getAvailableFreeSpaceProperty() const
    {
        std::error_code ec;
        auto si = std::filesystem::space(rootDirectory_, ec);
        if (ec) return 0;
        return static_cast<SharpRuntime::longcs>(si.available);
    }

    SharpRuntime::longcs IsolatedStorageFile::getUsedSizeProperty() const
    {
        SharpRuntime::longcs total = 0;
        if (!std::filesystem::exists(rootDirectory_)) return total;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(rootDirectory_)) {
            if (entry.is_regular_file()) {
                std::error_code ec;
                total += static_cast<SharpRuntime::longcs>(entry.file_size(ec));
            }
        }
        return total;
    }

    SharpRuntime::longcs IsolatedStorageFile::getQuotaProperty() const
    {
        // Verified against IsolatedStorageFile.cs's Quota property: real .NET does not enforce
        // quotas and always returns long.MaxValue. This port previously inherited the
        // IsolatedStorage base class's default of 0, which any caller checking "is there quota
        // remaining" against would read as "no space available" instead of "unlimited."
        return std::numeric_limits<SharpRuntime::longcs>::max();
    }

    const std::filesystem::path& IsolatedStorageFile::getRootDirectoryProperty() const
    {
        return rootDirectory_;
    }
}
