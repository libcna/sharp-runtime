// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/Xml/Linq/XNode.hpp"

namespace System::Xml::Linq {

    /**
     * @brief Represents an XML processing instruction.
     *
     * C++ counterpart of .NET System.Xml.Linq.XProcessingInstruction.
     *
     * @note .NET validates Target as a well-formed NCName (and rejects the reserved "xml"
     * target, case-insensitively) via XmlConvert.VerifyNCName **at construction**. This port
     * deliberately still does not validate at construction or in setTargetProperty() — out of
     * scope, consistent with this runtime's general lack of strict XML name validation elsewhere
     * (e.g. XName) — but **both serialization doors now reject a malformed target**
     * (`System::Xml::XmlConvert::VerifyName`, throwing `System::Xml::XmlException`). `WriteTo()`
     * has always done so; `ToString()`/`Save()` gained it with ticket #2196 (SR-AUD-335), where
     * the two doors were measured to disagree completely: `XProcessingInstruction("a?>b", "d")`
     * emitted `<?a?>b d?>` through the direct door while the writer door threw.
     *
     * @note Data containing `?>` is **repaired by inserting a space** (`<?p left? >right?>`),
     * matching real .NET's `XmlEncodedRawTextWriter.WriteCommentOrPi`. Before #2196 the direct
     * door emitted the raw `?>`, which closed the instruction early and made the resulting text
     * unparseable — this node kind is the one whose corruption was not silent.
     *
     * @note **This runtime cannot parse back a processing instruction placed anywhere except
     * before every other node.** Serialization is correct in every position and the text is
     * well-formed XML, but `XDocument::Parse` accepts a PI only at the very start of the
     * document — an XML declaration may precede it, and nothing else may, not even a comment.
     * `<root><?p d?></root>`, `<root/><?p d?>` and `<!--c--><?p d?><root/>` all throw.
     *
     * The cause is the vendored substrate's node-type model rather than anything in this port:
     * `vendor/tinyxml2` has no processing-instruction type, so every `<?` becomes an XML
     * *declaration* and inherits the rule that a declaration may appear only at document level
     * and before anything else. `vendor/` is third-party source and is never edited.
     *
     * **.NET has no such limitation** — its loader handles `XmlNodeType.XmlDeclaration` and
     * `XmlNodeType.ProcessingInstruction` as separate cases in the same general node loop
     * (`XmlLoader.cs:203-209`), which runs for element content too.
     *
     * Ticket **#2202**; pinned by
     * `XLinqLexicalSerializationTests.ProcessingInstruction_ParserPositionLimitIsSubstrateNotSerialization`
     * and, at the `System::Xml` layer, by `XmlWriterValidationTests.Decl2202_*`.
     */
    class XProcessingInstruction : public XNode {
        std::string target_;
        std::string data_;

    public:
        /** @brief Initializes a new processing instruction with the given target and data. */
        XProcessingInstruction(const std::string& target, const std::string& data)
            : target_(target), data_(data) {}

        [[nodiscard]] System::Xml::XmlNodeType getNodeTypeProperty() const override { return System::Xml::XmlNodeType::ProcessingInstruction; }

        /** @return The target application for this processing instruction. */
        [[nodiscard]] const std::string& getTargetProperty() const { return target_; }
        /** @brief Sets the target application. */
        /** @brief Sets the target, raising a **Name** pair (#2199) -- .NET uses Name, not Value,
         *  for this member (XProcessingInstruction.cs:108-110). */
        void setTargetProperty(const std::string& target) {
            const bool notify = NotifyChanging(this, XObjectChangeEventArgs::Name);
            target_ = target;
            if (notify) NotifyChanged(this, XObjectChangeEventArgs::Name);
        }

        /** @return The content of this processing instruction. */
        [[nodiscard]] const std::string& getDataProperty() const { return data_; }
        /** @brief Sets the content. */
        /** @brief Sets the data, raising a **Value** pair (#2199)
         *  (XProcessingInstruction.cs:73-75). */
        void setDataProperty(const std::string& data) {
            const bool notify = NotifyChanging(this, XObjectChangeEventArgs::Value);
            data_ = data;
            if (notify) NotifyChanged(this, XObjectChangeEventArgs::Value);
        }

        void WriteTo(System::Xml::XmlWriter& writer) const override;
        [[nodiscard]] SharpRuntime::intcs GetDeepHashCode() const override {
            return static_cast<SharpRuntime::intcs>(std::hash<std::string>{}(target_)) ^
                   static_cast<SharpRuntime::intcs>(std::hash<std::string>{}(data_));
        }

    protected:
        void SerializeTo(std::ostream& os, int depth, bool indent) const override;
        [[nodiscard]] bool DeepEqualsCore(const XNode& other) const override {
            const auto& o = static_cast<const XProcessingInstruction&>(other);
            return target_ == o.target_ && data_ == o.data_;
        }
    };

} // namespace System::Xml::Linq
