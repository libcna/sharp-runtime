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

    class EncoderFallback {
    public:
        virtual ~EncoderFallback() = default;
        [[nodiscard]] virtual std::vector<SharpRuntime::bytecs> GetFallbackBytes(char unknownChar) const = 0;
        [[nodiscard]] virtual SharpRuntime::intcs getMaxByteCountProperty() const = 0;

        static std::shared_ptr<EncoderFallback> ReplacementFallback();
        static std::shared_ptr<EncoderFallback> ExceptionFallback();
    };

    class EncoderReplacementFallback : public EncoderFallback {
        std::string replacement_;
    public:
        explicit EncoderReplacementFallback(const std::string& replacement = "?") : replacement_(replacement) {}
        [[nodiscard]] std::vector<SharpRuntime::bytecs> GetFallbackBytes(char) const override {
            return std::vector<SharpRuntime::bytecs>(replacement_.begin(), replacement_.end());
        }
        [[nodiscard]] SharpRuntime::intcs getMaxByteCountProperty() const override {
            return static_cast<SharpRuntime::intcs>(replacement_.size());
        }
        [[nodiscard]] const std::string& getDefaultStringProperty() const { return replacement_; }
    };

    class EncoderExceptionFallback : public EncoderFallback {
    public:
        [[nodiscard]] std::vector<SharpRuntime::bytecs> GetFallbackBytes(char) const override {
            throw std::runtime_error("Unable to encode character: EncoderExceptionFallback.");
        }
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
