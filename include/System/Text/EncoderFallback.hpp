// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Text {

    /** Abstract base for handling characters that cannot be encoded. */
    class EncoderFallback {
    public:
        virtual ~EncoderFallback() = default;
        /** Returns fallback bytes for a character that cannot be encoded. */
        [[nodiscard]] virtual std::vector<SharpRuntime::bytecs> GetFallbackBytes(char unknownChar) const = 0;
        /** Gets the maximum number of bytes produced by one fallback substitution. */
        [[nodiscard]] virtual SharpRuntime::intcs getMaxByteCountProperty() const = 0;

        /** Returns the singleton replacement fallback (substitutes '?'). */
        static std::shared_ptr<EncoderFallback> ReplacementFallback();
        /** Returns the singleton exception fallback (throws on unencodable characters). */
        static std::shared_ptr<EncoderFallback> ExceptionFallback();
    };

    /** Encoder fallback that substitutes a replacement byte sequence for unencodable characters. */
    class EncoderReplacementFallback : public EncoderFallback {
        std::string replacement_;
    public:
        /** Constructs the fallback with the given replacement string (default "?"). */
        explicit EncoderReplacementFallback(const std::string& replacement = "?") : replacement_(replacement) {}
        /** Returns the replacement bytes for any unencodable character. */
        [[nodiscard]] std::vector<SharpRuntime::bytecs> GetFallbackBytes(char) const override {
            return std::vector<SharpRuntime::bytecs>(replacement_.begin(), replacement_.end());
        }
        /** Gets the byte count of the replacement string. */
        [[nodiscard]] SharpRuntime::intcs getMaxByteCountProperty() const override {
            return static_cast<SharpRuntime::intcs>(replacement_.size());
        }
        /** Gets the default replacement string used by this fallback. */
        [[nodiscard]] const std::string& getDefaultStringProperty() const { return replacement_; }
    };

    /** Encoder fallback that throws an exception for unencodable characters. */
    class EncoderExceptionFallback : public EncoderFallback {
    public:
        /** Always throws std::runtime_error. */
        [[nodiscard]] std::vector<SharpRuntime::bytecs> GetFallbackBytes(char) const override {
            throw std::runtime_error("Unable to encode character: EncoderExceptionFallback.");
        }
        /** Returns 0 (no bytes produced; exception is thrown instead). */
        [[nodiscard]] SharpRuntime::intcs getMaxByteCountProperty() const override { return 0; }
    };

    inline std::shared_ptr<EncoderFallback> EncoderFallback::ReplacementFallback() {
        static auto inst = std::make_shared<EncoderReplacementFallback>("?");
        return inst;
    }
    inline std::shared_ptr<EncoderFallback> EncoderFallback::ExceptionFallback() {
        static auto inst = std::make_shared<EncoderExceptionFallback>();
        return inst;
    }

} // namespace System::Text
