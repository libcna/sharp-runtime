// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/Runtime/InteropServices/Architecture.hpp"
#include "System/Runtime/InteropServices/OSPlatform.hpp"

namespace System::Runtime::InteropServices {

    /**
     * @brief Provides information about the .NET runtime installation.
     *
     * C++ counterpart of .NET System.Runtime.InteropServices.RuntimeInformation.
     *
     * @note <b>#1980 group G-1 (SR-AUD-153) corrected this note, which was right about one of
     * the two members and wrong about the other.</b> It said both "are not reproduced — there is
     * no .NET runtime/assembly identity in a native C++ build for these to describe".
     *
     * That holds for <b>`FrameworkDescription`</b>, and the reference shows why: it is
     * <i>generated at build time</i> as the literal `".NET {version}"`
     * (`ProductVersionInfoGenerator.cs:55`), i.e. the .NET <i>product</i> version. There is no
     * such product here, so any string this port produced would be an invention.
     *
     * It does <b>not</b> hold for `RuntimeIdentifier`, which is not derived from the platform at
     * all: `RuntimeInformation.cs:20-21` is
     * `AppContext.GetData("RUNTIME_IDENTIFIER") as string ?? "unknown"` — a lookup in a store
     * this port has, with a literal fallback. It is reproduced exactly.
     */
    class RuntimeInformation {
    public:
        RuntimeInformation() = delete;

        /** @return A human-readable description of the operating system. */
        [[nodiscard]] static std::string getOSDescriptionProperty();

        /**
         * @return The platform on which the application is running, or `"unknown"`.
         *
         * `RuntimeInformation.cs:20-21`, transcribed:
         * `AppContext.GetData("RUNTIME_IDENTIFIER") as string ?? "unknown"`.
         *
         * @note The `as string` matters and is reproduced: an entry of some other type falls
         * through to `"unknown"` rather than being coerced, which is the same rule
         * `AppContext`'s own `APP_CONTEXT_BASE_DIRECTORY` lookup follows (#2255).
         */
        [[nodiscard]] static std::string getRuntimeIdentifierProperty();

        /** @return true if the current platform matches @p osPlatform. */
        [[nodiscard]] static bool IsOSPlatform(const OSPlatform& osPlatform);

        /** @return The process' architecture. */
        [[nodiscard]] static Architecture getProcessArchitectureProperty();

        /** @return The operating system's architecture. */
        [[nodiscard]] static Architecture getOSArchitectureProperty();
    };

} // namespace System::Runtime::InteropServices
