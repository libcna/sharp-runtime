// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include "System/ReadOnlyMemory.hpp"

namespace System {

    /**
     * @brief A lightweight abstraction for a payload of bytes that supports
     * converting between string, bytes, and files.
     *
     * C++ counterpart of .NET System.BinaryData.
     * Wraps a ReadOnlyMemory&lt;uint8_t&gt; and provides factory methods and
     * conversions. JSON serialization methods are omitted (out of scope for
     * the game-dev port target).
     */
    class BinaryData {
        ReadOnlyMemory<uint8_t> bytes_;
        std::string             mediaType_;

    public:
        // -----------------------------------------------------------------------
        // Static singleton
        // -----------------------------------------------------------------------

        /** @brief Returns an empty BinaryData instance. */
        [[nodiscard]] static const BinaryData& Empty() {
            static BinaryData instance{ReadOnlyMemory<uint8_t>{}};
            return instance;
        }

        // -----------------------------------------------------------------------
        // Constructors
        // -----------------------------------------------------------------------

        /**
         * @brief Creates a BinaryData instance by wrapping the provided byte vector.
         * @param data The data to wrap (copied into an internal vector).
         */
        explicit BinaryData(const std::vector<uint8_t>& data)
            : bytes_(data.data(), static_cast<int>(data.size())) {}

        /**
         * @brief Creates a BinaryData instance by wrapping the provided byte vector
         * and sets the media type.
         * @param data      The data to wrap.
         * @param mediaType MIME type string (e.g. "application/octet-stream").
         */
        BinaryData(const std::vector<uint8_t>& data, std::string mediaType)
            : bytes_(data.data(), static_cast<int>(data.size())),
              mediaType_(std::move(mediaType)) {}

        /**
         * @brief Creates a BinaryData instance by wrapping the provided ReadOnlyMemory.
         * @param data Byte data to wrap.
         */
        explicit BinaryData(ReadOnlyMemory<uint8_t> data)
            : bytes_(data) {}

        /**
         * @brief Creates a BinaryData instance by wrapping the provided ReadOnlyMemory
         * and sets the media type.
         * @param data      Byte data to wrap.
         * @param mediaType MIME type string.
         */
        BinaryData(ReadOnlyMemory<uint8_t> data, std::string mediaType)
            : bytes_(data), mediaType_(std::move(mediaType)) {}

        /**
         * @brief Creates a BinaryData instance from a string using UTF-8 encoding.
         * @param data The string data.
         */
        explicit BinaryData(const std::string& data)
            : bytes_(reinterpret_cast<const uint8_t*>(data.data()),
                     static_cast<int>(data.size())) {}

        /**
         * @brief Creates a BinaryData instance from a string using UTF-8 encoding
         * and sets the media type.
         * @param data      The string data.
         * @param mediaType MIME type string.
         */
        BinaryData(const std::string& data, std::string mediaType)
            : bytes_(reinterpret_cast<const uint8_t*>(data.data()),
                     static_cast<int>(data.size())),
              mediaType_(std::move(mediaType)) {}

        // -----------------------------------------------------------------------
        // Properties
        // -----------------------------------------------------------------------

        /**
         * @brief Gets the number of bytes in this data.
         * @return Byte count.
         */
        [[nodiscard]] int getLengthProperty() const noexcept {
            return bytes_.getLengthProperty();
        }

        /**
         * @brief Gets a value indicating whether this data is empty.
         * @return true if the length is zero.
         */
        [[nodiscard]] bool getIsEmptyProperty() const noexcept {
            return bytes_.getIsEmptyProperty();
        }

        /**
         * @brief Gets the MIME type of this data (empty string if not set).
         * @return MIME type string.
         */
        [[nodiscard]] const std::string& getMediaTypeProperty() const noexcept {
            return mediaType_;
        }

        // -----------------------------------------------------------------------
        // Static factory methods
        // -----------------------------------------------------------------------

        /**
         * @brief Creates a BinaryData instance by wrapping the provided ReadOnlyMemory.
         * @param data Byte data to wrap.
         * @return New BinaryData instance.
         */
        [[nodiscard]] static BinaryData FromBytes(ReadOnlyMemory<uint8_t> data) {
            return BinaryData(data);
        }

        /**
         * @brief Creates a BinaryData instance by wrapping the provided ReadOnlyMemory
         * and sets the media type.
         * @param data      Byte data to wrap.
         * @param mediaType MIME type string.
         * @return New BinaryData instance.
         */
        [[nodiscard]] static BinaryData FromBytes(ReadOnlyMemory<uint8_t> data,
                                                   std::string mediaType) {
            return BinaryData(data, std::move(mediaType));
        }

        /**
         * @brief Creates a BinaryData instance by wrapping the provided byte vector.
         * @param data The array to wrap.
         * @return New BinaryData instance.
         */
        [[nodiscard]] static BinaryData FromBytes(const std::vector<uint8_t>& data) {
            return BinaryData(data);
        }

        /**
         * @brief Creates a BinaryData instance from a string using UTF-8 encoding.
         * @param data The string data.
         * @return New BinaryData instance.
         */
        [[nodiscard]] static BinaryData FromString(const std::string& data) {
            return BinaryData(data);
        }

        /**
         * @brief Creates a BinaryData instance from a string using UTF-8 encoding
         * and sets the media type.
         * @param data      The string data.
         * @param mediaType MIME type string.
         * @return New BinaryData instance.
         */
        [[nodiscard]] static BinaryData FromString(const std::string& data,
                                                    std::string mediaType) {
            return BinaryData(data, std::move(mediaType));
        }

        /**
         * @brief Creates a BinaryData instance from the specified file.
         * @param path Path to the file.
         * @return New BinaryData instance containing all bytes from the file.
         * @throws std::runtime_error if the file cannot be opened.
         */
        [[nodiscard]] static BinaryData FromFile(const std::string& path) {
            return FromFile(path, "");
        }

        /**
         * @brief Creates a BinaryData instance from the specified file and sets the media type.
         * @param path      Path to the file.
         * @param mediaType MIME type string.
         * @return New BinaryData instance containing all bytes from the file.
         * @throws std::runtime_error if the file cannot be opened.
         */
        [[nodiscard]] static BinaryData FromFile(const std::string& path,
                                                  std::string mediaType) {
            std::ifstream file(path, std::ios::binary);
            if (!file) throw std::runtime_error("BinaryData::FromFile: cannot open " + path);
            std::vector<uint8_t> bytes(
                (std::istreambuf_iterator<char>(file)),
                std::istreambuf_iterator<char>());
            return BinaryData(bytes, std::move(mediaType));
        }

        // -----------------------------------------------------------------------
        // Conversion methods
        // -----------------------------------------------------------------------

        /**
         * @brief Converts the value of this instance to a string using UTF-8 decoding.
         * @return A string decoded from the bytes of this instance.
         */
        [[nodiscard]] std::string ToString() const {
            return std::string(reinterpret_cast<const char*>(bytes_.getPointer()),
                               static_cast<std::size_t>(bytes_.getLengthProperty()));
        }

        /**
         * @brief Converts the BinaryData to a byte vector.
         * @return A vector containing a copy of the underlying bytes.
         */
        [[nodiscard]] std::vector<uint8_t> ToArray() const {
            return bytes_.ToArray();
        }

        /**
         * @brief Returns a read-only view over the underlying bytes.
         * @return ReadOnlyMemory&lt;uint8_t&gt; over the backing store.
         */
        [[nodiscard]] ReadOnlyMemory<uint8_t> ToMemory() const noexcept {
            return bytes_;
        }

        /**
         * @brief Returns a new BinaryData that wraps the same bytes with a different media type.
         * @param mediaType New MIME type string.
         * @return New BinaryData with the updated media type.
         */
        [[nodiscard]] BinaryData WithMediaType(std::string mediaType) const {
            return BinaryData(bytes_, std::move(mediaType));
        }

        /**
         * @brief Provides direct access to the underlying bytes.
         * @param index Zero-based byte index.
         * @return The byte at the specified index.
         */
        [[nodiscard]] uint8_t operator[](int index) const {
            return bytes_[index];
        }
    };

} // namespace System
