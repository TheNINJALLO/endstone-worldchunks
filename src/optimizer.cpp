#include "worldchunks/api.h"
#include "worldchunks/optimizer_policy.h"
#include "worldchunks/presentation.h"
#include "worldchunks/storage.h"
#include <endstone/plugin/plugin.h>
#include <endstone/scheduler/scheduler.h>
#include <endstone/scheduler/task.h>
#include <nlohmann/json.hpp>
#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif
#include <chrono>
#include <fstream>
#include <sstream>

namespace {
using Json = nlohmann::json;
double now()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
Json defaults()
{
    return {{"schema", 1},
            {"enabled", true},
            {"keep_radius", 2},
            {"simulation_radius", 2},
            {"cleanup_interval_seconds", 30},
            {"tps_threshold", 18.0},
            {"low_tps_seconds", 5},
            {"tps_cooldown_seconds", 30},
            {"batch_size", 32}};
}
void validate(const Json& c)
{
    const auto expected = defaults();
    if (!c.is_object() || c.size() != expected.size()) {
        throw std::runtime_error("Config must contain exactly the documented settings");
    }
    for (const auto& [key, value] : expected.items()) {
        const auto& v = c.at(key);
        if (value.is_boolean() ? !v.is_boolean() : !v.is_number()) {
            throw std::runtime_error("Invalid type for " + key);
        }
        if (value.is_number_integer() && !v.is_number_integer()) {
            throw std::runtime_error(key + " must be an integer");
        }
    }
    auto range = [&](const char* key, double lo, double hi) {
        const double v = c.at(key).get<double>();
        if (!std::isfinite(v) || v < lo || v > hi) {
            throw std::runtime_error(std::string(key) + " must be " + std::to_string(lo) + ".." + std::to_string(hi));
        }
    };
    range("schema", 1, 1);
    range("keep_radius", 1, 32);
    range("simulation_radius", 1, c.at("keep_radius").get<int>());
    range("cleanup_interval_seconds", 0, 86400);
    range("tps_threshold", 0, 20);
    range("low_tps_seconds", 1, 300);
    range("tps_cooldown_seconds", 1, 3600);
    range("batch_size", 1, 512);
}
} // namespace

class WorldChunksOptimizer : public endstone::Plugin {
    WorldChunksRequestV1 request_{};
    std::shared_ptr<endstone::Task> task_;
    Json config_ = defaults();
    worldchunks::CleanupTrigger trigger_;
    bool ready_{};
    uint64_t interval_triggers_{}, tps_triggers_{};
    double sampled_tps_{20}, next_sample_{};
    Json call(const Json& request)
    {
        auto* provider = getServer().getPluginManager().getPlugin("worldchunks");
        if (!request_ || !provider || !provider->isEnabled()) {
            throw std::runtime_error("WorldChunks dependency is unavailable");
        }
        const auto response = Json::parse(request_(request.dump().c_str()));
        if (!response.at("ok").get<bool>()) {
            throw std::runtime_error(response.at("error").get<std::string>());
        }
        return response.at("result");
    }
    void connect()
    {
        auto* provider = getServer().getPluginManager().getPlugin("worldchunks");
        if (!provider || !provider->isEnabled()) {
            throw std::runtime_error("Install WorldChunks 0.3.0 alongside the optimizer");
        }
        // Endstone loads a shadow copy with RTLD_LOCAL. Resolve the API in that
        // already loaded module, without loading a second native adapter.
#ifdef _WIN32
        HMODULE module{};
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                reinterpret_cast<LPCWSTR>(*reinterpret_cast<void**>(provider)), &module)) {
            throw std::runtime_error("Cannot locate WorldChunks module");
        }
        request_ = reinterpret_cast<WorldChunksRequestV1>(GetProcAddress(module, WORLDCHUNKS_API_SYMBOL));
#else
        Dl_info info{};
        if (!dladdr(*reinterpret_cast<void**>(provider), &info)) {
            throw std::runtime_error("Cannot locate WorldChunks module");
        }
        void* module = dlopen(info.dli_fname, RTLD_NOW | RTLD_NOLOAD);
        if (!module) {
            throw std::runtime_error("WorldChunks module is not loaded");
        }
        request_ = reinterpret_cast<WorldChunksRequestV1>(dlsym(module, WORLDCHUNKS_API_SYMBOL));
        dlclose(module);
#endif
        if (!request_) {
            throw std::runtime_error("WorldChunks is too old: install the bundled 0.3.0 native plugin");
        }
        call({{"op", "status"}});
    }
    Json readConfig()
    {
        auto path = worldchunks::dataFolder(*this) / "config.json";
        if (!std::filesystem::exists(path)) {
            return defaults();
        }
        std::ifstream input(path);
        auto result = Json::parse(input);
        validate(result);
        return result;
    }
    void saveConfig(const Json& config)
    {
        std::filesystem::create_directories(worldchunks::dataFolder(*this));
        auto path = worldchunks::dataFolder(*this) / "config.json.tmp";
        {
            std::ofstream out(path);
            out << config.dump(2) << '\n';
            out.flush();
            if (!out) {
                throw std::runtime_error("Could not save optimizer config");
            }
        }
        worldchunks::replaceFile(path, worldchunks::dataFolder(*this) / "config.json");
    }
    void apply(const Json& candidate, bool save)
    {
        validate(candidate);
        // Check provider availability before persisting an operator change.
        call({{"op", "status"}});
        if (save) {
            saveConfig(candidate);
        }
        call({{"op", "configure"},
              {"enabled", candidate.at("enabled")},
              {"keep_radius", candidate.at("keep_radius")},
              {"simulation_radius", candidate.at("simulation_radius")},
              {"batch_size", candidate.at("batch_size")}});
        config_ = candidate;
        trigger_.reset(now(), config_.at("cleanup_interval_seconds"));
        next_sample_ = now();
    }
    Json status()
    {
        return {{"config", config_},
                {"native", call({{"op", "status"}})},
                {"sampled_tps", sampled_tps_},
                {"tps_source", "Endstone current TPS, sampled once per second"},
                {"interval_triggers", interval_triggers_},
                {"tps_triggers", tps_triggers_}};
    }
    void tick()
    {
        if (!ready_ || !getServer().getLevel() || !config_.at("enabled").get<bool>()) {
            return;
        }
        const auto time = now();
        if (time < next_sample_) {
            return;
        }
        next_sample_ = time + 1;
        sampled_tps_ = getServer().getCurrentTicksPerSecond();
        const auto reason =
            trigger_.poll(time, sampled_tps_, config_.at("cleanup_interval_seconds"), config_.at("tps_threshold"),
                          config_.at("low_tps_seconds"), config_.at("tps_cooldown_seconds"));
        if (reason.empty()) {
            return;
        }
        auto result = call({{"op", "cleanup"}, {"reason", reason}});
        if (reason == "low_tps") {
            ++tps_triggers_;
        }
        else {
            ++interval_triggers_;
        }
        getLogger().info("Cleanup requested: {}, TPS {:.2f}, radius {}, batch {}{}", reason, sampled_tps_,
                         config_.at("keep_radius").get<int>(), config_.at("batch_size").get<int>(),
                         result.at("paused_for_players").get<bool>() ? " (waiting for player grace)" : "");
    }

  public:
    void onEnable() override
    {
        // Startup dependencies have initialized their policy by this point, but
        // use the scheduler so both plugins always access a fully created world.
        task_ = getServer().getScheduler().runTaskTimer(
            *this,
            [this] {
                if (!getServer().getLevel()) {
                    return;
                }
                try {
                    if (!ready_) {
                        connect();
                        apply(readConfig(), true);
                        ready_ = true;
                        getLogger().info("Optimizer ready. Use /wco status and /wco help.");
                    }
                    tick();
                }
                catch (const std::exception& e) {
                    getLogger().error("Optimizer stopped: {}. Fix config/dependency, then use /wco reload.", e.what());
                    try {
                        if (request_) {
                            call({{"op", "disable"}});
                        }
                    }
                    catch (...) {
                    }
                    ready_ = false;
                    if (task_) {
                        task_->cancel();
                    }
                }
            },
            1, 1);
    }
    void onDisable() override
    {
        if (task_) {
            task_->cancel();
        }
        try {
            if (request_) {
                call({{"op", "disable"}});
            }
        }
        catch (const std::exception& e) {
            getLogger().warning("Optimizer release: {}", e.what());
        }
        ready_ = false;
        request_ = nullptr;
    }
    bool onCommand(endstone::CommandSender& sender, const endstone::Command&,
                   const std::vector<std::string>& raw) override
    {
        try {
            if (!sender.hasPermission("worldchunks.optimizer.admin")) {
                throw std::runtime_error("Missing worldchunks.optimizer.admin permission");
            }
            std::vector<std::string> args;
            for (const auto& part : raw) {
                std::istringstream input(part);
                for (std::string word; input >> word;) {
                    args.push_back(word);
                }
            }
            const bool json = worldchunks::takeJsonFlag(args);
            if (args.size() == 1 && args[0] == "help") {
                sender.sendMessage(
                    "wco status | config | on | off | run | reload\nwco set <setting> <value>\nSettings: keep_radius, "
                    "simulation_radius, cleanup_interval_seconds (0=off), tps_threshold (0=off), low_tps_seconds, "
                    "tps_cooldown_seconds, batch_size.\nRadius is a square around every online player. Pins are "
                    "preserved. Movement/spawn grace is 100 ticks.");
                return true;
            }
            if (args.size() == 1 && args[0] == "reload") {
                auto candidate = readConfig();
                connect();
                apply(candidate, false);
                if (!ready_) {
                    if (task_) {
                        task_->cancel();
                    }
                    ready_ = true;
                    onEnable();
                }
            }
            else {
                if (!ready_) {
                    throw std::runtime_error("Optimizer is not ready; check startup log or use wco reload");
                }
                if (args.empty() || (args.size() == 1 && args[0] == "status")) {
                }
                else if (args.size() == 1 && args[0] == "config") {
                    worldchunks::sendOptimizerResult(sender, config_, json);
                    return true;
                }
                else if (args.size() == 1 && (args[0] == "on" || args[0] == "off")) {
                    auto candidate = config_;
                    candidate["enabled"] = args[0] == "on";
                    apply(candidate, true);
                }
                else if (args.size() == 1 && args[0] == "run") {
                    call({{"op", "cleanup"}, {"reason", "manual"}});
                }
                else if (args.size() == 3 && args[0] == "set") {
                    if (!config_.contains(args[1]) || args[1] == "schema" || args[1] == "enabled") {
                        throw std::runtime_error("Unknown setting; use wco help");
                    }
                    auto candidate = config_;
                    candidate[args[1]] = Json::parse(args[2]);
                    apply(candidate, true);
                }
                else {
                    throw std::runtime_error("Use wco help for syntax");
                }
            }
            worldchunks::sendOptimizerResult(sender, status(), json);
        }
        catch (const std::exception& e) {
            sender.sendErrorMessage(std::string("WorldChunksOptimizer: ") + e.what());
        }
        return true;
    }
};
ENDSTONE_PLUGIN("worldchunks_optimizer", "0.3.0", WorldChunksOptimizer)
{
    description = "Automatic player chunk radius and TPS-triggered cleanup for WorldChunks";
    depend = {"worldchunks"};
    permission("worldchunks.optimizer.admin")
        .description("Configure the chunk optimizer")
        .default_(endstone::PermissionDefault::Operator);
    command("wco")
        .description("Configure automatic chunk cleanup")
        .usages("/wco [args: message]")
        .permissions("worldchunks.optimizer.admin");
}
