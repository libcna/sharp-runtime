// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/AppDomain.hpp"
#include <climits>
#include <unistd.h>

namespace System {

AppDomain::AppDomain() {
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        std::string path(buf);
        auto pos = path.rfind('/');
        baseDirectory_ = (pos != std::string::npos) ? path.substr(0, pos + 1) : "./";
    } else {
        baseDirectory_ = "./";
    }
}

} // namespace System
