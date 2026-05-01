#include "SharpRuntime/Storage/StoragePaths.hpp"

#include <filesystem>

namespace SharpRuntime::Storage
{
    std::filesystem::path StoragePaths::GetIsolatedStorageRoot()
    {
        const std::filesystem::path root = std::filesystem::current_path() / ".cna_isolated_storage";
        std::filesystem::create_directories(root);
        return root;
    }
}