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
    // #1983. This used to `return Architecture::X64`, FABRICATING an answer for a target it does
    // not recognise -- so a build for an unsupported architecture compiled cleanly and then
    // reported x64 to every caller, which is the worst of the three possible outcomes.
    //
    // .NET refuses at COMPILE time and nothing else: `ProcessArchitecture`'s chain of
    // `#if TARGET_*` ends in `#error Unknown Architecture` (`RuntimeInformation.cs:49-50`). It
    // has no runtime fallback because there is no correct runtime answer -- the property is a
    // statement about the compilation target, and if the target is unknown the build is what is
    // wrong.
#  error "Unknown architecture: System::Runtime::InteropServices::RuntimeInformation cannot \
report ProcessArchitecture for this compilation target. Add the target to this list rather than \
letting it report a fabricated value (ticket #1983; .NET does the same at RuntimeInformation.cs:49)."
#endif
}

Architecture RuntimeInformation::getOSArchitectureProperty() {
    // Real .NET's OSArchitecture queries the actual OS/kernel architecture (Interop.Sys.
    // GetOSArchitecture(), backed by uname()) rather than the running process' own architecture,
    // falling back to ProcessArchitecture only if that query is unavailable -- this matters for a
    // 32-bit process running under a 64-bit kernel (e.g. via WOW64/multilib), where the two
    // legitimately differ. An earlier version of this port always aliased OSArchitecture to
    // ProcessArchitecture, silently losing that distinction.
#if defined(_WIN32)
    // #1983. This returned getProcessArchitectureProperty() directly, so a 32-bit process on a
    // 64-bit Windows -- WOW64 -- reported X86 as the OPERATING SYSTEM's architecture. .NET's
    // Windows branch is a two-step probe (`RuntimeInformation.Windows.cs:34-75`) and both steps
    // are transcribed here:
    //
    //   1. `IsWow64Process2`, resolved at RUN TIME from kernel32 because it exists only on
    //      Windows 10 and later; its `nativeMachine` out-parameter is an IMAGE_FILE_MACHINE
    //      constant, mapped below. If the call itself fails, .NET falls back to
    //      ProcessArchitecture, and so does this.
    //   2. Otherwise `GetNativeSystemInfo`, whose `wProcessorArchitecture` is mapped through the
    //      PROCESSOR_ARCHITECTURE_* constants -- a different enumeration from step 1, which is
    //      why there are two mapping tables and not one.
    //
    // NOT VERIFIED AT RUNTIME, and that limit is stated rather than implied. This repository's
    // CI is Ubuntu-only and there is no mixed-bitness Windows host here, so what IS checked is
    // that the branch compiles for Windows and that its symbols appear in the Windows object and
    // in no other -- the evidence #2378 established for the same shape. Measured: the Windows
    // object imports GetModuleHandleW, GetProcAddress and GetNativeSystemInfo and contains the
    // "IsWow64Process2" string; the POSIX object imports none of them, contains no such string,
    // and still calls uname.
    //
    // WHAT THAT EVIDENCE CANNOT SHOW, stated because a mutation proved it: changing one of the
    // mapping arms below -- say making the IMAGE_FILE_MACHINE default invent X64 instead of
    // falling back -- alters no symbol and no string, so symbol inspection passes it. The arm's
    // BEHAVIOUR is unverifiable without executing it, which is precisely the third of the three
    // absences #1983 listed and the only one that has not gone away. The mapping tables are
    // transcribed constant by constant for that reason.
    using IsWow64Process2Fn = BOOL(WINAPI*)(HANDLE, USHORT*, USHORT*);
    if (HMODULE kernel32 = ::GetModuleHandleW(L"kernel32.dll")) {
        if (auto isWow64Process2 = reinterpret_cast<IsWow64Process2Fn>(
                reinterpret_cast<void*>(::GetProcAddress(kernel32, "IsWow64Process2")))) {
            USHORT processMachine = 0;
            USHORT nativeMachine = 0;
            if (isWow64Process2(::GetCurrentProcess(), &processMachine, &nativeMachine)) {
                // IMAGE_FILE_MACHINE_* constants, transcribed from
                // RuntimeInformation.Windows.cs:93-113. The default is ProcessArchitecture, as
                // .NET's is -- an unrecognised machine is not a licence to invent one.
                switch (nativeMachine) {
                    case 0x01C4: return Architecture::Arm;    // IMAGE_FILE_MACHINE_ARMNT
                    case 0x8664: return Architecture::X64;    // IMAGE_FILE_MACHINE_AMD64
                    case 0xAA64: return Architecture::Arm64;  // IMAGE_FILE_MACHINE_ARM64
                    case 0x014C: return Architecture::X86;    // IMAGE_FILE_MACHINE_I386
                    default:     return getProcessArchitectureProperty();
                }
            }
            return getProcessArchitectureProperty();
        }
    }
    // PROCESSOR_ARCHITECTURE_* constants, a DIFFERENT enumeration from the machine constants
    // above (RuntimeInformation.Windows.cs:77-91). .NET's default arm is X86, not
    // ProcessArchitecture, and that asymmetry is transcribed rather than tidied.
    SYSTEM_INFO sysInfo{};
    ::GetNativeSystemInfo(&sysInfo);
    switch (sysInfo.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_ARM64: return Architecture::Arm64;
        case PROCESSOR_ARCHITECTURE_ARM:   return Architecture::Arm;
        case PROCESSOR_ARCHITECTURE_AMD64: return Architecture::X64;
        case PROCESSOR_ARCHITECTURE_INTEL:
        default:                           return Architecture::X86;
    }
#elif defined(__EMSCRIPTEN__)
    return getProcessArchitectureProperty();
#else
    struct utsname info{};
    if (::uname(&info) == 0) {
        std::string machine(info.machine);
        if (machine == "x86_64") return Architecture::X64;
        if (machine == "i386" || machine == "i486" || machine == "i586" || machine == "i686") return Architecture::X86;
        if (machine == "aarch64" || machine == "arm64") return Architecture::Arm64;
        if (machine == "armv6l") return Architecture::Armv6;
        if (machine.substr(0, 3) == "arm") return Architecture::Arm;
        if (machine == "riscv64") return Architecture::RiscV64;
        if (machine == "loongarch64") return Architecture::LoongArch64;
        if (machine == "ppc64le") return Architecture::Ppc64le;
        if (machine == "s390x") return Architecture::S390x;
    }
    return getProcessArchitectureProperty();
#endif
}

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
