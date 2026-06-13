// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/Version.hpp"

namespace System {

    /// Provides a unique identifier for a manifest-based application.
    class ApplicationId {
        std::string name_;
        Version version_;
        std::string processorArchitecture_;
        std::string culture_;
        std::string publicKeyToken_;

    public:
        /// Initializes a new instance with the specified identity components.
        ApplicationId(const std::string& publicKeyToken,
                      const std::string& name,
                      const Version& version,
                      const std::string& processorArchitecture,
                      const std::string& culture)
            : name_(name), version_(version),
              processorArchitecture_(processorArchitecture),
              culture_(culture), publicKeyToken_(publicKeyToken) {}

        /// Returns the name component of the application identity.
        [[nodiscard]] const std::string& getNameProperty()                    const { return name_; }
        /// Returns the version component of the application identity.
        [[nodiscard]] const Version&     getVersionProperty()                 const { return version_; }
        /// Returns the processor architecture component of the application identity.
        [[nodiscard]] const std::string& getProcessorArchitectureProperty()   const { return processorArchitecture_; }
        /// Returns the culture component of the application identity.
        [[nodiscard]] const std::string& getCultureProperty()                 const { return culture_; }
        /// Returns the public key token component of the application identity.
        [[nodiscard]] const std::string& getPublicKeyTokenProperty()          const { return publicKeyToken_; }

        /// Returns a string representation of the application identity.
        [[nodiscard]] std::string ToString() const {
            return name_ + ", Version=" + version_.ToString()
                + ", Culture=" + culture_
                + ", ProcessorArchitecture=" + processorArchitecture_;
        }
    };

} // namespace System
