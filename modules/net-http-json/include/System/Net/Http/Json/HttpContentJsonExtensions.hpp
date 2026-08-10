// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/ArgumentNullException.hpp"
#include "System/Net/Http/HttpContent.hpp"
#include "System/Text/Json/JsonDocument.hpp"
#include "System/Threading/Tasks/Task.hpp"
#include <memory>

namespace System::Net::Http::Json {

    /**
     * @brief Extension-like static methods for reading JSON from HttpContent.
     *
     * C++ counterpart of .NET System.Net.Http.Json.HttpContentJsonExtensions.
     *
     * @note .NET's `ReadFromJsonAsync<T>`/`ReadFromJsonAsync(Type)` deserialize into an arbitrary
     * reflectable type via `JsonSerializer`; this runtime has no reflection and
     * `System::Text::Json::JsonSerializer::Serialize<T>()`/typed-Deserialize are intentional stubs
     * (see their class doc notes). Only the always-available, non-generic form is ported: it
     * parses the content into a `System::Text::Json::JsonDocument` tree (via
     * `JsonSerializer::Deserialize`, which already works), which the caller then navigates via
     * `JsonElement`. A synchronous overload is also provided (.NET does not have one; content
     * reading here is already synchronous, see HttpContent's class doc note), in addition to the
     * async form matching .NET's naming.
     */
    struct HttpContentJsonExtensions {
        HttpContentJsonExtensions() = delete;

        /**
         * @brief Reads @p content and parses it as JSON, synchronously.
         *
         * @param content The content to read from. Must not be an empty `shared_ptr`.
         * @throws System::ArgumentNullException if @p content is empty.
         */
        static std::shared_ptr<System::Text::Json::JsonDocument> ReadFromJson(const std::shared_ptr<HttpContent>& content) {
            // Verified against HttpContentJsonExtensions.cs, whose every public overload begins
            // with ArgumentNullException.ThrowIfNull(content). This port dereferenced the
            // shared_ptr directly, so an empty one was an AddressSanitizer SEGV on address 0x0
            // preceded by a UBSan "member access within null pointer" at this line (ticket
            // #1814 / SR-AUD-236, build-probe/1814_prefix_defects.log case 1).
            if (!content) throw System::ArgumentNullException("content");
            return System::Text::Json::JsonDocument::Parse(content->ReadAsString());
        }

        /**
         * @brief Reads @p content and parses it as JSON, asynchronously.
         *
         * A null @p content is rejected **synchronously, at the call**, before any task is
         * started — it is not reported through the returned task.
         *
         * @param content The content to read from. Must not be an empty `shared_ptr`.
         * @throws System::ArgumentNullException if @p content is empty. Thrown by this call
         *         itself, not stored on the returned task.
         */
        static System::Threading::Tasks::TaskT<std::shared_ptr<System::Text::Json::JsonDocument>>
        ReadFromJsonAsync(const std::shared_ptr<HttpContent>& content) {
            // The placement of this check is the whole point, and it mirrors .NET's own code
            // layout rather than being a defensive duplicate of ReadFromJson's. In
            // HttpContentJsonExtensions.cs the public ReadFromJsonAsync overloads are NOT async
            // methods: each runs ArgumentNullException.ThrowIfNull(content) and only then calls
            // the separate ReadFromJsonAsyncCore, precisely so a null argument surfaces at the
            // call site instead of inside the returned task.
            //
            // Doing it here matters more in this port than it does in .NET. Before this check,
            // the null dereference happened on the std::async worker thread, so
            // build-probe/1814_prefix_defects.log case 3 -- a caller that starts the task and
            // never awaits it -- still killed the process with an ASan SEGV on 0x0 on thread
            // T1. A guard only inside ReadFromJson would have converted that into a stored
            // exception on a task the caller never observes, which is quieter but still wrong.
            if (!content) throw System::ArgumentNullException("content");
            return System::Threading::Tasks::TaskT<std::shared_ptr<System::Text::Json::JsonDocument>>::Run(
                [content]() { return ReadFromJson(content); });
        }
    };

} // namespace System::Net::Http::Json
