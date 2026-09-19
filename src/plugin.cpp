#include "worldchunks/api.h"
#include "worldchunks/native.h"
#include "worldchunks/presentation.h"
#include "worldchunks/storage.h"
#include <algorithm>
#include <charconv>
#include <endstone/event/player/player_join_event.h>
#include <endstone/event/player/player_quit_event.h>
#include <endstone/event/player/player_respawn_event.h>
#include <endstone/event/player/player_teleport_event.h>
#include <endstone/level/dimension.h>
#include <endstone/plugin/plugin.h>
#include <endstone/scheduler/scheduler.h>
#include <endstone/scheduler/task.h>
#include <fstream>
#include <sstream>

namespace {
worldchunks::Native* api_native{};
}
extern "C" ENDSTONE_EXPORT const char* worldchunks_request_v1(const char* input) noexcept
{
    static thread_local std::string result;
    try {
        if (!api_native) {
            throw std::runtime_error("WorldChunks native adapter is unavailable");
        }
        result = nlohmann::json{{"ok", true}, {"result", api_native->optimizer(nlohmann::json::parse(input))}}.dump();
    }
    catch (const std::exception& e) {
        result = nlohmann::json{{"ok", false}, {"error", e.what()}}.dump();
    }
    return result.c_str();
}

class WorldChunksPlugin : public endstone::Plugin {
    std::unique_ptr<worldchunks::Native> native_;
    std::shared_ptr<endstone::Task> task_;
    bool policy_loaded_{};
    void savePolicy()
    {
        std::filesystem::create_directories(worldchunks::dataFolder(*this));
        auto path = worldchunks::dataFolder(*this) / "policy.json";
        auto temporary = worldchunks::dataFolder(*this) / "policy.json.tmp";
        {
            std::ofstream out(temporary);
            out << native_->policy().dump(2) << '\n';
            out.flush();
            if (!out) {
                throw std::runtime_error("Policy changed in memory but could not be saved to disk");
            }
        }
        worldchunks::replaceFile(temporary, path);
    }
    void loadPolicy()
    {
        auto path = worldchunks::dataFolder(*this) / "policy.json";
        if (!std::filesystem::exists(path)) {
            savePolicy();
            return;
        }
        std::ifstream input(path);
        auto policy = nlohmann::json::parse(input);
        if (policy.at("schema") != 1) {
            throw std::runtime_error("Unsupported policy schema");
        }
        int radius = policy.at("simulation_radius_cap").get<int>();
        if (radius < 0 || radius > 32) {
            throw std::runtime_error("Invalid saved radius");
        }
        auto dimensions = getServer().getLevel()->getDimensions();
        for (const auto* list : {"denied", "pins"}) {
            if (!policy.at(list).is_array()) {
                throw std::runtime_error("Policy coordinates must be arrays");
            }
            for (const auto& entry : policy.at(list)) {
                auto name = entry.at("dimension").get<std::string>();
                int x = entry.at("x").get<int>(), z = entry.at("z").get<int>();
                if (x < -1874999 || x > 1874999 || z < -1874999 || z > 1874999) {
                    throw std::runtime_error("Saved coordinate outside world border");
                }
                if (std::none_of(dimensions.begin(), dimensions.end(), [&](auto* d) { return d->getName() == name; })) {
                    throw std::runtime_error("Unknown saved dimension: " + name);
                }
            }
        }
        native_->radius(radius);
        for (const auto& entry : policy.at("denied")) {
            native_->deny(entry.at("dimension"), entry.at("x"), entry.at("z"));
        }
        for (const auto& entry : policy.at("pins")) {
            native_->pin(entry.at("dimension"), entry.at("x"), entry.at("z"));
        }
        getLogger().info("Restored saved chunk policy.");
    }

  public:
    void onLoad() override
    {
        try {
            native_ = std::make_unique<worldchunks::Native>(getServer());
            native_->install();
            api_native = native_.get();
            getLogger().info("Exact BDS 1.26.51.1 / Endstone 0.11.11 binary checks passed; chunk hooks installed.");
        }
        catch (const std::exception& e) {
            getLogger().error("Native adapter unavailable: {}", e.what());
            native_.reset();
        }
    }
    void onEnable() override
    {
        if (!native_) {
            return;
        }
        registerEvent(&WorldChunksPlugin::onJoin, *this, endstone::EventPriority::Monitor);
        registerEvent(&WorldChunksPlugin::onQuit, *this, endstone::EventPriority::Monitor);
        registerEvent(&WorldChunksPlugin::onTeleport, *this, endstone::EventPriority::Monitor, true);
        registerEvent(&WorldChunksPlugin::onRespawn, *this, endstone::EventPriority::Monitor);
        task_ = getServer().getScheduler().runTaskTimer(
            *this,
            [this] {
                if (!getServer().getLevel()) {
                    return;
                }
                if (!policy_loaded_) {
                    policy_loaded_ = true;
                    try {
                        loadPolicy();
                    }
                    catch (const std::exception& e) {
                        getLogger().error("Saved policy could not be restored: {}", e.what());
                    }
                }
                try {
                    native_->tick();
                }
                catch (const std::exception& e) {
                    getLogger().error("Chunk policy failed: {}", e.what());
                }
            },
            1, 1);
    }
    void onDisable() override
    {
        api_native = nullptr;
        if (task_) {
            task_->cancel();
        }
        if (native_) {
            native_->stop();
        }
    }
    void onJoin(endstone::PlayerJoinEvent& event)
    {
        if (native_) {
            native_->joined(event.getPlayer());
        }
    }
    void onQuit(endstone::PlayerQuitEvent& event)
    {
        if (native_) {
            native_->left(event.getPlayer());
        }
    }
    void onTeleport(endstone::PlayerTeleportEvent&)
    {
        if (native_) {
            native_->spawnGrace();
        }
    }
    void onRespawn(endstone::PlayerRespawnEvent&)
    {
        if (native_) {
            native_->spawnGrace();
        }
    }
    bool onCommand(endstone::CommandSender& sender, const endstone::Command&,
                   const std::vector<std::string>& raw) override
    {
        try {
            std::vector<std::string> args;
            for (const auto& part : raw) {
                std::istringstream stream(part);
                for (std::string word; stream >> word;) {
                    args.push_back(std::move(word));
                }
            }
            const bool json = worldchunks::takeJsonFlag(args);
            if (!sender.hasPermission("worldchunks.admin")) {
                sender.sendErrorMessage("Missing worldchunks.admin permission");
                return true;
            }
            if (!native_) {
                throw std::runtime_error("Native adapter is unavailable; see startup log");
            }
            nlohmann::json result;
            bool changed = false;
            if (args.empty() || (args.size() == 1 && args[0] == "status")) {
                result = native_->status();
            }
            else if (args.size() == 1 && args[0] == "views") {
                result = native_->views();
            }
            else if (args.size() == 1 && args[0] == "players") {
                result = native_->players();
            }
            else if (args.size() == 1 && args[0] == "policy") {
                result = native_->policy();
            }
            else if (args.size() == 1 && args[0] == "reset") {
                result = native_->reset();
                changed = true;
            }
            else if (args.size() == 1 && args[0] == "help") {
                sender.sendMessage(
                    "wc status | players | views | policy | reset\nwc radius <0=vanilla|1..32>\nwc "
                    "<inspect|unload|allow|load|release> <Overworld|Nether|TheEnd> <chunk-x> <chunk-z>\nCoordinates "
                    "are chunks (floor(block / 16)). Changes persist in policy.json.");
                return true;
            }
            else if (args[0] == "radius" && args.size() == 2) {
                int radius = -1;
                const auto [end, err] = std::from_chars(args[1].data(), args[1].data() + args[1].size(), radius);
                if (err != std::errc{} || end != args[1].data() + args[1].size()) {
                    throw std::runtime_error("Radius must be an integer");
                }
                result = native_->radius(radius);
                changed = true;
            }
            else if (args.size() == 4) {
                int x{}, z{};
                auto parse = [](const std::string& s, int& out) {
                    const auto [end, err] = std::from_chars(s.data(), s.data() + s.size(), out);
                    if (err != std::errc{} || end != s.data() + s.size() || out < -1874999 || out > 1874999) {
                        throw std::runtime_error("Chunk coordinates must be integers within the world border");
                    }
                };
                parse(args[2], x);
                parse(args[3], z);
                if (args[0] == "inspect") {
                    result = native_->inspect(args[1], x, z);
                }
                else if (args[0] == "unload") {
                    result = native_->deny(args[1], x, z);
                }
                else if (args[0] == "allow") {
                    result = native_->allow(args[1], x, z);
                }
                else if (args[0] == "load") {
                    result = native_->pin(args[1], x, z);
                }
                else if (args[0] == "release") {
                    result = native_->unpin(args[1], x, z);
                }
                else {
                    throw std::runtime_error("Unknown operation");
                }
                changed = args[0] != "inspect";
            }
            else {
                throw std::runtime_error("Use wc help for syntax");
            }
            if (changed) {
                savePolicy();
            }
            worldchunks::sendCoreResult(sender, result, json);
        }
        catch (const std::exception& e) {
            sender.sendErrorMessage(std::string("WorldChunks: ") + e.what());
        }
        return true;
    }
};
ENDSTONE_PLUGIN("worldchunks", "0.3.0", WorldChunksPlugin)
{
    description = "Native chunk residency control for the exact BDS 1.26.51.1 build";
    load = endstone::PluginLoadOrder::Startup;
    permission("worldchunks.admin")
        .description("Manage native chunk residency")
        .default_(endstone::PermissionDefault::Operator);
    command("wc")
        .description("Control native chunk loading and unloading")
        .usages("/wc [args: message]")
        .permissions("worldchunks.admin");
}
