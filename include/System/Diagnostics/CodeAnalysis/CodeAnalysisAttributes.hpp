// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/Attribute.hpp"

namespace System::Diagnostics::CodeAnalysis {

    /** @brief Indicates that the output of a method is never null. */
    class NotNullAttribute : public System::Attribute {};

    /** @brief Indicates that the output of a method may be null even when the non-nullable type is used. */
    class MaybeNullAttribute : public System::Attribute {};

    /** @brief Specifies that null is allowed as an input parameter even when the type does not allow it. */
    class AllowNullAttribute : public System::Attribute {};

    /** @brief Specifies that null is disallowed as an input parameter even when the type allows it. */
    class DisallowNullAttribute : public System::Attribute {};

    /** @brief Specifies that the output is non-null when the named input parameter is non-null. */
    class NotNullIfNotNullAttribute : public System::Attribute {
        std::string parameterName_;
    public:
        /**
         * @brief Constructs the attribute referencing the controlling parameter.
         * @param p Name of the input parameter whose nullability determines the output nullability.
         */
        explicit NotNullIfNotNullAttribute(const std::string& p) : parameterName_(p) {}
        /** @return The name of the controlling input parameter. */
        [[nodiscard]] const std::string& getParameterNameProperty() const { return parameterName_; }
    };

    /** @brief Specifies that an output may be null when the method returns the specified Boolean value. */
    class MaybeNullWhenAttribute : public System::Attribute {
        bool returnValue_;
    public:
        /**
         * @brief Constructs the attribute with the triggering return value.
         * @param rv The Boolean return value that indicates the output may be null.
         */
        explicit MaybeNullWhenAttribute(bool rv) : returnValue_(rv) {}
        /** @return The Boolean return value that triggers the maybe-null postcondition. */
        [[nodiscard]] bool getReturnValueProperty() const { return returnValue_; }
    };

    /** @brief Specifies that an output is not null when the method returns the specified Boolean value. */
    class NotNullWhenAttribute : public System::Attribute {
        bool returnValue_;
    public:
        /**
         * @brief Constructs the attribute with the triggering return value.
         * @param rv The Boolean return value that indicates the output is not null.
         */
        explicit NotNullWhenAttribute(bool rv) : returnValue_(rv) {}
        /** @return The Boolean return value that triggers the not-null postcondition. */
        [[nodiscard]] bool getReturnValueProperty() const { return returnValue_; }
    };

    /** @brief Marks a method that unconditionally throws or loops; callers are unreachable after the call. */
    class DoesNotReturnAttribute : public System::Attribute {};

    /** @brief Marks a method that does not return when the parameter has the specified Boolean value. */
    class DoesNotReturnIfAttribute : public System::Attribute {
        bool parameterValue_;
    public:
        /**
         * @brief Constructs the attribute with the triggering parameter value.
         * @param pv The Boolean parameter value that causes the method not to return.
         */
        explicit DoesNotReturnIfAttribute(bool pv) : parameterValue_(pv) {}
        /** @return The Boolean parameter value that causes the method not to return. */
        [[nodiscard]] bool getParameterValueProperty() const { return parameterValue_; }
    };

    /** @brief Specifies that the named member is non-null when the method returns. */
    class MemberNotNullAttribute : public System::Attribute {
        std::string member_;
    public:
        /**
         * @brief Constructs the attribute for the specified member name.
         * @param m Name of the member that is not null after the method returns.
         */
        explicit MemberNotNullAttribute(const std::string& m) : member_(m) {}
        /** @return The name of the member guaranteed to be non-null on return. */
        [[nodiscard]] const std::string& getMemberProperty() const { return member_; }
    };

    /** @brief Specifies that the named member is non-null when the method returns the specified Boolean value. */
    class MemberNotNullWhenAttribute : public System::Attribute {
        bool returnValue_;
        std::string member_;
    public:
        /**
         * @brief Constructs the attribute with the triggering return value and member name.
         * @param rv The Boolean return value that indicates the member is not null.
         * @param m  Name of the member that is not null when the method returns @p rv.
         */
        MemberNotNullWhenAttribute(bool rv, const std::string& m) : returnValue_(rv), member_(m) {}
        /** @return The Boolean return value that triggers the not-null member postcondition. */
        [[nodiscard]] bool getReturnValueProperty()        const { return returnValue_; }
        /** @return The name of the member guaranteed to be non-null for the specified return value. */
        [[nodiscard]] const std::string& getMemberProperty() const { return member_; }
    };

    /** @brief Indicates that the annotated member requires unreferenced code and may break when trimmed. */
    class RequiresUnreferencedCodeAttribute : public System::Attribute {
        std::string message_;
        std::string url_;
    public:
        /**
         * @brief Constructs the attribute with a diagnostic message and optional URL.
         * @param msg Explanation of why unreferenced code is needed.
         * @param url Optional link to further documentation.
         */
        explicit RequiresUnreferencedCodeAttribute(const std::string& msg, const std::string& url = {})
            : message_(msg), url_(url) {}
        /** @return Explanation of why unreferenced code is needed. */
        [[nodiscard]] const std::string& getMessageProperty() const { return message_; }
        /** @return Optional URL with further documentation; empty if not set. */
        [[nodiscard]] const std::string& getUrlProperty()     const { return url_; }
    };

    /** @brief Indicates that the annotated member requires dynamic code and may break in AOT environments. */
    class RequiresDynamicCodeAttribute : public System::Attribute {
        std::string message_;
        std::string url_;
    public:
        /**
         * @brief Constructs the attribute with a diagnostic message and optional URL.
         * @param msg Explanation of why dynamic code is needed.
         * @param url Optional link to further documentation.
         */
        explicit RequiresDynamicCodeAttribute(const std::string& msg, const std::string& url = {})
            : message_(msg), url_(url) {}
        /** @return Explanation of why dynamic code is needed. */
        [[nodiscard]] const std::string& getMessageProperty() const { return message_; }
        /** @return Optional URL with further documentation; empty if not set. */
        [[nodiscard]] const std::string& getUrlProperty()     const { return url_; }
    };

    /** @brief Excludes the attributed type or member from code coverage analysis. */
    class ExcludeFromCodeCoverageAttribute : public System::Attribute {
        std::string justification_;
    public:
        /** @brief Constructs the attribute with no justification. */
        ExcludeFromCodeCoverageAttribute() = default;
        /**
         * @brief Constructs the attribute with an explanatory justification string.
         * @param j Reason for excluding from code coverage.
         */
        explicit ExcludeFromCodeCoverageAttribute(const std::string& j) : justification_(j) {}
        /** @return The justification for excluding from code coverage; empty if not set. */
        [[nodiscard]] const std::string& getJustificationProperty() const { return justification_; }
    };

    /** @brief Suppresses a specific static-analysis rule violation for the attributed member. */
    class SuppressMessageAttribute : public System::Attribute {
        std::string category_;
        std::string checkId_;
        std::string justification_;
        std::string messageId_;
        std::string scope_;
        std::string target_;
    public:
        /**
         * @brief Constructs the attribute for the given category and check identifier.
         * @param cat Rule category (e.g. "Microsoft.Design").
         * @param id  Check identifier (e.g. "CA1000:DoNotDeclareStaticMembersOnGenericTypes").
         */
        SuppressMessageAttribute(const std::string& cat, const std::string& id)
            : category_(cat), checkId_(id) {}
        /** @return The rule category. */
        [[nodiscard]] const std::string& getCategoryProperty()      const { return category_; }
        /** @return The check identifier. */
        [[nodiscard]] const std::string& getCheckIdProperty()       const { return checkId_; }
        /** @return The suppression justification; empty if not set. */
        [[nodiscard]] const std::string& getJustificationProperty() const { return justification_; }
        /** @brief Sets the suppression justification. */
        void setJustificationProperty(const std::string& v) { justification_ = v; }
        /** @brief Sets the message identifier for filtering specific instances. */
        void setMessageIdProperty(const std::string& v) { messageId_ = v; }
        /** @brief Sets the scope of the suppression (e.g. "member", "type"). */
        void setScopeProperty(const std::string& v)     { scope_ = v; }
        /** @brief Sets the target of the suppression (fully-qualified member name). */
        void setTargetProperty(const std::string& v)    { target_ = v; }
    };

    /**
     * @brief Specifies the syntax kind for a string parameter or return value.
     * 
     * Used by IDE tooling to enable syntax highlighting and validation for string literals.
     */
    class StringSyntaxAttribute : public System::Attribute {
        std::string syntax_;
    public:
        static constexpr const char* CompositeFormat = "CompositeFormat"; ///< .NET composite format string.
        static constexpr const char* DateOnlyFormat   = "DateOnlyFormat"; ///< Date-only format string.
        static constexpr const char* DateTimeFormat   = "DateTimeFormat"; ///< Date and time format string.
        static constexpr const char* EnumFormat       = "EnumFormat";     ///< Enum format string.
        static constexpr const char* GuidFormat       = "GuidFormat";     ///< GUID format string.
        static constexpr const char* Json             = "Json";           ///< JSON syntax.
        static constexpr const char* NumericFormat    = "NumericFormat";  ///< Numeric format string.
        static constexpr const char* Regex            = "Regex";          ///< Regular expression syntax.
        static constexpr const char* TimeOnlyFormat   = "TimeOnlyFormat"; ///< Time-only format string.
        static constexpr const char* TimeSpanFormat   = "TimeSpanFormat"; ///< TimeSpan format string.
        static constexpr const char* Uri              = "Uri";            ///< URI syntax.
        static constexpr const char* Xml              = "Xml";            ///< XML syntax.

        /**
         * @brief Constructs the attribute with the specified syntax kind.
         * @param syntax One of the predefined syntax constants or a custom identifier.
         */
        explicit StringSyntaxAttribute(const std::string& syntax) : syntax_(syntax) {}
        /** @return The syntax kind identifier for this parameter or return value. */
        [[nodiscard]] const std::string& getSyntaxProperty() const { return syntax_; }
    };

} // namespace System::Diagnostics::CodeAnalysis
