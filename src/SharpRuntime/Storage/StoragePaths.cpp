#include "SharpRuntime/Storage/StoragePaths.hpp"

#include <filesystem>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

namespace SharpRuntime::Storage
{
    std::filesystem::path StoragePaths::GetIsolatedStorageRoot()
    {
#if defined(__EMSCRIPTEN__)
        // On Emscripten, persist save data under /save which is mounted as
        // IDBFS by the application startup code so data survives page reloads.
        const std::filesystem::path root = std::filesystem::path("/save") / ".cna_isolated_storage";
#else
        const std::filesystem::path root = std::filesystem::current_path() / ".cna_isolated_storage";
#endif
        std::filesystem::create_directories(root);
        return root;
    }
}