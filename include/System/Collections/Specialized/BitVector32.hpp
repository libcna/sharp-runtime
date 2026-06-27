// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <bit>
#include <cstdint>
#include <functional>
#include <string>

namespace System::Collections::Specialized {

/**
 * @brief A compact structure that stores Boolean values and small integers as a 32-bit integer.
 *
 * C++ counterpart of .NET System.Collections.Specialized.BitVector32.
 * Provides two usage modes: per-bit Boolean flags (using CreateMask) and
 * multi-bit integer sections (using CreateSection).
 */
struct BitVector32 {

    /**
     * @brief Represents a contiguous range of bits within a BitVector32.
     *
     * C++ counterpart of .NET System.Collections.Specialized.BitVector32.Section.
     * Created via BitVector32::CreateSection.
     */
    struct Section {
        uint16_t mask_;    ///< Bit mask for this section.
        uint16_t offset_;  ///< Bit offset of this section within the 32-bit value.

        /**
         * @brief Constructs a Section with the given mask and bit offset.
         * @param mask   The bit mask for the section.
         * @param offset The bit offset of the section.
         */
        Section(uint16_t mask, uint16_t offset) : mask_(mask), offset_(offset) {}

        /**
         * @brief Gets the bit mask for this section.
         *
         * C++ counterpart of .NET BitVector32.Section.Mask.
         * @return The mask as a 16-bit value (equivalent to .NET short).
         */
        [[nodiscard]] int getMaskProperty()   const { return mask_; }

        /**
         * @brief Gets the bit offset of this section within the 32-bit value.
         *
         * C++ counterpart of .NET BitVector32.Section.Offset.
         * @return The offset as a 16-bit value (equivalent to .NET short).
         */
        [[nodiscard]] int getOffsetProperty() const { return offset_; }

        /**
         * @brief Determines whether this Section equals another Section.
         *
         * C++ counterpart of .NET BitVector32.Section.Equals(BitVector32.Section).
         * @param other The Section to compare.
         * @return true if both mask and offset match.
         */
        [[nodiscard]] bool Equals(const Section& other) const {
            return mask_ == other.mask_ && offset_ == other.offset_;
        }

        /**
         * @brief Returns a hash code for this Section.
         *
         * C++ counterpart of .NET BitVector32.Section.GetHashCode().
         * @return A hash based on mask and offset.
         */
        [[nodiscard]] int GetHashCode() const {
            return static_cast<int>(mask_) ^ (static_cast<int>(offset_) << 16);
        }

        /**
         * @brief Returns a string representation of this Section.
         *
         * C++ counterpart of .NET BitVector32.Section.ToString().
         * @return A string in the form "Section{mask=N, offset=N}".
         */
        [[nodiscard]] std::string ToString() const {
            return "Section{mask=" + std::to_string(mask_) + ", offset=" + std::to_string(offset_) + "}";
        }

        /**
         * @brief Returns a string representation of the given Section.
         *
         * C++ counterpart of .NET BitVector32.Section.ToString(BitVector32.Section).
         * @param section The Section to convert.
         * @return A string in the form "Section{mask=N, offset=N}".
         */
        static std::string ToString(const Section& section) { return section.ToString(); }

        /**
         * @brief Returns true if two Section values are equal.
         * @param a First Section.
         * @param b Second Section.
         * @return true if a.Equals(b).
         */
        friend bool operator==(const Section& a, const Section& b) { return a.Equals(b); }

        /**
         * @brief Returns true if two Section values are not equal.
         * @param a First Section.
         * @param b Second Section.
         * @return true if !a.Equals(b).
         */
        friend bool operator!=(const Section& a, const Section& b) { return !a.Equals(b); }
    };

private:
    uint32_t data_;

public:
    /**
     * @brief Default-constructs a BitVector32 with all bits cleared.
     *
     * C++ counterpart of the default BitVector32 state in .NET.
     */
    BitVector32() : data_(0) {}

    /**
     * @brief Constructs a BitVector32 from the given integer bit pattern.
     *
     * C++ counterpart of .NET BitVector32(int).
     * @param data The initial 32-bit value.
     */
    explicit BitVector32(int data) : data_(static_cast<uint32_t>(data)) {}

    /** @brief Copy-constructs a BitVector32. */
    BitVector32(const BitVector32&) = default;

    /** @brief Copy-assigns a BitVector32. */
    BitVector32& operator=(const BitVector32&) = default;

    /**
     * @brief Gets the internal 32-bit storage value.
     *
     * C++ counterpart of .NET BitVector32.Data.
     * @return The raw 32-bit integer data.
     */
    [[nodiscard]] uint32_t getDataProperty() const { return data_; }

    /**
     * @brief Returns true if all bits specified by the bit mask @p bit are set.
     *
     * C++ counterpart of .NET BitVector32.Item[int] getter.
     * @param bit A bitmask specifying the bit(s) to test.
     * @return true if all specified bits are set.
     */
    [[nodiscard]] bool operator[](int bit) const {
        return (data_ & static_cast<uint32_t>(bit)) == static_cast<uint32_t>(bit);
    }

    /**
     * @brief Sets or clears the bits specified by @p bit.
     *
     * C++ counterpart of .NET BitVector32.Item[int] setter.
     * @param bit   A bitmask specifying the bit(s) to modify.
     * @param value true to set the bits; false to clear them.
     */
    void set(int bit, bool value) {
        if (value) data_ |= static_cast<uint32_t>(bit);
        else       data_ &= ~static_cast<uint32_t>(bit);
    }

    /**
     * @brief Returns the integer value stored in the given @p section.
     *
     * C++ counterpart of .NET BitVector32.Item[BitVector32.Section] getter.
     * @param section The section describing the bit range.
     * @return The integer value extracted from the section.
     */
    [[nodiscard]] int operator[](const Section& section) const {
        return static_cast<int>((data_ >> section.offset_) & section.mask_);
    }

    /**
     * @brief Stores @p value in the given @p section.
     *
     * C++ counterpart of .NET BitVector32.Item[BitVector32.Section] setter.
     * @param section The section describing the bit range.
     * @param value   The integer value to store (must fit within the section's mask).
     */
    void set(const Section& section, int value) {
        data_ = (data_ & ~(static_cast<uint32_t>(section.mask_) << section.offset_))
              | ((static_cast<uint32_t>(value) & section.mask_) << section.offset_);
    }

    /**
     * @brief Creates the first bit mask (value 1) for use with this BitVector32.
     *
     * C++ counterpart of .NET BitVector32.CreateMask().
     * @return The integer 1.
     */
    static int CreateMask() { return CreateMask(0); }

    /**
     * @brief Creates the next bit mask by shifting @p previous one bit to the left.
     *
     * C++ counterpart of .NET BitVector32.CreateMask(int).
     * @param previous The previous mask; pass 0 to get the first mask.
     * @return The next mask in the sequence.
     */
    static int CreateMask(int previous) {
        if (previous == 0) return 1;
        return static_cast<int>(static_cast<unsigned>(previous) << 1);
    }

    /**
     * @brief Creates a Section that can hold a value in the range [0, maxValue].
     *
     * C++ counterpart of .NET BitVector32.CreateSection(short).
     * @param maxValue The maximum value the section must be able to hold.
     * @return A Section starting at bit 0.
     */
    static Section CreateSection(uint16_t maxValue) { return CreateSection(maxValue, Section(0, 0)); }

    /**
     * @brief Creates a Section following @p previous that can hold a value in [0, maxValue].
     *
     * C++ counterpart of .NET BitVector32.CreateSection(short, BitVector32.Section).
     * @param maxValue The maximum value the section must be able to hold.
     * @param previous The preceding Section; the new section starts just after it.
     * @return A new Section positioned after @p previous.
     */
    static Section CreateSection(uint16_t maxValue, const Section& previous) {
        uint16_t offset = (previous.mask_ != 0)
            ? static_cast<uint16_t>(previous.offset_ + std::popcount(static_cast<uint32_t>(previous.mask_)))
            : 0;
        uint16_t mask = 0;
        uint16_t v = maxValue;
        while (v > 0) { mask = static_cast<uint16_t>((mask << 1) | 1); v >>= 1; }
        return Section(mask, offset);
    }

    /**
     * @brief Determines whether this BitVector32 equals @p other.
     *
     * C++ counterpart of .NET BitVector32.Equals(BitVector32).
     * @param other The BitVector32 to compare.
     * @return true if both have identical bit patterns.
     */
    [[nodiscard]] bool Equals(const BitVector32& other) const { return data_ == other.data_; }

    /**
     * @brief Returns a hash code for this BitVector32.
     *
     * C++ counterpart of .NET BitVector32.GetHashCode().
     * @return The raw 32-bit data cast to int.
     */
    [[nodiscard]] int GetHashCode() const { return static_cast<int>(data_); }

    /**
     * @brief Returns true if both BitVector32 values have identical bit patterns.
     * @param o The value to compare against.
     * @return true if equal.
     */
    bool operator==(const BitVector32& o) const { return data_ == o.data_; }

    /**
     * @brief Returns true if the BitVector32 values differ.
     * @param o The value to compare against.
     * @return true if not equal.
     */
    bool operator!=(const BitVector32& o) const { return data_ != o.data_; }

    /**
     * @brief Returns a string representation of the bit pattern.
     *
     * C++ counterpart of .NET BitVector32.ToString().
     * @return A string in the form "BitVector32{XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX}" (32 bits, MSB first).
     */
    [[nodiscard]] std::string ToString() const {
        std::string s = "BitVector32{";
        for (int i = 31; i >= 0; --i) s += (data_ & (1u << i)) ? '1' : '0';
        return s + "}";
    }

    /**
     * @brief Returns a string representation of the given BitVector32.
     *
     * C++ counterpart of .NET BitVector32.ToString(BitVector32).
     * @param value The BitVector32 to convert.
     * @return The same string as @p value.ToString().
     */
    static std::string ToString(const BitVector32& value) { return value.ToString(); }
};

} // namespace System::Collections::Specialized
