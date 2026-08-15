// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Environment.hpp"
#include "System/IO/DirectoryNotFoundException.hpp"

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <psapi.h>
#  include <shlobj.h>
#  undef GetCurrentDirectory   // windows.h macro collides with our method name
#  undef GetEnvironmentVariable // windows.h macro collides with our method name
#  undef SetCurrentDirectory   // windows.h macro collides with our method name
#  undef SetEnvironmentVariable // windows.h macro collides with our method name
#elif defined(__EMSCRIPTEN__)
#  include <unistd.h>
#  include <climits>
#else
#  include <unistd.h>
#  include <climits>
#  include <pwd.h>
#  include <time.h>
#  include <sys/resource.h>
#  include <sys/utsname.h>
#  include <cstdio>
#endif
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <sstream>
#include <vector>

// HOST_NAME_MAX is a Linux/glibc extension (POSIX.1-2001 mentions it but doesn't mandate it) --
// Apple's libc headers never define it at all (confirmed via a real macOS CI build: "use of
// undeclared identifier 'HOST_NAME_MAX'"), even though gethostname() itself is fully POSIX and
// available everywhere. 255 is the POSIX-guaranteed minimum (_POSIX_HOST_NAME_MAX), a safe,
// portable buffer size fallback for any platform (macOS, other BSDs, Emscripten) that omits the
// Linux-specific constant.
#if !defined(_WIN32) && !defined(HOST_NAME_MAX)
#  define HOST_NAME_MAX 255
#endif

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
extern char** environ; // POSIX — global process environment array
#endif

namespace System {

namespace {
    // Splits one raw "NAME=value" environment-block entry on its first '='. Returns false (skip
    // this entry) for a malformed block entry with no '=' at all, or one where '=' is the very
    // first character (an empty name) -- matching real .NET's ParseEntry, which explicitly skips
    // "entries starting with '=' and entries with no value" when building the environment
    // dictionary (Environment.Variables.Unix.cs). The previous version of the POSIX loop below
    // only checked for "no '=' at all", not "'=' is the first character", so a corrupted/synthetic
    // environment block entry like "=X" would have produced an empty-string key -- inconsistent
    // with the Windows loop just below it, which already had the `eq > 0` guard.
    bool splitEnvEntry(const std::string& entry, std::string& key, std::string& value) {
        auto eq = entry.find('=');
        if (eq == std::string::npos || eq == 0) return false;
        key = entry.substr(0, eq);
        value = entry.substr(eq + 1);
        return true;
    }

    bool tryGetEnvironmentVariable(
            const std::string& name, std::string& value) {
        // getenv("") is unspecified by POSIX. Real .NET returns null for an empty name, which
        // this runtime represents as an unsuccessful lookup and an empty public return value.
        if (name.empty()) return false;
#if defined(_WIN32)
        char* rawValue = nullptr;
        std::size_t valueLength = 0;
        if (_dupenv_s(&rawValue, &valueLength, name.c_str()) != 0 || rawValue == nullptr) {
            std::free(rawValue);
            return false;
        }
        value.assign(rawValue);
        std::free(rawValue);
        return true;
#else
        const char* rawValue = std::getenv(name.c_str());
        if (rawValue == nullptr) return false;
        value.assign(rawValue);
        return true;
#endif
    }
}

// ---------------------------------------------------------------------------
// OSVersion
// ---------------------------------------------------------------------------

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
static int parseNextNumber(const std::string& s, std::size_t& pos) {
    // skip to next digit
    while (pos < s.size() && (s[pos] < '0' || s[pos] > '9')) ++pos;
    int num = 0;
    while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') {
        int d = s[pos++] - '0';
        if (num <= (2147483647 - d) / 10) num = num * 10 + d;
        else return 2147483647; // overflow guard
    }
    return num;
}
#endif

System::OperatingSystem Environment::getOSVersionProperty() {
#if defined(_WIN32)
    OSVERSIONINFOEXW osvi{};
    osvi.dwOSVersionInfoSize = sizeof(osvi);
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4996)
#endif
    GetVersionExW(reinterpret_cast<OSVERSIONINFOW*>(&osvi));
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
    Version v(static_cast<int>(osvi.dwMajorVersion),
               static_cast<int>(osvi.dwMinorVersion),
               static_cast<int>(osvi.dwBuildNumber),
               static_cast<int>(osvi.wServicePackMajor));
    return System::OperatingSystem(PlatformID::Win32NT, v);
#elif defined(__EMSCRIPTEN__)
    return System::OperatingSystem(PlatformID::Other, Version(0, 0));
#else
    // Parse utsname.release (e.g. "6.1.0-28-amd64") for major.minor.build.revision
    struct utsname uts {};
    uname(&uts);
    std::string release(uts.release);
    std::size_t pos = 0;
    int major    = parseNextNumber(release, pos);
    int minor    = parseNextNumber(release, pos);
    int build    = parseNextNumber(release, pos);
    int revision = parseNextNumber(release, pos);
    return System::OperatingSystem(PlatformID::Unix, Version(major, minor, build, revision));
#endif
}

// Upper bound on the buffers the two path-retrieval doors below are allowed to grow to.
// It is a runaway guard, not a path limit: a pathological or repeated ERANGE must not turn
// into unbounded allocation. Every real current directory, and every real executable path,
// is orders of magnitude below it. The 4 KiB fixed buffers this replaced were the defect
// (SR-AUD-107) precisely because they were near the size of real data.
static constexpr std::size_t kPathRetrievalCeiling = 1024u * 1024u;

std::string Environment::GetCurrentDirectory() {
    // The current directory has no portable length limit: a process can reach one longer
    // than PATH_MAX by chdir()ing one legal component at a time, and getcwd() then reports
    // ERANGE for any buffer that cannot hold it. This used to call getcwd() once into a
    // fixed char[4096] and return "" on ERANGE, so a real 4,868-byte current directory --
    // built exactly that way and measured in build-probe/2239_probe1_before.log -- became
    // an empty string. .NET's Unix Interop.Sys.GetCwd imposes no such public ceiling, and
    // this repository's own System::IO::Directory::GetCurrentDirectory has never had one
    // (it is std::filesystem::current_path()). Grow until the call succeeds; every failure
    // that is NOT ERANGE keeps returning "", so the error contract is unchanged.
    // Ticket #2240 / SR-AUD-107; see docs/CoreEnvironmentCompatibleSlicePlan.md.
#if defined(_WIN32)
    // Win32's own documented two-call pattern: a zero-length call returns the required
    // buffer size INCLUDING the terminating NUL.
    const DWORD needed = GetCurrentDirectoryA(0, nullptr);
    if (needed == 0 || needed > kPathRetrievalCeiling) return "";
    std::vector<char> buf(needed);
    const DWORD written = GetCurrentDirectoryA(needed, buf.data());
    if (written == 0 || written >= needed) return "";
    return std::string(buf.data(), written);
#else
    std::vector<char> buf(4096);
    for (;;) {
        if (getcwd(buf.data(), buf.size())) return std::string(buf.data());
        if (errno != ERANGE) return "";
        if (buf.size() >= kPathRetrievalCeiling) return "";
        buf.resize(buf.size() * 2);
    }
#endif
}

SharpRuntime::intcs Environment::getProcessorCountProperty() {
#if defined(_WIN32)
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    return static_cast<SharpRuntime::intcs>(si.dwNumberOfProcessors);
#elif defined(__EMSCRIPTEN__)
    return 1; // WebAssembly is single-threaded without pthreads
#else
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? static_cast<SharpRuntime::intcs>(n) : 1;
#endif
}

std::string Environment::getMachineNameProperty() {
#if defined(_WIN32)
    char buf[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(buf);
    if (GetComputerNameA(buf, &size)) return std::string(buf);
    return "";
#else
    // Real .NET's Unix MachineName truncates at the first '.' to strip the domain suffix
    // (Environment.Unix.cs: `int dotPos = hostName.IndexOf('.'); return dotPos < 0 ? hostName :
    // hostName.Substring(0, dotPos);`) -- gethostname() alone can return a fully-qualified name
    // like "myhost.example.com" on systems where that's how the hostname is configured.
    char buf[HOST_NAME_MAX + 1];
    if (gethostname(buf, sizeof(buf)) != 0) return "";
    std::string hostName(buf);
    auto dotPos = hostName.find('.');
    return dotPos == std::string::npos ? hostName : hostName.substr(0, dotPos);
#endif
}

std::string Environment::getUserNameProperty() {
#if defined(_WIN32)
    char buf[256];
    DWORD size = sizeof(buf);
    if (GetUserNameA(buf, &size)) return std::string(buf);
    return "";
#elif defined(__EMSCRIPTEN__)
    const char* user = std::getenv("USER");
    return user ? std::string(user) : std::string("user");
#else
    const char* user = std::getenv("USER");
    if (user) return std::string(user);
    struct passwd* pw = getpwuid(getuid());
    return pw ? std::string(pw->pw_name) : std::string();
#endif
}

static void validateEnvironmentVariableName(const std::string& name) {
    ArgumentException::ThrowIfNullOrEmpty(name, "variable");
    if (name[0] == '\0')
        throw ArgumentException("The first char in the string is the null character.", "variable");
    if (name.find('=') != std::string::npos)
        throw ArgumentException("Environment variable name cannot contain equal character.", "variable");
}

void Environment::SetEnvironmentVariable(const std::string& name, const std::string& value) {
    validateEnvironmentVariableName(name);
#if defined(_WIN32)
    if (value.empty())
        _putenv_s(name.c_str(), "");
    else
        _putenv_s(name.c_str(), value.c_str());
#else
    if (value.empty())
        ::unsetenv(name.c_str());
    else
        ::setenv(name.c_str(), value.c_str(), 1);
#endif
}

void Environment::SetEnvironmentVariable(const std::string& name, const std::string& value,
                                         EnvironmentVariableTarget target) {
    if (target == EnvironmentVariableTarget::Process) {
        SetEnvironmentVariable(name, value);
        return;
    }
    // No Windows-registry (or other persistent-store) backing in this port, so — matching
    // .NET's own behavior on non-Windows platforms — User/Machine are validated but otherwise no-ops.
    validateEnvironmentVariableName(name);
}

std::vector<std::string> Environment::GetLogicalDrives() {
#if defined(_WIN32)
    std::vector<std::string> drives;
    char buf[512];
    DWORD len = GetLogicalDriveStringsA(static_cast<DWORD>(sizeof(buf)), buf);
    for (char* p = buf; p < buf + len; p += std::strlen(p) + 1)
        drives.emplace_back(p);
    return drives;
#else
    return { "/" };
#endif
}

std::string Environment::GetEnvironmentVariable(const std::string& name) {
    std::string value;
    return tryGetEnvironmentVariable(name, value) ? value : std::string();
}

// Ported from real .NET's Environment.ExpandEnvironmentVariablesCore (Environment.UnixOrBrowser.cs)
// rather than a from-scratch %VAR% scanner, since the reference algorithm has a non-obvious
// property the previous implementation here didn't reproduce: when a %name% token fails to
// resolve, only its opening '%' is treated as consumed literal text -- the closing '%' is left
// to double as the *opening* delimiter of the next token, rather than both percents being
// consumed as one failed pair. E.g. for "%UNDEFINED%HOME%" (UNDEFINED unset, HOME set), real
// .NET produces "%UNDEFINED" + <value of HOME> (the middle '%' is absorbed into the HOME token),
// not "%UNDEFINED%" + "HOME%" with HOME left unexpanded, which is what a naive
// find-the-next-'%'-and-consume-both scanner (the previous version of this function) produced.
std::string Environment::ExpandEnvironmentVariables(const std::string& name) {
    if (name.empty()) return name;

    std::string result;
    result.reserve(name.size());
    size_t lastPos = 0;
    size_t pos;
    while (lastPos < name.size() && (pos = name.find('%', lastPos + 1)) != std::string::npos) {
        if (name[lastPos] == '%') {
            std::string key = name.substr(lastPos + 1, pos - lastPos - 1);
            std::string value;
            if (tryGetEnvironmentVariable(key, value)) {
                result += value;
                lastPos = pos + 1;
                continue;
            }
        }
        result.append(name, lastPos, pos - lastPos);
        lastPos = pos;
    }
    result.append(name, lastPos, std::string::npos);
    return result;
}

std::string Environment::GetFolderPath(SpecialFolder folder) {
#if defined(_WIN32)
    char buf[MAX_PATH];
    if (SHGetFolderPathA(nullptr, static_cast<int>(folder), nullptr, SHGFP_TYPE_CURRENT, buf) == S_OK)
        return std::string(buf);
    return "";
#else
    const char* home = std::getenv("HOME");
    std::string h = home ? std::string(home) : std::string();
    switch (folder) {
        case SpecialFolder::Personal:          // == MyDocuments (0x0005), returns home
        case SpecialFolder::UserProfile:      return h;
        case SpecialFolder::Desktop:
        case SpecialFolder::DesktopDirectory: return h + "/Desktop";
        case SpecialFolder::MyMusic:          return h + "/Music";
        case SpecialFolder::MyPictures:       return h + "/Pictures";
        case SpecialFolder::MyVideos:         return h + "/Videos";
        case SpecialFolder::ApplicationData:  return h + "/.config";
        case SpecialFolder::LocalApplicationData: return h + "/.local/share";
        case SpecialFolder::CommonApplicationData: return "/etc";
        case SpecialFolder::ProgramFiles:     return "/usr";
        case SpecialFolder::System:           return "/usr/lib";
        case SpecialFolder::Fonts:            return "/usr/share/fonts";
        case SpecialFolder::Templates:        return h + "/Templates";
        default:                              return "";
    }
#endif
}

Environment::ProcessCpuUsage Environment::getCpuUsageProperty() {
#if defined(_WIN32)
    FILETIME creation, exit, kernel, user;
    if (GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user)) {
        auto toTS = [](FILETIME ft) -> TimeSpan {
            longcs ticks = (static_cast<longcs>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
            return TimeSpan(ticks); // FILETIME is in 100-ns units, same as TimeSpan ticks
        };
        return { toTS(user), toTS(kernel) };
    }
    return { TimeSpan::Zero, TimeSpan::Zero };
#elif defined(__EMSCRIPTEN__)
    return { TimeSpan::Zero, TimeSpan::Zero };
#else
    struct rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        auto tvToTS = [](const struct timeval& tv) -> TimeSpan {
            longcs ticks = static_cast<longcs>(tv.tv_sec) * 10000000LL
                         + static_cast<longcs>(tv.tv_usec) * 10LL;
            return TimeSpan(ticks);
        };
        return { tvToTS(usage.ru_utime), tvToTS(usage.ru_stime) };
    }
    return { TimeSpan::Zero, TimeSpan::Zero };
#endif
}

SharpRuntime::intcs Environment::getProcessIdProperty() {
#if defined(_WIN32)
    return static_cast<SharpRuntime::intcs>(GetCurrentProcessId());
#else
    return static_cast<SharpRuntime::intcs>(::getpid());
#endif
}

SharpRuntime::longcs Environment::getTickCount64Property() {
#if defined(_WIN32)
    return static_cast<SharpRuntime::longcs>(GetTickCount64());
#else
    struct timespec ts{};
    // CLOCK_BOOTTIME is Linux-specific (added in kernel 2.6.39) -- undeclared on Apple/BSD
    // platforms entirely (confirmed via a real macOS CI build: "use of undeclared identifier
    // 'CLOCK_BOOTTIME'"). CLOCK_MONOTONIC is the portable POSIX fallback available on every
    // Unix-like platform, and matches .NET's own cross-platform GetTickCount64 PAL
    // implementation, which uses CLOCK_MONOTONIC uniformly rather than a Linux-only clock.
#if defined(CLOCK_BOOTTIME)
    clock_gettime(CLOCK_BOOTTIME, &ts);
#else
    clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
    return static_cast<SharpRuntime::longcs>(ts.tv_sec) * 1000LL
         + static_cast<SharpRuntime::longcs>(ts.tv_nsec) / 1000000LL;
#endif
}

bool Environment::getIsPrivilegedProcessProperty() {
#if defined(_WIN32)
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION elev{};
    DWORD size = 0;
    bool result = GetTokenInformation(token, TokenElevation, &elev, sizeof(elev), &size)
                  && elev.TokenIsElevated;
    CloseHandle(token);
    return result;
#elif defined(__EMSCRIPTEN__)
    return false;
#else
    return ::geteuid() == 0;
#endif
}

void Environment::SetCurrentDirectory(const std::string& path) {
    // Real .NET's Environment.CurrentDirectory setter first calls
    // ArgumentException.ThrowIfNullOrEmpty(value) before touching the OS (Environment.cs), then
    // throws when the underlying OS call fails (Environment.Windows.cs:
    // `if (!Interop.Kernel32.SetCurrentDirectory(value))` throws a Win32-error-derived exception,
    // typically DirectoryNotFoundException for a missing path) -- this previously did neither,
    // silently accepting an empty path and ignoring chdir()'s return value entirely instead of
    // surfacing either failure.
    ArgumentException::ThrowIfNullOrEmpty(path, "value");
#if defined(_WIN32)
    if (!SetCurrentDirectoryA(path.c_str()))
        throw System::IO::DirectoryNotFoundException("Could not find a part of the path '" + path + "'.");
#else
    if (chdir(path.c_str()) != 0)
        throw System::IO::DirectoryNotFoundException("Could not find a part of the path '" + path + "'.");
#endif
}

std::string Environment::getProcessPathProperty() {
    // Same fixed-4-KiB ceiling as GetCurrentDirectory had, and worse: neither OS primitive
    // here reports truncation as an error. readlink() returns the number of bytes it
    // WROTE, so a longer path used to come back silently TRUNCATED rather than empty, and
    // GetModuleFileNameA's return was discarded entirely, so a failure handed back whatever
    // was in the buffer. A filled buffer is the only truncation signal either one gives, so
    // that is what the loops test. Ticket #2240 / SR-AUD-107, which names this branch.
    //
    // Measured, and deliberately NOT overclaimed: on Linux the truncation is unreachable
    // through this interface. procfs builds the `exe` link target in a page-sized buffer, so
    // a running process whose own executable path exceeds PATH_MAX gets ENAMETOOLONG from
    // readlink() rather than a long path -- reproduced by running a helper binary from a
    // 4,671-byte directory (build-probe/2240_probe2.log). Before and after this change that
    // case returns the same empty string, which is the right answer. What the loop buys is
    // defensive correctness on any platform that does not cap the answer for us, plus the
    // Windows zero-return handling, which was a real unconditional defect.
#if defined(_WIN32)
    std::vector<char> buf(4096);
    for (;;) {
        SetLastError(ERROR_SUCCESS);
        const DWORD len = GetModuleFileNameA(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
        if (len == 0) return "";
        if (len < buf.size() && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            return std::string(buf.data(), len);
        if (buf.size() >= kPathRetrievalCeiling) return "";
        buf.resize(buf.size() * 2);
    }
#elif defined(__EMSCRIPTEN__)
    return "";
#else
    std::vector<char> buf(4096);
    for (;;) {
        const ssize_t len = readlink("/proc/self/exe", buf.data(), buf.size());
        if (len <= 0) return "";
        if (static_cast<std::size_t>(len) < buf.size())
            return std::string(buf.data(), static_cast<std::size_t>(len));
        if (buf.size() >= kPathRetrievalCeiling) return "";
        buf.resize(buf.size() * 2);
    }
#endif
}

std::map<std::string, std::string> Environment::GetEnvironmentVariables() {
    std::map<std::string, std::string> result;
#if defined(_WIN32)
    LPCH envBlock = GetEnvironmentStringsA();
    if (envBlock) {
        for (LPCH p = envBlock; *p; p += std::strlen(p) + 1) {
            std::string key, value;
            if (splitEnvEntry(p, key, value)) result[key] = value;
        }
        FreeEnvironmentStringsA(envBlock);
    }
#else
    for (char** ep = environ; ep && *ep; ++ep) {
        std::string key, value;
        if (splitEnvEntry(*ep, key, value)) result[key] = value;
    }
#endif
    return result;
}

SharpRuntime::intcs Environment::getSystemPageSizeProperty() {
#if defined(_WIN32)
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    return static_cast<SharpRuntime::intcs>(si.dwPageSize);
#else
    return static_cast<SharpRuntime::intcs>(getpagesize());
#endif
}

std::string Environment::getUserDomainNameProperty() {
#if defined(_WIN32)
    char buf[256]{};
    DWORD size = sizeof(buf);
    if (GetComputerNameExA(ComputerNameDnsDomain, buf, &size) && buf[0])
        return std::string(buf);
    return getMachineNameProperty();
#else
    return getMachineNameProperty();
#endif
}

SharpRuntime::longcs Environment::getWorkingSetProperty() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return static_cast<SharpRuntime::longcs>(pmc.WorkingSetSize);
    return 0;
#elif defined(__linux__)
    FILE* f = std::fopen("/proc/self/status", "r");
    if (!f) return 0;
    char line[256];
    long kb = 0;
    while (std::fgets(line, sizeof(line), f)) {
        if (std::sscanf(line, "VmRSS: %ld kB", &kb) == 1) break;
    }
    std::fclose(f);
    return static_cast<SharpRuntime::longcs>(kb) * 1024LL;
#else
    return 0;
#endif
}

static std::vector<std::string> s_commandLineArgs;

void Environment::InitializeCommandLine(int argc, char** argv) {
    s_commandLineArgs.clear();
    for (int i = 0; i < argc; ++i) s_commandLineArgs.emplace_back(argv[i]);
}

std::vector<std::string> Environment::GetCommandLineArgs() {
    return s_commandLineArgs;
}

namespace {

    // The ASCII whitespace set char.IsWhiteSpace recognises among the Basic-Latin bytes that
    // can appear unescaped in an argument. Deliberately NOT std::isspace, which is
    // locale-dependent and takes an int: this is a byte-level operation over UTF-8 storage.
    bool isArgWhitespace(char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
    }

    bool containsNoWhitespaceOrQuotes(const std::string& s) {
        for (char c : s)
            if (isArgWhitespace(c) || c == '"') return false;
        return true;
    }

    // PasteArguments.AppendArgument -- the inverse of CommandLineToArgvW's parsing rules for
    // every argument after argv[0]:
    //   - a backslash is an ordinary character EXCEPT when followed by a quote;
    //   - 2n backslashes before a quote parse as n literal backslashes plus a delimiter quote;
    //   - 2n+1 backslashes before a quote parse as n literal backslashes plus a literal quote;
    //   - parsing stops at the first whitespace outside a quoted region.
    void appendArgument(std::string& out, const std::string& argument) {
        if (!out.empty()) out += ' ';

        if (!argument.empty() && containsNoWhitespaceOrQuotes(argument)) {
            out += argument;   // the common case emits byte-identically to the old join
            return;
        }

        out += '"';
        std::size_t idx = 0;
        while (idx < argument.size()) {
            const char c = argument[idx++];
            if (c == '\\') {
                std::size_t numBackSlash = 1;
                while (idx < argument.size() && argument[idx] == '\\') { ++idx; ++numBackSlash; }
                if (idx == argument.size()) {
                    // A closing quote is about to follow, so these must be doubled.
                    out.append(numBackSlash * 2, '\\');
                } else if (argument[idx] == '"') {
                    out.append(numBackSlash * 2 + 1, '\\');
                    out += '"';
                    ++idx;
                } else {
                    out.append(numBackSlash, '\\');
                }
            } else if (c == '"') {
                out += '\\';
                out += '"';
            } else {
                out += c;
            }
        }
        out += '"';
    }

    // PasteArguments.Paste's pasteFirstArgumentUsingArgV0Rules branch. argv[0] parses under
    // different rules: a backslash is always an ordinary character and quotes exist only to
    // carry whitespace, so there is no way to escape anything.
    void appendArgV0(std::string& out, const std::string& argument) {
        bool hasWhitespace = false;
        for (char c : argument)
            if (isArgWhitespace(c)) { hasWhitespace = true; break; }

        if (argument.empty() || hasWhitespace) {
            // A literal '"' cannot be represented under these rules at all. This port emits
            // it verbatim rather than rejecting the argument, because a public diagnostic
            // property that throws for a legal argv[0] would be a worse failure than an
            // unparseable one. What .NET does here is not derivable from the finding and
            // /rv is absent in this container, so the choice is PINNED by a test and
            // deferred to ticket #2242 rather than guessed.
            out += '"';
            out += argument;
            out += '"';
        } else {
            out += argument;
        }
    }

} // namespace

std::string Environment::getCommandLineProperty() {
    // .NET's Environment.CommandLine is
    // PasteArguments.Paste(GetCommandLineArgs(), pasteFirstArgumentUsingArgV0Rules: true),
    // which quotes whitespace and escapes quotes and backslashes so the argument sequence
    // round-trips. This used to space-join the raw entries verbatim, so "prog" + "two words"
    // re-parsed as three arguments and "prog" + "" lost an argument entirely -- measured
    // against a CommandLineToArgvW-shaped reference parser in
    // build-probe/2239_probe1_before.log, 16 of 18 rows wrong. An argument that is non-empty
    // and free of whitespace and quotes is still emitted verbatim, so the common case is
    // byte-identical to the old output. Ticket #2241 / SR-AUD-108.
    std::string result;
    for (std::size_t i = 0; i < s_commandLineArgs.size(); ++i) {
        if (i == 0) appendArgV0(result, s_commandLineArgs[i]);
        else        appendArgument(result, s_commandLineArgs[i]);
    }
    return result;
}

} // namespace System
