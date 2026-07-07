// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/Compression/ZipFileExtensions.hpp"
#include "System/IO/Directory.hpp"
#include "System/IO/File.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"
#include "System/IO/Path.hpp"

#include <memory>

namespace System::IO::Compression {

    ZipArchiveEntry ZipFileExtensions::CreateEntryFromFile(ZipArchive& archive, const std::string& sourceFileName,
                                                            const std::string& entryName) {
        return CreateEntryFromFile(archive, sourceFileName, entryName, CompressionLevel::Optimal);
    }

    ZipArchiveEntry ZipFileExtensions::CreateEntryFromFile(ZipArchive& archive, const std::string& sourceFileName,
                                                            const std::string& entryName, CompressionLevel compressionLevel) {
        auto entry = archive.CreateEntry(entryName, compressionLevel);

        auto data = System::IO::File::ReadAllBytes(sourceFileName);
        std::unique_ptr<System::IO::Stream> dest(entry.Open());
        if (!data.empty()) {
            dest->Write(data.data(), 0, static_cast<System::IO::intcs>(data.size()));
        }
        return entry;
    }

    void ZipFileExtensions::ExtractToDirectory(ZipArchive& archive, const std::string& destinationDirectoryName) {
        ExtractToDirectory(archive, destinationDirectoryName, false);
    }

    void ZipFileExtensions::ExtractToDirectory(ZipArchive& archive, const std::string& destinationDirectoryName,
                                                bool overwriteFiles) {
        for (auto& entry : archive.getEntriesProperty()) {
            const std::string& fullName = entry.getFullNameProperty();
            // Entries whose name ends with a directory separator represent directories; skip them.
            if (!fullName.empty() && (fullName.back() == '/' || fullName.back() == '\\')) {
                continue;
            }

            const std::string destPath = System::IO::Path::Combine(destinationDirectoryName, fullName);
            const std::string destDir = System::IO::Path::GetDirectoryName(destPath);
            if (!destDir.empty() && !System::IO::Directory::Exists(destDir)) {
                System::IO::Directory::CreateDirectory(destDir);
            }

            ExtractToFile(entry, destPath, overwriteFiles);
        }
    }

    void ZipFileExtensions::ExtractToFile(ZipArchiveEntry& entry, const std::string& destinationFileName) {
        ExtractToFile(entry, destinationFileName, false);
    }

    void ZipFileExtensions::ExtractToFile(ZipArchiveEntry& entry, const std::string& destinationFileName, bool overwrite) {
        const System::IO::FileMode mode = overwrite ? System::IO::FileMode::Create : System::IO::FileMode::CreateNew;
        System::IO::FileStream out(destinationFileName, mode, System::IO::FileAccess::Write);

        std::unique_ptr<System::IO::Stream> source(entry.Open());
        System::IO::bytecs buffer[65536];
        System::IO::intcs n;
        while ((n = source->Read(buffer, 0, static_cast<System::IO::intcs>(sizeof(buffer)))) > 0) {
            out.Write(buffer, 0, n);
        }
        out.Close();
    }

} // namespace System::IO::Compression
