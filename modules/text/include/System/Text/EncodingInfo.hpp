// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Text/UTF32Encoding.hpp"
#include "System/ArgumentException.hpp"
#include <memory>
#include <string>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Text/Encoding.hpp"

namespace System::Text {

    /** Provides information about a character encoding (code page, name, display name). */
    class EncodingInfo {
        SharpRuntime::intcs codePage_;
        std::string name_;
        std::string displayName_;

    public:
        /** Constructs an EncodingInfo with the given code-page identifier, IANA name, and display name. */
        EncodingInfo(SharpRuntime::intcs codePage, std::string name, std::string displayName)
            : codePage_(codePage), name_(std::move(name)), displayName_(std::move(displayName)) {}

        /** Gets the code-page identifier for this encoding. */
        [[nodiscard]] SharpRuntime::intcs getCodePageProperty() const { return codePage_; }
        /** Gets the IANA name of this encoding. */
        [[nodiscard]] const std::string& getNameProperty() const { return name_; }
        /** Gets the human-readable name of this encoding. */
        [[nodiscard]] const std::string& getDisplayNameProperty() const { return displayName_; }

        /**
         * @brief Returns the Encoding this EncodingInfo describes.
         *
         * C++ counterpart of .NET `EncodingInfo.GetEncoding()`, which is
         * `Provider?.GetEncoding(CodePage) ?? Encoding.GetEncoding(CodePage)`
         * (`EncodingInfo.cs:56`) -- it resolves **its own code page**, and that is the whole
         * content of ticket #2021 (SR-AUD-299).
         *
         * Before that ticket this returned `Encoding::UTF8()` unconditionally, so
         * `EncodingInfo(20127, "us-ascii", "US-ASCII").GetEncoding()` handed back an object
         * whose `getCodePageProperty()` was **65001** and which encoded `é` as UTF-8. An object
         * that reports one code page and behaves as another is worse than one that refuses.
         *
         * This runtime has no `EncodingProvider` registry (see that type's own doc-comment), so
         * the resolution covers the seven code pages this component implements. Anything else is
         * rejected rather than silently substituted.
         *
         * @throws System::ArgumentException if this instance's code page is not one this
         *         runtime implements. The message is .NET's `Argument_EncodingNotSupported`.
         */
        [[nodiscard]] std::shared_ptr<Encoding> GetEncoding() const {
            switch (codePage_) {
                case 65001: return Encoding::UTF8();
                case 20127: return Encoding::ASCII();
                case 1200:  return Encoding::Unicode();
                case 1201:  return Encoding::BigEndianUnicode();
                case 12000: return Encoding::UTF32();
                // 12001 (UTF-32 big-endian) has no factory in this component; the type exists
                // and is constructible, so it is resolved directly rather than rejected.
                case 12001: return std::make_shared<UTF32Encoding>(true, true);
                case 65000: return Encoding::UTF7();
                case 28591: return Encoding::Latin1();
                default: break;
            }
            throw System::ArgumentException(
                "'" + std::to_string(codePage_) +
                    "' is not a supported encoding name. For information on defining a custom "
                    "encoding, see the documentation for the Encoding.RegisterProvider method.",
                "codepage");
        }

    };

} // namespace System::Text
