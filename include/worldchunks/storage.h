#pragma once

#include <endstone/plugin/plugin.h>
#include <filesystem>
#ifdef _WIN32
#include <Windows.h>
#endif

namespace worldchunks {
inline std::filesystem::path dataFolder(const endstone::Plugin& plugin)
{
    // A wheel loads the native libraries from its private package directory.
    // Keep settings beside the bundle's data folder so updates preserve them.
    if (auto* bundle = plugin.getServer().getPluginManager().getPlugin("worldchunks_bundle")) {
        return bundle->getDataFolder().parent_path() / plugin.getName();
    }
    return plugin.getDataFolder();
}

inline void replaceFile(const std::filesystem::path& temporary, const std::filesystem::path& target)
{
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        throw std::filesystem::filesystem_error(
            "Cannot replace settings file", temporary, target,
            std::error_code(static_cast<int>(GetLastError()), std::system_category()));
    }
#else
    std::filesystem::rename(temporary, target);
#endif
}
} // namespace worldchunks
