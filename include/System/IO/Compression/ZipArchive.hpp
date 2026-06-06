// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <vector>
#include "System/IO/Stream.hpp"
#include "System/NotImplementedException.hpp"

// NOTE: Full implementation requires libzip or miniz.
// To implement:
//   - miniz (single-header, MIT/public domain): include miniz.h and use mz_zip_reader_*/mz_zip_writer_* API.
//   - libzip: find_package(libzip) + target_link_libraries(SHARP_RUNTIME zip)
//     Use zip_open / zip_stat / zip_fopen / zip_fread / zip_close.
// ZipArchive holds an open zip file; ZipArchiveEntry represents a single entry inside it.

namespace System::IO::Compression {

    /** @brief Specifies values for interacting with zip archive entries. */
    enum class ZipArchiveMode { Read = 0, Create = 1, Update = 2 };

    /**
     * @brief Represents a single entry in a zip archive.
     *
     * Partial C++ counterpart of .NET System.IO.Compression.ZipArchiveEntry.
     *
     * @note Status: Stub — throws NotImplementedException. Requires miniz or libzip.
     */
    class ZipArchiveEntry {
    public:
        [[nodiscard]] std::string getNameProperty()      const { throw NotImplementedException("ZipArchiveEntry requires miniz or libzip."); }
        [[nodiscard]] std::string getFullNameProperty()  const { throw NotImplementedException("ZipArchiveEntry requires miniz or libzip."); }
        [[nodiscard]] long long   getLengthProperty()    const { throw NotImplementedException("ZipArchiveEntry requires miniz or libzip."); }
        void Delete() { throw NotImplementedException("ZipArchiveEntry requires miniz or libzip."); }
    };

    /**
     * @brief Represents a zip archive.
     *
     * Partial C++ counterpart of .NET System.IO.Compression.ZipArchive.
     *
     * @note Status: Stub — constructor and all methods throw NotImplementedException.
     *   See comment at top of this file for integration instructions.
     */
    class ZipArchive {
    public:
        ZipArchive(Stream*, ZipArchiveMode = ZipArchiveMode::Read) {
            throw NotImplementedException(
                "ZipArchive is not implemented. "
                "Requires miniz (single-header, MIT) or libzip. "
                "See include/System/IO/Compression/ZipArchive.hpp for integration notes.");
        }

        ZipArchive(const std::string&, ZipArchiveMode = ZipArchiveMode::Read) {
            throw NotImplementedException(
                "ZipArchive is not implemented. "
                "Requires miniz (single-header, MIT) or libzip. "
                "See include/System/IO/Compression/ZipArchive.hpp for integration notes.");
        }

        [[nodiscard]] std::vector<ZipArchiveEntry> getEntriesProperty() const {
            throw NotImplementedException("ZipArchive requires miniz or libzip.");
        }

        ZipArchiveEntry GetEntry(const std::string&) {
            throw NotImplementedException("ZipArchive requires miniz or libzip.");
        }

        ZipArchiveEntry CreateEntry(const std::string&) {
            throw NotImplementedException("ZipArchive requires miniz or libzip.");
        }

        void Dispose() {}
    };

} // namespace System::IO::Compression
