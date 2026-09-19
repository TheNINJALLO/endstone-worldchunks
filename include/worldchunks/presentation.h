#pragma once

#include <algorithm>
#include <endstone/command/command_sender.h>
#include <nlohmann/json.hpp>
#include <sstream>

namespace worldchunks {
inline bool takeJsonFlag(std::vector<std::string>& args)
{
    return std::erase(args, "--json") != 0;
}

inline std::string label(std::string key)
{
    std::replace(key.begin(), key.end(), '_', ' ');
    if (!key.empty()) {
        key.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(key.front())));
    }
    return key;
}

inline std::string valueText(const nlohmann::json& value)
{
    if (value.is_boolean()) {
        return value.get<bool>() ? "yes" : "no";
    }
    if (value.is_string()) {
        return value.get<std::string>();
    }
    return value.dump();
}

inline void sendFields(endstone::CommandSender& sender, const nlohmann::json& result)
{
    for (const auto& [key, value] : result.items()) {
        sender.sendMessage(label(key) + ": " + valueText(value));
    }
}

inline void sendCoreResult(endstone::CommandSender& sender, const nlohmann::json& result, bool json)
{
    if (json) {
        sender.sendMessage("WORLDCHUNKS " + result.dump());
        return;
    }
    sender.sendMessage("WorldChunks");
    if (result.contains("tracked_views")) {
        sender.sendMessage("Native hooks: active | Views: " + valueText(result.at("tracked_views")) +
                           " | Pinned: " + valueText(result.at("pins")));
        sender.sendMessage("Manual unload rules: " + valueText(result.at("denied")) +
                           " | Optimizer rules: " + valueText(result.at("optimizer_denied")));
        sender.sendMessage("Simulation radius: " + valueText(result.at("effective_radius_cap")) +
                           " (0 = vanilla) | Tick offsets: " + valueText(result.at("effective_tick_offsets")));
        for (const auto& dimension : result.at("dimensions")) {
            sender.sendMessage(dimension.at("name").get<std::string>() + ": " + valueText(dimension.at("loaded")) +
                               " loaded chunks");
        }
    }
    else if (result.is_array()) {
        if (result.empty()) {
            sender.sendMessage("No entries.");
        }
        for (size_t i = 0; i < result.size(); ++i) {
            sender.sendMessage("Entry " + std::to_string(i + 1));
            sendFields(sender, result[i]);
        }
    }
    else {
        sendFields(sender, result);
    }
}

inline void sendOptimizerResult(endstone::CommandSender& sender, const nlohmann::json& result, bool json)
{
    if (json) {
        sender.sendMessage("WORLDCHUNKS_OPTIMIZER " + result.dump());
        return;
    }
    sender.sendMessage("WorldChunks Optimizer");
    if (!result.contains("config")) {
        sendFields(sender, result);
        return;
    }
    const auto& config = result.at("config");
    const auto& native = result.at("native");
    sender.sendMessage(std::string("Enabled: ") + valueText(config.at("enabled")) +
                       " | Keep radius: " + valueText(config.at("keep_radius")) +
                       " | Simulation radius: " + valueText(config.at("simulation_radius")));
    sender.sendMessage("Cleanup interval: " + valueText(config.at("cleanup_interval_seconds")) +
                       "s | TPS threshold: " + valueText(config.at("tps_threshold")));
    std::ostringstream tps;
    tps.setf(std::ios::fixed);
    tps.precision(2);
    tps << result.at("sampled_tps").get<double>();
    sender.sendMessage("Current TPS: " + tps.str() +
                       " | Waiting for players: " + valueText(native.at("paused_for_players")));
    sender.sendMessage("Cleanup runs: " + valueText(native.at("cleanup_runs")) +
                       " | Chunks queued: " + valueText(native.at("pending_chunks")) +
                       " | Temporary unload rules: " + valueText(native.at("temporary_denies")));
}
} // namespace worldchunks
