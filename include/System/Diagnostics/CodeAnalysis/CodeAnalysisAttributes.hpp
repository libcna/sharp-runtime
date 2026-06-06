// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/Attribute.hpp"

namespace System::Diagnostics::CodeAnalysis {

    class NotNullAttribute : public System::Attribute {};
    class MaybeNullAttribute : public System::Attribute {};
    class AllowNullAttribute : public System::Attribute {};
    class DisallowNullAttribute : public System::Attribute {};
    class NotNullIfNotNullAttribute : public System::Attribute {
        std::string parameterName_;
    public:
        explicit NotNullIfNotNullAttribute(const std::string& p) : parameterName_(p) {}
        [[nodiscard]] const std::string& getParameterNameProperty() const { return parameterName_; }
    };
    class MaybeNullWhenAttribute : public System::Attribute {
        bool returnValue_;
    public:
        explicit MaybeNullWhenAttribute(bool rv) : returnValue_(rv) {}
        [[nodiscard]] bool getReturnValueProperty() const { return returnValue_; }
    };
    class NotNullWhenAttribute : public System::Attribute {
        bool returnValue_;
    public:
        explicit NotNullWhenAttribute(bool rv) : returnValue_(rv) {}
        [[nodiscard]] bool getReturnValueProperty() const { return returnValue_; }
    };
    class DoesNotReturnAttribute : public System::Attribute {};
    class DoesNotReturnIfAttribute : public System::Attribute {
        bool parameterValue_;
    public:
        explicit DoesNotReturnIfAttribute(bool pv) : parameterValue_(pv) {}
        [[nodiscard]] bool getParameterValueProperty() const { return parameterValue_; }
    };
    class MemberNotNullAttribute : public System::Attribute {
        std::string member_;
    public:
        explicit MemberNotNullAttribute(const std::string& m) : member_(m) {}
        [[nodiscard]] const std::string& getMemberProperty() const { return member_; }
    };
    class MemberNotNullWhenAttribute : public System::Attribute {
        bool returnValue_;
        std::string member_;
    public:
        MemberNotNullWhenAttribute(bool rv, const std::string& m) : returnValue_(rv), member_(m) {}
        [[nodiscard]] bool getReturnValueProperty()        const { return returnValue_; }
        [[nodiscard]] const std::string& getMemberProperty() const { return member_; }
    };
    class RequiresUnreferencedCodeAttribute : public System::Attribute {
        std::string message_;
        std::string url_;
    public:
        explicit RequiresUnreferencedCodeAttribute(const std::string& msg, const std::string& url = {})
            : message_(msg), url_(url) {}
        [[nodiscard]] const std::string& getMessageProperty() const { return message_; }
        [[nodiscard]] const std::string& getUrlProperty()     const { return url_; }
    };
    class RequiresDynamicCodeAttribute : public System::Attribute {
        std::string message_;
        std::string url_;
    public:
        explicit RequiresDynamicCodeAttribute(const std::string& msg, const std::string& url = {})
            : message_(msg), url_(url) {}
        [[nodiscard]] const std::string& getMessageProperty() const { return message_; }
        [[nodiscard]] const std::string& getUrlProperty()     const { return url_; }
    };
    class ExcludeFromCodeCoverageAttribute : public System::Attribute {
        std::string justification_;
    public:
        ExcludeFromCodeCoverageAttribute() = default;
        explicit ExcludeFromCodeCoverageAttribute(const std::string& j) : justification_(j) {}
        [[nodiscard]] const std::string& getJustificationProperty() const { return justification_; }
    };
    class SuppressMessageAttribute : public System::Attribute {
        std::string category_;
        std::string checkId_;
        std::string justification_;
        std::string messageId_;
        std::string scope_;
        std::string target_;
    public:
        SuppressMessageAttribute(const std::string& cat, const std::string& id)
            : category_(cat), checkId_(id) {}
        [[nodiscard]] const std::string& getCategoryProperty()      const { return category_; }
        [[nodiscard]] const std::string& getCheckIdProperty()       const { return checkId_; }
        [[nodiscard]] const std::string& getJustificationProperty() const { return justification_; }
        void setJustificationProperty(const std::string& v) { justification_ = v; }
        void setMessageIdProperty(const std::string& v) { messageId_ = v; }
        void setScopeProperty(const std::string& v)     { scope_ = v; }
        void setTargetProperty(const std::string& v)    { target_ = v; }
    };
    class StringSyntaxAttribute : public System::Attribute {
        std::string syntax_;
    public:
        static constexpr const char* CompositeFormat = "CompositeFormat";
        static constexpr const char* DateOnlyFormat   = "DateOnlyFormat";
        static constexpr const char* DateTimeFormat   = "DateTimeFormat";
        static constexpr const char* EnumFormat       = "EnumFormat";
        static constexpr const char* GuidFormat       = "GuidFormat";
        static constexpr const char* Json             = "Json";
        static constexpr const char* NumericFormat    = "NumericFormat";
        static constexpr const char* Regex            = "Regex";
        static constexpr const char* TimeOnlyFormat   = "TimeOnlyFormat";
        static constexpr const char* TimeSpanFormat   = "TimeSpanFormat";
        static constexpr const char* Uri              = "Uri";
        static constexpr const char* Xml              = "Xml";

        explicit StringSyntaxAttribute(const std::string& syntax) : syntax_(syntax) {}
        [[nodiscard]] const std::string& getSyntaxProperty() const { return syntax_; }
    };

} // namespace System::Diagnostics::CodeAnalysis
