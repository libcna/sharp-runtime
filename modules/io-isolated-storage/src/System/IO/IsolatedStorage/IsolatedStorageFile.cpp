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

    // The four doors below used throwing std::filesystem entry points, so a root that could
    // not be created or iterated put a native std::filesystem_error across the public API
    // instead of this module's IsolatedStorageException. Each now takes the error_code overload
    // and translates, keeping the original error as the inner exception.
    IsolatedStorageFile::IsolatedStorageFile(const std::filesystem::path& rootDirectory, IsolatedStorageScope scope)
        : rootDirectory_(rootDirectory)
    {
        scope_ = scope;
        std::error_code ec;
        std::filesystem::create_directories(rootDirectory_, ec);
        if (ec)
            throw IsolatedStorageException(
                "Failed to open isolated storage root: " + rootDirectory_.string()
                + " (" + ec.message() + ")");
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


    // -----------------------------------------------------------------------------------------
    // Ticket #2209 (2026-08-18): a DIRECTORY-QUALIFIED search pattern.
    //
    // Both enumeration doors used to iterate rootDirectory_ and glob the whole pattern against a
    // bare filename, so GetFileNames("sub/*") returned nothing although sub/nested.dat existed.
    // .NET's own source states the contract in a comment above each method:
    //
    //     // foo\abc*.txt will give all abc*.txt files in foo directory
    //     // foo\data* will give all directory names in foo directory that starts with data
    //
    // and implements it by delegating to Directory.EnumerateFiles(RootDirectory, searchPattern),
    // whose FileSystemEnumerableFactory.NormalizeInputs splits the pattern at its last separator,
    // joins the directory half onto the root, and matches only the final segment
    // (FileSystemEnumerableFactory.cs:45-56). The RESULT is still a bare name, not a sub-path:
    // .NET maps each hit through Path.GetFileName (IsolatedStorageFile.cs:177) precisely because
    // the store hides its own root.
    //
    // ONE DELIBERATE DEVIATION, and it is a narrowing on a security boundary. .NET does NOT run
    // the pattern through its containment helper -- GetFileNames/GetDirectoryNames are the only
    // two doors on the type that bypass GetFullPath -- so in .NET, GetFileNames("../*") escapes
    // the store and lists its parent. This port resolves the directory half through the same
    // fullPath() every other door uses, so it is rejected. Reproducing the escape would mean
    // introducing a confinement hole to match a reference that has one; the port is more
    // RESTRICTIVE here, which SA-8 does not reach, and the asymmetry is pinned by a test.
    //
    // "" and "sub/" both mean "everything in that directory", matching NormalizeInputs' own
    // "We also allowed for expression to be \"foo\\\" which would translate to \"foo\\*\"".
    std::filesystem::path IsolatedStorageFile::resolveSearchScope(const std::string& searchPattern,
                                                                  std::string&       glob) const
    {
        if (searchPattern.find('\0') != std::string::npos)
            throw System::ArgumentException("Path must not contain an embedded NUL character.",
                                            "searchPattern");

        std::size_t split = std::string::npos;
        for (std::size_t i = 0; i < searchPattern.size(); ++i)
            if (isDirectorySeparator(searchPattern[i])) split = i;

        if (split == std::string::npos) {
            glob = searchPattern.empty() ? "*" : searchPattern;
            return rootDirectory_;
        }

        glob = searchPattern.substr(split + 1);
        if (glob.empty()) glob = "*";

        const std::string directoryPart = searchPattern.substr(0, split);
        // A pattern that is nothing but separators names the root itself, which fullPath()
        // rejects as empty -- correctly for every other door, and wrongly for this one.
        std::size_t firstReal = 0;
        while (firstReal < directoryPart.size() && isDirectorySeparator(directoryPart[firstReal]))
            ++firstReal;
        if (firstReal == directoryPart.size()) return rootDirectory_;

        return fullPath(directoryPart, "searchPattern");
    }

    std::vector<std::string> IsolatedStorageFile::GetFileNames(const std::string& searchPattern) const
    {
        throwIfDisposed();
        std::vector<std::string> names;
        std::string              glob;
        const std::filesystem::path scope = resolveSearchScope(searchPattern, glob);
        if (!std::filesystem::exists(scope)) return names;
        std::error_code ec;
        std::filesystem::directory_iterator it(scope, ec);
        if (ec)
            throw IsolatedStorageException(
                "Failed to enumerate isolated storage files (" + ec.message() + ")");
        for (const auto& entry : it) {
            if (!entry.is_regular_file(ec) || ec) { ec.clear(); continue; }
            std::string name = entry.path().filename().string();
            if (globMatch(glob, name))
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
        std::string              glob;
        const std::filesystem::path scope = resolveSearchScope(searchPattern, glob);
        if (!std::filesystem::exists(scope)) return names;
        std::error_code ec;
        std::filesystem::directory_iterator it(scope, ec);
        if (ec)
            throw IsolatedStorageException(
                "Failed to enumerate isolated storage directories (" + ec.message() + ")");
        for (const auto& entry : it) {
            if (!entry.is_directory(ec) || ec) { ec.clear(); continue; }
            std::string name = entry.path().filename().string();
            if (globMatch(glob, name))
                names.push_back(name);
        }
        std::sort(names.begin(), names.end());
        return names;
    }

    // --- Store lifecycle ---

    // Verified against IsolatedStorageFile.cs: real .NET calls EnsureStoreIsValid() in Remove
    // and in all three space properties, exactly as it does in the file and directory
    // operations. This port guarded the ten operations and left these four unguarded, so a
    // closed or already-removed store still answered them and still deleted its own tree.
    void IsolatedStorageFile::Remove()
    {
        throwIfDisposed();
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
        throwIfDisposed();
        std::error_code ec;
        auto si = std::filesystem::space(rootDirectory_, ec);
        if (ec) return 0;
        return static_cast<SharpRuntime::longcs>(si.available);
    }

    SharpRuntime::longcs IsolatedStorageFile::getUsedSizeProperty() const
    {
        throwIfDisposed();
        SharpRuntime::longcs total = 0;
        if (!std::filesystem::exists(rootDirectory_)) return total;
        std::error_code ec;
        std::filesystem::recursive_directory_iterator it(rootDirectory_, ec);
        if (ec)
            throw IsolatedStorageException(
                "Failed to measure isolated storage usage (" + ec.message() + ")");
        for (std::filesystem::recursive_directory_iterator end; it != end; it.increment(ec)) {
            if (ec)
                throw IsolatedStorageException(
                    "Failed to measure isolated storage usage (" + ec.message() + ")");
            std::error_code entryEc;
            if (!it->is_regular_file(entryEc) || entryEc) continue;
            const auto size = it->file_size(entryEc);
            if (!entryEc) total += static_cast<SharpRuntime::longcs>(size);
        }
        return total;
    }

    SharpRuntime::longcs IsolatedStorageFile::getQuotaProperty() const
    {
        // Verified against IsolatedStorageFile.cs's Quota property: real .NET does not enforce
        // quotas and always returns long.MaxValue. This port previously inherited the
        // IsolatedStorage base class's default of 0, which any caller checking "is there quota
        // remaining" against would read as "no space available" instead of "unlimited."
        throwIfDisposed();
        return std::numeric_limits<SharpRuntime::longcs>::max();
    }

    const std::filesystem::path& IsolatedStorageFile::getRootDirectoryProperty() const
    {
        return rootDirectory_;
    }
}
