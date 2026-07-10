// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Runtime/InteropServices/RuntimeInformation.hpp"

#if defined(_WIN32)
#include <windows.h>
#elif defined(__EMSCRIPTEN__)
// No uname() under Emscripten.
#else
#include <sys/utsname.h>
#endif

namespace System::Runtime::InteropServices {

Architecture RuntimeInformation::getProcessArchitectureProperty() {
#if defined(__x86_64__) || defined(_M_X64)
    return Architecture::X64;
#elif defined(__i386__) || defined(_M_IX86)
    return Architecture::X86;
#elif defined(__aarch64__) || defined(_M_ARM64)
    return Architecture::Arm64;
#elif defined(__arm__) || defined(_M_ARM)
    return Architecture::Arm;
#elif defined(__wasm__)
    return Architecture::Wasm;
#elif defined(__loongarch64)
    return Architecture::LoongArch64;
#elif defined(__powerpc64__)
    return Architecture::Ppc64le;
#elif defined(__riscv) && __riscv_xlen == 64
    return Architecture::RiscV64;
#else
    return Architecture::X64;
#endif
}

Architecture RuntimeInformation::getOSArchitectureProperty() { return getProcessArchitectureProperty(); }

bool RuntimeInformation::IsOSPlatform(const OSPlatform& osPlatform) {
#if defined(_WIN32)
    return osPlatform == OSPlatform::Windows;
#elif defined(__APPLE__)
    return osPlatform == OSPlatform::OSX;
#elif defined(__FreeBSD__)
    return osPlatform == OSPlatform::FreeBSD;
#elif defined(__linux__)
    return osPlatform == OSPlatform::Linux;
#else
    (void)osPlatform;
    return false;
#endif
}

std::string RuntimeInformation::getOSDescriptionProperty() {
#if defined(_WIN32)
    return "Microsoft Windows";
#elif defined(__EMSCRIPTEN__)
    return "Emscripten";
#else
    struct utsname info{};
    if (::uname(&info) == 0) {
        return std::string(info.sysname) + " " + info.release;
    }
    return "Unknown";
#endif
}

} // namespace System::Runtime::InteropServices
