// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <stdexcept>
#include <string>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Text {

    /** Abstract base for handling byte sequences that cannot be decoded. */
    class DecoderFallback {
    public:
        virtual ~DecoderFallback() = default;
        /** Returns a fallback string for the given undecodable byte sequence. */
        [[nodiscard]] virtual std::string GetFallbackString(const SharpRuntime::bytecs* bytesUnknown, SharpRuntime::intcs byteCount) const = 0;
        /** Gets the maximum number of characters produced by one fallback substitution. */
        [[nodiscard]] virtual SharpRuntime::intcs getMaxCharCountProperty() const = 0;

        /** Returns the singleton replacement fallback (substitutes '?'). */
        static std::shared_ptr<DecoderFallback> ReplacementFallback();
        /** Returns the singleton exception fallback (throws on bad bytes). */
        static std::shared_ptr<DecoderFallback> ExceptionFallback();
    };

    /** Decoder fallback that substitutes a replacement string for undecodable bytes. */
    class DecoderReplacementFallback : public DecoderFallback {
        std::string replacement_;
    public:
        /** Constructs the fallback with the given replacement string (default "?"). */
        explicit DecoderReplacementFallback(const std::string& replacement = "?") : replacement_(replacement) {}
        /** Returns the replacement string for any undecodable byte sequence. */
        [[nodiscard]] std::string GetFallbackString(const SharpRuntime::bytecs*, SharpRuntime::intcs) const override { return replacement_; }
        /** Gets the length of the replacement string. */
        [[nodiscard]] SharpRuntime::intcs getMaxCharCountProperty() const override { return static_cast<SharpRuntime::intcs>(replacement_.size()); }
        /** Gets the default replacement string used by this fallback. */
        [[nodiscard]] const std::string& getDefaultStringProperty() const { return replacement_; }
    };

    /** Decoder fallback that throws an exception for undecodable bytes. */
    class DecoderExceptionFallback : public DecoderFallback {
    public:
        /** Always throws std::runtime_error. */
        [[nodiscard]] std::string GetFallbackString(const SharpRuntime::bytecs*, SharpRuntime::intcs) const override {
            throw std::runtime_error("Unable to decode byte sequence: DecoderExceptionFallback.");
        }
        /** Returns 0 (no characters produced; exception is thrown instead). */
        [[nodiscard]] SharpRuntime::intcs getMaxCharCountProperty() const override { return 0; }
    };

    inline std::shared_ptr<DecoderFallback> DecoderFallback::ReplacementFallback() {
        static auto inst = std::make_shared<DecoderReplacementFallback>("?");
        return inst;
    }
    inline std::shared_ptr<DecoderFallback> DecoderFallback::ExceptionFallback() {
        static auto inst = std::make_shared<DecoderExceptionFallback>();
        return inst;
    }

} // namespace System::Text
