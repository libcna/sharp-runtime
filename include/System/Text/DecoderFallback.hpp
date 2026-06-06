// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <stdexcept>
#include <string>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Text {

    class DecoderFallback {
    public:
        virtual ~DecoderFallback() = default;
        [[nodiscard]] virtual std::string GetFallbackString(const SharpRuntime::bytecs* bytesUnknown, SharpRuntime::intcs byteCount) const = 0;
        [[nodiscard]] virtual SharpRuntime::intcs getMaxCharCountProperty() const = 0;

        static std::shared_ptr<DecoderFallback> ReplacementFallback();
        static std::shared_ptr<DecoderFallback> ExceptionFallback();
    };

    class DecoderReplacementFallback : public DecoderFallback {
        std::string replacement_;
    public:
        explicit DecoderReplacementFallback(const std::string& replacement = "?") : replacement_(replacement) {}
        [[nodiscard]] std::string GetFallbackString(const SharpRuntime::bytecs*, SharpRuntime::intcs) const override { return replacement_; }
        [[nodiscard]] SharpRuntime::intcs getMaxCharCountProperty() const override { return static_cast<SharpRuntime::intcs>(replacement_.size()); }
        [[nodiscard]] const std::string& getDefaultStringProperty() const { return replacement_; }
    };

    class DecoderExceptionFallback : public DecoderFallback {
    public:
        [[nodiscard]] std::string GetFallbackString(const SharpRuntime::bytecs*, SharpRuntime::intcs) const override {
            throw std::runtime_error("Unable to decode byte sequence: DecoderExceptionFallback.");
        }
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
