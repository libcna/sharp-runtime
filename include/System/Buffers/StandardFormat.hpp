// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <string>

namespace System::Buffers {

    /**
     * @brief Defines the format to use when formatting primitive values into text.
     *
     * C++ counterpart of .NET System.Buffers.StandardFormat.
     * A StandardFormat consists of a format symbol character (e.g. 'G', 'D', 'X')
     * and an optional precision value (0–99).
     */
    class StandardFormat {
        uint8_t format_ = 0;
        uint8_t precision_;

    public:
        /** @brief Sentinel value indicating that no precision is specified. */
        static constexpr uint8_t NoPrecision = 255;
        /** @brief Maximum allowed precision value. */
        static constexpr uint8_t MaxPrecision = 99;

        /** @brief Constructs a default StandardFormat with no symbol and no precision. */
        StandardFormat() : precision_(NoPrecision) {}
        /**
         * @brief Constructs a StandardFormat with the given format symbol and optional precision.
         * @param symbol    The format character (e.g. 'G', 'D', 'X', 'F').
         * @param precision The precision value (0–99), or NoPrecision if not specified.
         */
        explicit StandardFormat(char symbol, uint8_t precision = NoPrecision)
            : format_(static_cast<uint8_t>(symbol)), precision_(precision) {}

        /** @brief Returns the format symbol character. */
        [[nodiscard]] char    getSymbolProperty()       const { return static_cast<char>(format_); }
        /** @brief Returns the precision value, or NoPrecision if not set. */
        [[nodiscard]] uint8_t getPrecisionProperty()    const { return precision_; }
        /** @brief Returns true if a precision value has been specified. */
        [[nodiscard]] bool    getHasPrecisionProperty() const { return precision_ != NoPrecision; }

        /** @brief Returns true if two StandardFormat values are equal. */
        bool operator==(const StandardFormat& o) const { return format_ == o.format_ && precision_ == o.precision_; }
        /** @brief Returns true if two StandardFormat values are not equal. */
        bool operator!=(const StandardFormat& o) const { return !(*this == o); }

        /**
         * @brief Returns a string representation of this format, e.g. "G" or "D3".
         */
        [[nodiscard]] std::string ToString() const {
            std::string s(1, static_cast<char>(format_));
            if (precision_ != NoPrecision) s += std::to_string(precision_);
            return s;
        }

        /**
         * @brief Parses a format string into a StandardFormat value.
         * @param format A format string such as "G", "D3", or "F2".
         */
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
