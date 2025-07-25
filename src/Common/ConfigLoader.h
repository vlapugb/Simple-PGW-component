#pragma once

#include <fstream>
#include <unordered_set>

#include "nlohmann/json.hpp"
#include "spdlog/spdlog.h"

namespace Common {

    struct ServerSettings {
        std::string peer{};
        size_t port{};
        size_t session_timeout{};
        std::string cdr_log = "cdr.log";
        size_t http_port;
        size_t graceful_shutdown_rate;
        std::string pgw_log = "pgw.log";
        spdlog::level::level_enum log_level = spdlog::level::trace;
        std::unordered_set<std::string> blacklist;
    };

    struct ClientSettings {
        sockaddr_in peer{};
        size_t port{};
        std::string client_log = "client.log";
        spdlog::level::level_enum log_level = spdlog::level::trace;
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ServerSettings,
                                       peer, port, session_timeout, cdr_log, http_port,
                                       graceful_shutdown_rate, pgw_log, log_level, blacklist)

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ClientSettings,
                                       peer, port, client_log, log_level)

    class SettingsLoader final {
    public:
        template<typename TypeSettings>
        [[nodiscard]] static TypeSettings load(const char *type_env_value);
    };

    template<typename TypeSettings>
    TypeSettings SettingsLoader::load(const char *type_env_value) {
        nlohmann::json _server_settings;
        const char *path = std::getenv(type_env_value);
        if (!path) {
            throw std::runtime_error(std::string("Environment variable '") + type_env_value + "' is not set");
        }

        std::ifstream json_reader(path);
        if (!json_reader)
            throw std::runtime_error(
                std::string("Settings file for '") + type_env_value + "' not found at path: " + path);
        json_reader >> _server_settings;
        return _server_settings.get<TypeSettings>();
    }
}
