// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Environment.hpp"
#ifdef _WIN32
#include "System/ApplicationException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#endif
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
#  include <sys/stat.h>   // GetFolderPath's SpecialFolderOption::Create (#2320)
#else
#  include <unistd.h>
#  include <climits>
#  include <sys/stat.h>   // GetFolderPath's SpecialFolderOption::Create (#2320)
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

namespace {

#if !defined(_WIN32)
/// The XDG base-directory rule, transcribed from `Environment.GetFolderPathCore.Unix.cs:153-163`.
///
/// The test is `config is null || !config.StartsWith('/')` -- so a variable is honoured ONLY when
/// it is set AND ABSOLUTE, and an empty value falls back exactly as an unset one does, because an
/// empty string does not start with '/' either. The XDG specification says the same: a relative
/// value "must be ignored". Ticket #2320 decision 1, answer (b).
std::string xdgBase(const char* variable, const std::string& home, const char* fallback) {
    const char* value = std::getenv(variable);
    if (value != nullptr && value[0] == '/') return std::string(value);
    return home + fallback;
}

/// `Interop.Sys.Access(path, R_OK) == 0` -- the verification .NET's DEFAULT option performs.
bool readable(const std::string& path) {
    return !path.empty() && ::access(path.c_str(), R_OK) == 0;
}

/// `Directory.CreateDirectory(path)`: creates every missing component, like `mkdir -p`.
///
/// Written with `::mkdir` rather than `System::IO::Directory` on purpose -- `Core.Base` does not
/// depend on `modules/io`, and giving it that edge to create one directory would be a far larger
/// change than the behaviour it buys.
void createDirectoryTree(const std::string& path) {
    if (path.empty()) return;
    std::string partial;
    partial.reserve(path.size());
    for (std::size_t i = 0; i < path.size(); ++i) {
        partial.push_back(path[i]);
        const bool last = (i + 1 == path.size());
        if (path[i] == '/' || last) {
            if (partial != "/" ) ::mkdir(partial.c_str(), 0777);   // EEXIST is the normal case
        }
    }
}
#endif

}  // namespace

std::string Environment::GetFolderPath(SpecialFolder folder) {
    return GetFolderPath(folder, SpecialFolderOption::None);
}

std::string Environment::GetFolderPath(SpecialFolder folder, SpecialFolderOption option) {
    // `Environment.cs:149-163`: the OPTION is validated even though the FOLDER is not -- an
    // undefined folder legitimately returns "" (see #2321), an undefined option never does.
    if (option != SpecialFolderOption::None && option != SpecialFolderOption::Create &&
        option != SpecialFolderOption::DoNotVerify) {
        throw System::ArgumentOutOfRangeException(
            "option", "Illegal enum value: " + std::to_string(static_cast<long long>(option)) + ".");
    }

#if defined(_WIN32)
    // Windows resolves and applies the flags in one call: SpecialFolderOption's values ARE the
    // CSIDL flags, so they are simply OR-ed into the folder id, which is why this branch does not
    // repeat the POSIX verification below.
    char buf[MAX_PATH];
    const int csidl = static_cast<int>(folder) | static_cast<int>(option);
    if (SHGetFolderPathA(nullptr, csidl, nullptr, SHGFP_TYPE_CURRENT, buf) == S_OK)
        return std::string(buf);
    return "";
#else
    const char* home = std::getenv("HOME");
    // `GetFolderPathCore.Unix.cs:76-81`: fall back to "/" when the home directory is unknown.
    // .NET states the reason and it is a safety property rather than a tidy default -- "/" is not
    // writable by a non-root user, so an application cannot silently write private data into a
    // path built from an empty string.
    std::string h = (home != nullptr && home[0] != '\0') ? std::string(home) : std::string("/");
    if (h == "/") h.clear();   // so h + "/.config" stays "/.config" rather than "//.config"

    std::string path;
    switch (folder) {
        case SpecialFolder::Personal:          // == MyDocuments (0x0005), returns home
        case SpecialFolder::UserProfile:      path = h.empty() ? "/" : h; break;
        case SpecialFolder::Desktop:
        case SpecialFolder::DesktopDirectory: path = h + "/Desktop"; break;
        case SpecialFolder::MyMusic:          path = h + "/Music"; break;
        case SpecialFolder::MyPictures:       path = h + "/Pictures"; break;
        case SpecialFolder::MyVideos:         path = h + "/Videos"; break;
        // The two XDG bases, and the ONLY two this ticket adopts -- see the migration note for
        // the rest of the table, which diverges from the reference in six further places and is
        // ticket #2364 rather than a silent widening here.
        case SpecialFolder::ApplicationData:  path = xdgBase("XDG_CONFIG_HOME", h, "/.config"); break;
        case SpecialFolder::LocalApplicationData:
                                              path = xdgBase("XDG_DATA_HOME", h, "/.local/share"); break;
        case SpecialFolder::CommonApplicationData: path = "/etc"; break;
        case SpecialFolder::ProgramFiles:     path = "/usr"; break;
        case SpecialFolder::System:           path = "/usr/lib"; break;
        case SpecialFolder::Fonts:            path = "/usr/share/fonts"; break;
        case SpecialFolder::Templates:        path = h + "/Templates"; break;
        default:                              return "";
    }

    // `GetFolderPathCore.Unix.cs:26-47`. Note what the DEFAULT does: `None` VERIFIES, and returns
    // "" when the directory is not readable. Before #2320 this port returned every path
    // unconditionally, so a caller could not tell a real directory from a name.
    if (path.empty() || option == SpecialFolderOption::DoNotVerify || readable(path)) return path;
    if (option == SpecialFolderOption::None) return "";
    createDirectoryTree(path);
    return path;
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

#ifdef _WIN32
    // PasteArguments.Paste's pasteFirstArgumentUsingArgV0Rules branch. argv[0] parses under
    // different rules: a backslash is always an ordinary character and quotes exist only to
    // carry whitespace, so there is no way to escape anything.
    //
    // WINDOWS ONLY, and that is .NET's split, resolved by ticket #2242 from the reference
    // tree: PasteArguments has two partial implementations, and the Unix one
    // (PasteArguments.Unix.cs) IGNORES pasteFirstArgumentUsingArgV0Rules entirely -- every
    // argument, argv[0] included, goes through AppendArgument. See getCommandLineProperty().
    void appendArgV0(std::string& out, const std::string& argument) {
        bool hasWhitespace = false;
        for (char c : argument) {
            if (c == '"') {
                // A literal '"' cannot be represented under these rules at all, and .NET
                // rejects it rather than emitting something that will not parse back:
                // PasteArguments.Windows.cs throws ApplicationException(
                // SR.Argv_IncludeDoubleQuote) from inside this very loop. #2241 emitted it
                // verbatim on the reasoning that a throwing diagnostic property is worse than
                // an unparseable one; #2242 measured the reference and that pin was wrong.
                throw System::ApplicationException("The argv[0] argument cannot include a double quote.");
            }
            if (isArgWhitespace(c)) hasWhitespace = true;
        }

        if (argument.empty() || hasWhitespace) {
            out += '"';
            out += argument;
            out += '"';
        } else {
            out += argument;
        }
    }
#endif // _WIN32

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
    //
    // Ticket #2242 added the platform split, and it is transcribed rather than invented.
    // .NET's PasteArguments is a partial class with two implementations of Paste:
    //
    //   PasteArguments.Windows.cs  honours pasteFirstArgumentUsingArgV0Rules
    //   PasteArguments.Unix.cs     "On Unix: the rules for parsing the executable name
    //                              (argv[0]) are ignored" -- every argument, argv[0]
    //                              included, goes through AppendArgument
    //
    // Environment.cs:125 passes `pasteFirstArgumentUsingArgV0Rules: true` on both, so the
    // flag is honoured on Windows and discarded everywhere else. This port applied the
    // argv[0] rules unconditionally, which diverged on its own baseline platform.
    //
    // The two rule sets agree on almost everything, so the observable Linux change is small:
    // an argv[0] with whitespace is quoted either way, one containing only backslashes is
    // emitted verbatim either way, and an empty argv[0] becomes "" either way. They differ
    // for an argv[0] containing a quote -- now escaped as `"pro\"g"` rather than emitted raw.
    std::string result;
    for (std::size_t i = 0; i < s_commandLineArgs.size(); ++i) {
#ifdef _WIN32
        if (i == 0) appendArgV0(result, s_commandLineArgs[i]);
        else        appendArgument(result, s_commandLineArgs[i]);
#else
        appendArgument(result, s_commandLineArgs[i]);
#endif
    }
    return result;
}

} // namespace System
