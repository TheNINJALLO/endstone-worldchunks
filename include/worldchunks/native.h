#pragma once
#include <endstone/server.h>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

namespace worldchunks {
class Native {
  public:
    explicit Native(endstone::Server& server);
    ~Native();
    void install();
    void stop();
    void tick();
    nlohmann::json status();
    nlohmann::json inspect(const std::string& dimension, int x, int z);
    nlohmann::json deny(const std::string& dimension, int x, int z);
    nlohmann::json allow(const std::string& dimension, int x, int z);
    nlohmann::json pin(const std::string& dimension, int x, int z);
    nlohmann::json unpin(const std::string& dimension, int x, int z);
    nlohmann::json views();
    nlohmann::json players();
    nlohmann::json radius(int value);
    nlohmann::json policy();
    nlohmann::json reset();
    void joined(endstone::Player& player);
    void left(endstone::Player& player);
    void spawnGrace();
    nlohmann::json optimizer(const nlohmann::json& request);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace worldchunks
