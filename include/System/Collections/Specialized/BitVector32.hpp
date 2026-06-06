// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <string>

namespace System::Collections::Specialized {

    struct BitVector32 {
        struct Section {
            uint16_t mask_;
            uint16_t offset_;
            Section(uint16_t mask, uint16_t offset) : mask_(mask), offset_(offset) {}
            [[nodiscard]] int getMaskProperty()   const { return mask_; }
            [[nodiscard]] int getOffsetProperty() const { return offset_; }
        };

    private:
        uint32_t data_;

    public:
        BitVector32() : data_(0) {}
        explicit BitVector32(int data) : data_(static_cast<uint32_t>(data)) {}
        BitVector32(const BitVector32&) = default;

        [[nodiscard]] uint32_t getDataProperty() const { return data_; }

        bool operator[](int bit) const { return (data_ & static_cast<uint32_t>(bit)) == static_cast<uint32_t>(bit); }

        void set(int bit, bool value) {
            if (value) data_ |= static_cast<uint32_t>(bit);
            else       data_ &= ~static_cast<uint32_t>(bit);
        }

        int operator[](const Section& section) const {
            return static_cast<int>((data_ >> section.offset_) & section.mask_);
        }

        void set(const Section& section, int value) {
            data_ = (data_ & ~(static_cast<uint32_t>(section.mask_) << section.offset_))
                  | ((static_cast<uint32_t>(value) & section.mask_) << section.offset_);
        }

        static int CreateMask() { return CreateMask(0); }
        static int CreateMask(int previous) {
            if (previous == 0) return 1;
            return static_cast<int>(static_cast<unsigned>(previous) << 1);
        }

        static Section CreateSection(uint16_t maxValue) { return CreateSection(maxValue, Section(0, 0)); }
        static Section CreateSection(uint16_t maxValue, const Section& previous) {
            uint16_t offset = previous.offset_ ? static_cast<uint16_t>(previous.offset_ + __builtin_popcount(previous.mask_)) : 0;
            uint16_t mask = 0;
            uint16_t v = maxValue;
            while (v > 0) { mask = static_cast<uint16_t>((mask << 1) | 1); v >>= 1; }
            return Section(mask, offset);
        }

        bool operator==(const BitVector32& o) const { return data_ == o.data_; }
        bool operator!=(const BitVector32& o) const { return data_ != o.data_; }

        [[nodiscard]] std::string ToString() const {
            std::string s = "BitVector32{";
            for (int i = 31; i >= 0; --i) s += (data_ & (1u << i)) ? '1' : '0';
            return s + "}";
        }
    };

} // namespace System::Collections::Specialized
