// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <vector>
#include "System/IO/Directory.hpp"

namespace System::IO {

    enum class DriveType {
        Unknown         = 0,
        NoRootDirectory = 1,
        Removable       = 2,
        Fixed           = 3,
        Network         = 4,
        CDRom           = 5,
        Ram             = 6,
    };

    class DriveInfo {
        std::string name_;

    public:
        explicit DriveInfo(const std::string& driveName) : name_(driveName) {}

        [[nodiscard]] const std::string& getNameProperty() const { return name_; }
        [[nodiscard]] bool               getIsReadyProperty()    const { return Directory::Exists(name_); }
        [[nodiscard]] DriveType          getDriveTypeProperty()  const { return DriveType::Fixed; }
        [[nodiscard]] long long          getAvailableFreeSpaceProperty()  const { return 0LL; }
        [[nodiscard]] long long          getTotalFreeSpaceProperty()      const { return 0LL; }
        [[nodiscard]] long long          getTotalSizeProperty()           const { return 0LL; }
        [[nodiscard]] std::string        getVolumeLabel()                 const { return name_; }
        [[nodiscard]] std::string        getDriveFormatProperty()         const { return "Unknown"; }

        [[nodiscard]] std::string ToString() const { return name_; }

        static std::vector<DriveInfo> GetDrives() {
#ifdef _WIN32
            std::vector<DriveInfo> drives;
            DWORD mask = GetLogicalDrives();
            for (int i = 0; i < 26; ++i) {
                if (mask & (1 << i)) {
                    std::string name(1, char('A' + i));
                    name += ":\\";
                    drives.emplace_back(name);
                }
            }
            return drives;
#else
            return { DriveInfo("/") };
#endif
        }
    };

} // namespace System::IO
