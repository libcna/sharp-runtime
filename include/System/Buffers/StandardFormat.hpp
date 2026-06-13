// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <string>

namespace System::Buffers {

    /// Defines the format to use when formatting primitive values.
    class StandardFormat {
        uint8_t format_ = 0;
        uint8_t precision_;

    public:
        /// Sentinel value indicating that no precision is specified.
        static constexpr uint8_t NoPrecision = 255;
        /// Maximum allowed precision value.
        static constexpr uint8_t MaxPrecision = 99;

        /// Constructs a default StandardFormat with no symbol and no precision.
        StandardFormat() : precision_(NoPrecision) {}
        /// Constructs a StandardFormat with the given format symbol and optional precision.
        explicit StandardFormat(char symbol, uint8_t precision = NoPrecision)
            : format_(static_cast<uint8_t>(symbol)), precision_(precision) {}

        /// Returns the format symbol character.
        [[nodiscard]] char    getSymbolProperty()    const { return static_cast<char>(format_); }
        /// Returns the precision value, or NoPrecision if not set.
        [[nodiscard]] uint8_t getPrecisionProperty() const { return precision_; }
        /// Returns true if a precision value has been specified.
        [[nodiscard]] bool    getHasPrecisionProperty() const { return precision_ != NoPrecision; }

        /// Returns true if two StandardFormat values are equal.
        bool operator==(const StandardFormat& o) const { return format_ == o.format_ && precision_ == o.precision_; }
        /// Returns true if two StandardFormat values are not equal.
        bool operator!=(const StandardFormat& o) const { return !(*this == o); }

        /// Returns a string representation of this format, e.g. "G" or "D3".
        [[nodiscard]] std::string ToString() const {
            std::string s(1, static_cast<char>(format_));
            if (precision_ != NoPrecision) s += std::to_string(precision_);
            return s;
        }

        /// Parses a format string into a StandardFormat value.
        static StandardFormat Parse(const std::string& format) {
            if (format.empty()) return StandardFormat();
            char sym = format[0];
            if (format.size() > 1) {
                uint8_t prec = static_cast<uint8_t>(std::stoi(format.substr(1)));
                return StandardFormat(sym, prec);
            }
            return StandardFormat(sym);
        }
    };

} // namespace System::Buffers
