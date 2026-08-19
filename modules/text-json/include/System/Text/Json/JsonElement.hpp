// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <string>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Text/Json/JsonValueKind.hpp"
#include "nlohmann/json.hpp"
#include "System/ObjectDisposedException.hpp"
#include "System/Text/Json/detail/JsonDocumentState.hpp"

namespace System::Text::Json {

    using SharpRuntime::intcs;
    using SharpRuntime::longcs;

    class JsonProperty;

    /**
     * @brief Represents a single JSON value (object, array, string, number, boolean, or null).
     *
     * C++ counterpart of .NET System.Text.Json.JsonElement.
     *
     * @note Backed directly by a node in the owning JsonDocument's parsed `nlohmann::ordered_json` tree
     * (shared ownership via aliasing `shared_ptr`, so a JsonElement keeps the whole document tree
     * alive) rather than reproducing .NET's compact binary token database over a raw UTF-8 buffer
     * — same observable API, simpler implementation.
     */
    class JsonElement {
        // #2117: the element points at the document's shared STATE and carries the node as a raw
        // pointer into it, which is .NET's `_parent` plus `_idx`. It used to hold an aliasing
        // `shared_ptr` straight to the node, which kept the tree alive -- correct for lifetime,
        // and with no route back to the document to ask whether it had been disposed.
        //
        // sizeof(JsonElement) 48 -> 56 under SA-3; pinned by JsonLayoutPinTests.
        std::shared_ptr<detail::JsonDocumentState> state_;
        const nlohmann::ordered_json*              node_ = nullptr;
        std::string propertyName_; // set only for elements obtained via EnumerateObject(); see JsonProperty

        friend class JsonProperty;
        friend class JsonDocument;

        [[nodiscard]] const nlohmann::ordered_json& require(JsonValueKind expected, const char* what) const;

        /**
         * @brief The node, after checking the owning document has not been disposed.
         *
         * The single choke point .NET spreads over some twenty `CheckNotDisposed()` calls in
         * `JsonDocument`. Every accessor goes through it, so an element handed out before
         * `Dispose()` reports the disposal rather than serving data.
         *
         * A **default** element has no state and is NOT disposed -- it is undefined, and .NET
         * distinguishes the two: `CheckValidInstance()` raises `InvalidOperationException` for a
         * null parent while `CheckNotDisposed()` raises `ObjectDisposedException`. Callers here
         * keep their existing "undefined" behaviour, so this returns nullptr for that case and
         * each accessor answers as it always did.
         */
        [[nodiscard]] const nlohmann::ordered_json* checkedNode() const {
            if (state_ && state_->disposed.load(std::memory_order_relaxed))
                throw System::ObjectDisposedException("JsonDocument");
            return node_;
        }

    public:
        /** @brief Constructs an undefined JsonElement. */
        JsonElement() = default;
        /** @brief Wraps a node in a JsonDocument's shared state (internal; use JsonDocument::getRootElementProperty()/GetProperty()/etc. instead). */
        JsonElement(std::shared_ptr<detail::JsonDocumentState> state, const nlohmann::ordered_json* node)
            : state_(std::move(state)), node_(node) {}

        /**
         * @return The kind of this JSON value.
         * @throws System::ObjectDisposedException if the owning document has been disposed
         *         (#2117). .NET throws here too: `ValueKind` reads `_parent.GetJsonTokenType`,
         *         which begins with `CheckNotDisposed()`.
         */
        [[nodiscard]] JsonValueKind getValueKindProperty() const {
            if (!checkedNode()) return JsonValueKind::Undefined;
            if (node_->is_object()) return JsonValueKind::Object;
            if (node_->is_array()) return JsonValueKind::Array;
            if (node_->is_string()) return JsonValueKind::String;
            if (node_->is_number()) return JsonValueKind::Number;
            if (node_->is_boolean()) return node_->get<bool>() ? JsonValueKind::True : JsonValueKind::False;
            if (node_->is_null()) return JsonValueKind::Null;
            return JsonValueKind::Undefined;
        }

        /**
         * @return The value as a string, or "" if this element's ValueKind is Null.
         * @throws System::InvalidOperationException if this element is any other non-string kind.
         * @note .NET's GetString() returns `string?` and special-cases Null to return null
         * before the type check (JsonDocument.cs's GetString); this port's GetString() keeps
         * the non-nullable std::string signature every other caller already relies on, so Null
         * maps to "" instead. Check getValueKindProperty() == JsonValueKind::Null first if the
         * null/empty-string distinction matters.
         */
        [[nodiscard]] std::string GetString() const {
            if (checkedNode() && node_->is_null()) return {};
            return require(JsonValueKind::String, "String").get<std::string>();
        }

        /** @return The value as a 32-bit integer. @throws System::InvalidOperationException/System::FormatException. */
        [[nodiscard]] intcs GetInt32() const;
        /** @return The value as a 64-bit integer. @throws System::InvalidOperationException/System::FormatException. */
        [[nodiscard]] longcs GetInt64() const;
        /** @return The value as a double. @throws System::InvalidOperationException if this element is not a JSON number. */
        [[nodiscard]] double GetDouble() const { return require(JsonValueKind::Number, "Number").get<double>(); }
        /** @return The value as a boolean (true if ValueKind is True, false if False). @throws System::InvalidOperationException otherwise. */
        [[nodiscard]] bool GetBoolean() const;

        /**
         * @brief Tries to get this element's value as a 32-bit integer.
         * @return false (without throwing) if this is a Number whose value doesn't fit an
         * intcs, or isn't an integer literal (e.g. has a decimal point/exponent).
         * @throws System::InvalidOperationException if this element's ValueKind isn't Number.
         */
        [[nodiscard]] bool TryGetInt32(intcs& value) const;
        /**
         * @brief Tries to get this element's value as a 64-bit integer.
         * @return false (without throwing) if this is a Number whose value doesn't fit a
         * longcs, or isn't an integer literal (e.g. has a decimal point/exponent).
         * @throws System::InvalidOperationException if this element's ValueKind isn't Number.
         */
        [[nodiscard]] bool TryGetInt64(longcs& value) const;
        /** @brief Tries to get this element's value as a double without throwing on failure. */
        [[nodiscard]] bool TryGetDouble(double& value) const {
            if (!checkedNode() || !node_->is_number()) { value = 0; return false; }
            value = node_->get<double>();
            return true;
        }

        /**
         * @return This element **re-rendered** as JSON text — see the declared limitation below.
         *
         * @note **DECLARED LIMITATION (SR-AUD-325, cause TJ-D, ticket #2118, decided 2026-08-19).**
         * This member does **not** return the source text and, on this substrate, cannot.
         *
         * .NET's contract is unambiguous and is the opposite: `JsonElement.GetRawText()` is
         * `_parent.GetRawValueAsString(_idx)` (`JsonElement.cs:1196-1201`), which slices the
         * **original document bytes** and transcodes them (`JsonDocument.cs:700-704`). Nothing is
         * re-rendered there.
         *
         * This port holds a *parsed* `nlohmann` tree. `nlohmann`'s DOM **retains no source spans
         * at all**, so `dump()` can only re-render from the parsed value. Measured differences,
         * pinned by `JsonGatedBehaviourPins.Decl2118_GetRawTextReRendersRatherThanReturningSourceText`:
         *
         * | source | .NET returns | this port returns |
         * |---|---|---|
         * | `1e+01` | `1e+01` | `10.0` |
         * | `1.10` | `1.10` | `1.1` |
         * | `"\u0061"` | `"\u0061"` | `"a"` |
         * | `{ "a" : 1 }` | `{ "a" : 1 }` | `{"a":1}` |
         *
         * **Why this is declared rather than repaired.** Honouring the contract means `JsonDocument`
         * retaining the original text **and every `JsonElement` carrying an offset and length into
         * it** — an object-layout change to both types, and a parse-time and memory cost paid by
         * **every** caller whether or not `GetRawText` is ever called. That was offered and
         * **declined** on 2026-08-19 (`docs/StandingApprovals.md` SA-13); #2117 had already grown
         * `JsonElement` 48 → 56 in the same session, and this would have grown it again to buy a
         * member most callers never touch.
         *
         * @note The value returned is always **valid JSON that parses back to an equal element**.
         * What is lost is the *representation*, never the *value*. A document already in the
         * renderer's canonical form round-trips byte for byte.
         *
         * @note `ToString()` is the same door and must not diverge from it; that is asserted.
         */
        [[nodiscard]] std::string GetRawText() const { return checkedNode() ? node_->dump() : std::string(); }

        /**
         * @brief Tries to get a named object property.
         * @return false (without throwing) if this is an Object with no such property.
         * @throws System::InvalidOperationException if this element's ValueKind isn't Object
         * (verified against JsonDocument.TryGetProperty.cs's TryGetNamedPropertyValue, which
         * checks the token type unconditionally before searching for the property, even for
         * the Try-prefixed overload).
         */
        [[nodiscard]] bool TryGetProperty(const std::string& name, JsonElement& out) const {
            const auto& n = require(JsonValueKind::Object, "Object");
            auto it = n.find(name);
            if (it == n.end()) return false;
            out = JsonElement(state_, &(*it));
            return true;
        }

        /**
         * @return A named object property.
         * @throws System::InvalidOperationException if this element's ValueKind isn't Object.
         * @throws System::Collections::Generic::KeyNotFoundException if no such property exists.
         */
        [[nodiscard]] JsonElement GetProperty(const std::string& name) const;

        /**
         * @return true if this element is an object containing a property named @p name.
         * @throws System::InvalidOperationException if this element's ValueKind isn't Object.
         */
        [[nodiscard]] bool HasProperty(const std::string& name) const {
            JsonElement ignored;
            return TryGetProperty(name, ignored);
        }

        /** @return The number of elements in this JSON array. @throws System::InvalidOperationException if not an array. */
        [[nodiscard]] intcs GetArrayLength() const {
            return static_cast<intcs>(require(JsonValueKind::Array, "Array").size());
        }

        /** @return The array element at @p index. @throws System::InvalidOperationException/System::IndexOutOfRangeException. */
        [[nodiscard]] JsonElement operator[](intcs index) const;

        /** @return The items of this JSON array. @throws System::InvalidOperationException if not an array. */
        [[nodiscard]] std::vector<JsonElement> EnumerateArray() const {
            const auto& arr = require(JsonValueKind::Array, "Array");
            std::vector<JsonElement> result;
            result.reserve(arr.size());
            for (const auto& item : arr) result.emplace_back(state_, &item);
            return result;
        }

        /** @return The properties of this JSON object, in document order. @throws System::InvalidOperationException if not an object. */
        [[nodiscard]] std::vector<JsonProperty> EnumerateObject() const;

        /** @return A deep copy of this element that owns its own storage. */
        /**
         * @return A deep copy of this element that owns its own storage.
         *
         * The copy has its own state, so it survives the original document's `Dispose()` — which
         * is .NET's contract too: `Clone()` delegates to `_parent.CloneElement(_idx)`, producing
         * an element rooted in a NEW document (`JsonElement.cs`). Cloning an element whose
         * document is already disposed throws, because the check runs first.
         */
        [[nodiscard]] JsonElement Clone() const {
            if (!checkedNode()) return JsonElement();
            auto fresh = std::make_shared<detail::JsonDocumentState>(*node_);
            return JsonElement(fresh, &fresh->root);
        }

        /** @return The raw JSON text of this element (same as GetRawText()). */
        [[nodiscard]] std::string ToString() const { return GetRawText(); }
    };

} // namespace System::Text::Json
