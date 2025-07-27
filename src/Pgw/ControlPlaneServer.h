#pragma once
#include <atomic>
#include <functional>
#include <memory>
#include <thread>

#include "SessionManager.h"
#include "crow.h"

class ControlPlaneServer {
    public:
    using StopCallback = std::function<void ()>;

    ControlPlaneServer (const Common::ServerSettings& settings,
    SessionManager& session_manager,
    StopCallback on_stop,
    const std::shared_ptr<spdlog::logger>& logger);

    ~ControlPlaneServer ();

    void start ();

    void stop ();

    void request_graceful_shutdown () const;

    private:
    uint16_t _http_port;
    SessionManager& _session_mgr;
    StopCallback _on_stop;
    crow::SimpleApp _app;
    std::vector<std::thread> _workers;
    std::atomic<bool> _running{ false };
    std::shared_ptr<spdlog::logger> _logger;
};
