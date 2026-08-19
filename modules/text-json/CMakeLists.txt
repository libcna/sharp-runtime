# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

sharp_runtime_register_module(
    NAME Text.Json
    TARGET sharp_runtime_text_json
    TYPE STATIC
    # Collections.Core became a PUBLIC edge with ticket #1889: JsonArray and JsonObject hold a
    # System::Collections::detail::MutationCounter in their PUBLIC headers, because CLAUDE.md's
    # collection invariant requires exactly that type for a fail-fast enumerator and forbids a
    # local copy. The module-boundary validator rejected the private declaration, which is what
    # made the edge explicit rather than accidental. Same shape as #1814's Net.Http.Json edge.
    PUBLIC_DEPENDENCIES Core.Base Text Collections.Core
    TEST_DEPENDENCIES Collections.Core
)
