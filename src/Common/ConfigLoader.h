#pragma once

#include <fstream>
#include <unordered_set>

#include "nlohmann/json.hpp"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"

namespace Common {
struct ServerSettings {
    std::string udp_ip{};
    size_t udp_port{};
    size_t session_timeout_sec{};
    std::string cdr_file = "cdr.log";
    size_t http_port;
    size_t graceful_shutdown_rate;
    std::string log_file                = "pgw.log";
    spdlog::level::level_enum log_level = spdlog::level::trace;
    std::unordered_set<std::string> blacklist;
};

struct ClientSettings {
    std::string server_ip{};
    size_t server_port{};
    std::string log_file                = "client.log";
    spdlog::level::level_enum log_level = spdlog::level::trace;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE (ServerSettings, udp_ip, udp_port, session_timeout_sec, cdr_file, http_port, graceful_shutdown_rate, log_file, log_level, blacklist)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE (ClientSettings, server_ip, server_port, log_file, log_level)

class SettingsLoader final {
    public:
    template <typename TypeSettings> [[nodiscard]] static TypeSettings load (const char* type_env_value);
};

template <typename TypeSettings> TypeSettings SettingsLoader::load (const char* type_env_value) {
    nlohmann::json _settings;
    const char* path = std::getenv (type_env_value);
    if (!path) {
        throw std::runtime_error (std::string ("Environment variable '") + type_env_value + "' is not set");
    }

    std::ifstream json_reader (path);
    if (!json_reader)
        throw std::runtime_error (std::string ("Settings file for '") + type_env_value + "' not found at path: " + path);
    json_reader >> _settings;
    if (_settings.contains ("log_level") && _settings["log_level"].is_string ()) {
        auto lvl_str           = _settings["log_level"].get<std::string> ();
        auto lvl_enum          = spdlog::level::from_str (lvl_str);
        _settings["log_level"] = lvl_enum;
    }

    return _settings.get<TypeSettings> ();
}

template <typename TypeSettings>
static std::shared_ptr<spdlog::logger>
make_file_logger (const TypeSettings& settings, const std::string& logger_name, const std::string& file_name) {
    const char* env                = std::getenv ("PROJECT_DIR");
    std::filesystem::path log_path = env;
    log_path /= "logs";
    log_path /= file_name;
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt> (log_path.string (), true);
    auto logger    = std::make_shared<spdlog::logger> (logger_name, file_sink);
    logger->set_level (settings.log_level);
    return logger;
}
} // namespace Common
