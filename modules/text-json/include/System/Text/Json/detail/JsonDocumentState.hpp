// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <atomic>
#include <utility>

#include "nlohmann/json.hpp"

namespace System::Text::Json::detail {

    /**
     * @brief The state a `JsonDocument` and every `JsonElement` handed out from it share.
     *
     * Ticket #2117 (SR-AUD-324, cause TJ-H). Before it, an element captured before
     * `JsonDocument::Dispose()` kept answering — not a dangling read (the element held an owning
     * aliasing `shared_ptr`, so the tree stayed alive) but a **disposed document still serving
     * data**, where .NET throws.
     *
     * @par This is .NET's own structure, not a workaround
     * .NET's `JsonElement` holds a `JsonDocument _parent` and an index, and delegates every
     * accessor to `_parent.GetXxx(_idx)`; each of those begins with `CheckNotDisposed()`
     * (`JsonDocument.cs`, some twenty call sites). So the disposal flag lives with the
     * **document**, and the element reaches it through the reference it already holds. This
     * struct is that reference: elements point at the state rather than at a bare tree node, and
     * carry the node as a raw pointer into it — the direct counterpart of `_parent` plus `_idx`.
     *
     * The flag is `std::atomic` because `Dispose()` and a reader may be on different threads and
     * the previous design gave no guarantee either way; making the answer well-defined costs one
     * relaxed load per access.
     */
    struct JsonDocumentState {
        nlohmann::ordered_json root;
        std::atomic<bool>      disposed{false};

        explicit JsonDocumentState(nlohmann::ordered_json parsed) : root(std::move(parsed)) {}
    };

}  // namespace System::Text::Json::detail
