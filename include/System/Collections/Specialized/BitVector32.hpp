// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <bit>
#include <cstdint>
#include <string>

namespace System::Collections::Specialized {

    /** A compact structure that stores Boolean values and small integers as a 32-bit integer. */
    struct BitVector32 {
        /** Represents a section of a BitVector32 described by a contiguous range of bits. */
        struct Section {
            uint16_t mask_;
            uint16_t offset_;
            /** Constructs a Section with the given mask and bit offset. */
            Section(uint16_t mask, uint16_t offset) : mask_(mask), offset_(offset) {}
            /** Gets the mask for this section. */
            [[nodiscard]] int getMaskProperty()   const { return mask_; }
            /** Gets the bit offset of this section within the 32-bit value. */
            [[nodiscard]] int getOffsetProperty() const { return offset_; }
        };

    private:
        uint32_t data_;

    public:
        /** Default-constructs a BitVector32 with all bits cleared. */
        BitVector32() : data_(0) {}
        /** Constructs a BitVector32 from the given integer bit pattern. */
        explicit BitVector32(int data) : data_(static_cast<uint32_t>(data)) {}
        /** Copy-constructs a BitVector32. */
        BitVector32(const BitVector32&) = default;

        /** Gets the internal 32-bit storage value. */
        [[nodiscard]] uint32_t getDataProperty() const { return data_; }

        /** Returns true if all bits specified by the bit mask are set. */
        bool operator[](int bit) const { return (data_ & static_cast<uint32_t>(bit)) == static_cast<uint32_t>(bit); }

        /** Sets or clears the bits specified by the bit mask. */
        void set(int bit, bool value) {
            if (value) data_ |= static_cast<uint32_t>(bit);
            else       data_ &= ~static_cast<uint32_t>(bit);
        }

        /** Returns the integer value stored in the given section. */
        int operator[](const Section& section) const {
            return static_cast<int>((data_ >> section.offset_) & section.mask_);
        }

        /** Stores the given integer value in the given section. */
        void set(const Section& section, int value) {
            data_ = (data_ & ~(static_cast<uint32_t>(section.mask_) << section.offset_))
                  | ((static_cast<uint32_t>(value) & section.mask_) << section.offset_);
        }

        /** Creates the first bit mask for use with this BitVector32. */
        static int CreateMask() { return CreateMask(0); }
        /** Creates the next bit mask following the given previous mask. */
        static int CreateMask(int previous) {
            if (previous == 0) return 1;
            return static_cast<int>(static_cast<unsigned>(previous) << 1);
        }

        /** Creates a Section that can hold a value in the range [0, maxValue]. */
        static Section CreateSection(uint16_t maxValue) { return CreateSection(maxValue, Section(0, 0)); }
        /** Creates a Section that can hold a value in the range [0, maxValue] following the given previous Section. */
        static Section CreateSection(uint16_t maxValue, const Section& previous) {
            uint16_t offset = previous.offset_ ? static_cast<uint16_t>(previous.offset_ + std::popcount(static_cast<uint32_t>(previous.mask_))) : 0;
            uint16_t mask = 0;
            uint16_t v = maxValue;
            while (v > 0) { mask = static_cast<uint16_t>((mask << 1) | 1); v >>= 1; }
            return Section(mask, offset);
        }

        /** Returns true if both BitVector32 values have identical bit patterns. */
        bool operator==(const BitVector32& o) const { return data_ == o.data_; }
        /** Returns true if the BitVector32 values differ. */
        bool operator!=(const BitVector32& o) const { return data_ != o.data_; }

        /** Returns a string representation of the bit pattern. */
        [[nodiscard]] std::string ToString() const {
            std::string s = "BitVector32{";
            for (int i = 31; i >= 0; --i) s += (data_ & (1u << i)) ? '1' : '0';
            return s + "}";
        }
    };

} // namespace System::Collections::Specialized
