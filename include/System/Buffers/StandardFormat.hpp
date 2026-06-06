// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <string>

namespace System::Buffers {

    class StandardFormat {
        uint8_t format_ = 0;
        uint8_t precision_;

    public:
        static constexpr uint8_t NoPrecision = 255;
        static constexpr uint8_t MaxPrecision = 99;

        StandardFormat() : precision_(NoPrecision) {}
        explicit StandardFormat(char symbol, uint8_t precision = NoPrecision)
            : format_(static_cast<uint8_t>(symbol)), precision_(precision) {}

        [[nodiscard]] char    getSymbolProperty()    const { return static_cast<char>(format_); }
        [[nodiscard]] uint8_t getPrecisionProperty() const { return precision_; }
        [[nodiscard]] bool    getHasPrecisionProperty() const { return precision_ != NoPrecision; }

        bool operator==(const StandardFormat& o) const { return format_ == o.format_ && precision_ == o.precision_; }
        bool operator!=(const StandardFormat& o) const { return !(*this == o); }

        [[nodiscard]] std::string ToString() const {
            std::string s(1, static_cast<char>(format_));
            if (precision_ != NoPrecision) s += std::to_string(precision_);
            return s;
        }

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
