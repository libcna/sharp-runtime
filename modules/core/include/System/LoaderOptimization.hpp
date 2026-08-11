// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System {

    /**
     * @brief Specifies how the loader optimizes the loading of an executable.
     *
     * C++ counterpart of .NET System.LoaderOptimization.
     * Note: multi-domain and domain-mask values are legacy; modern .NET uses a
     * single AppDomain.
     *
     * @warning **The `@deprecated` tags below are prose, not a compiler
     * diagnostic.** .NET marks `DomainMask` and `DisallowBindings` with
     * `Obsolete`, so naming either in C# produces a compiler warning; naming
     * either here compiles silently, because the declarations carry a Doxygen
     * tag rather than C++ `[[deprecated]]`. That is the whole of SR-AUD-117, and
     * it is **not** repaired by this note: adding `[[deprecated]]` turns every
     * use into a hard error under this repository's own `-Wall -Wextra -Werror`
     * (measured), so it can break a consumer build and needs a decision this
     * repository has not taken — see ticket #2289, which also covers the three
     * other prose-only deprecations in this port
     * (`AppDomain::GetCurrentThreadId`, `CultureTypes::WindowsOnlyCultures`,
     * `CultureTypes::FrameworkCultures`). Until then, treat the tags as advice
     * your compiler will not repeat.
     */
    enum class LoaderOptimization {
        /** @brief No optimization is specified. */
        NotSpecified    = 0,
        /** @brief The application will probably have a single domain. */
        SingleDomain    = 1,
        /** @brief The application will probably have multiple domains using the same code. */
        MultiDomain     = 2,
        /**
         * @brief The application will probably have multiple domains using different code.
         *
         * C++ counterpart of .NET LoaderOptimization.MultiDomainHost.
         */
        MultiDomainHost = 3,
        /**
         * @brief Deprecated alias for MultiDomainHost.
         *
         * C++ counterpart of .NET LoaderOptimization.DomainMask (deprecated).
         * @deprecated Use MultiDomainHost instead.
         */
        DomainMask      = 3,
        /**
         * @brief Disallow binding redirects.
         *
         * C++ counterpart of .NET LoaderOptimization.DisallowBindings (deprecated).
         * @deprecated This value is not supported.
         */
        DisallowBindings = 4,
    };

} // namespace System
