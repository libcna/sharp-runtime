// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Attribute.hpp"

namespace System {
    /**
     * @brief Marks the .NET concept "the value of a static field is unique for
     * each thread". Carries no data and has **no effect in the C++ port**.
     *
     * C++ counterpart of .NET System.ThreadStaticAttribute.
     *
     * @warning **This class cannot give any field per-thread storage, and
     * nothing reads it.** In .NET the attribute is metadata attached to a static
     * field, and the runtime supplies separate storage per thread on the
     * strength of that attachment. C++ has no mechanism by which an object of a
     * class could attach to a declaration: constructing one, storing one,
     * deriving from one or naming it in a comment changes nothing about any
     * variable's storage duration, and no code in this repository inspects it.
     * The same is true of `STAThreadAttribute` and `MTAThreadAttribute`, which
     * say so; this header used to promise the per-thread behaviour instead,
     * which is the whole of SR-AUD-113.
     *
     * The C++ facility that does what the .NET attribute describes is the
     * `thread_local` storage-duration specifier, written on the declaration
     * itself:
     *
     * @code
     * // .NET:  [ThreadStatic] private static int counter;
     * static thread_local int counter;
     * @endcode
     *
     * Ported code that needs per-thread state must write that. This class exists
     * so that ported code naming `System::ThreadStaticAttribute` still compiles,
     * and so that the .NET intent stays visible next to the declaration it was
     * attached to — it is documentation for a human reader, not a mechanism.
     *
     * It is instantiable rather than abstract for the reason `Attribute` itself
     * records: the base's constructor is public so attributes can be built in
     * tests. It has no first-party production consumer.
     *
     * SR-AUD-113, tickets #2287 (review) and #2288.
     */
    class ThreadStaticAttribute : public Attribute {};
} // namespace System
