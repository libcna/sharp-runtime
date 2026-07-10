// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Console.hpp"

#if defined(_WIN32)
#  include <io.h>
#elif defined(__EMSCRIPTEN__)
// No terminal to redirect-detect; stdin/stdout/stderr are never real TTYs.
#else
#  include <unistd.h>
#endif

namespace System {

    bool Console::getIsInputRedirectedProperty() {
#if defined(_WIN32)
        return !_isatty(_fileno(stdin));
#elif defined(__EMSCRIPTEN__)
        return false;
#else
        return !isatty(fileno(stdin));
#endif
    }

    bool Console::getIsOutputRedirectedProperty() {
#if defined(_WIN32)
        return !_isatty(_fileno(stdout));
#elif defined(__EMSCRIPTEN__)
        return false;
#else
        return !isatty(fileno(stdout));
#endif
    }

    bool Console::getIsErrorRedirectedProperty() {
#if defined(_WIN32)
        return !_isatty(_fileno(stderr));
#elif defined(__EMSCRIPTEN__)
        return false;
#else
        return !isatty(fileno(stderr));
#endif
    }

} // namespace System
