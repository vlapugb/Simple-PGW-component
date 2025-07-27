#include "ControlPlaneServer.h"
#include "crow.h"
#include "spdlog/spdlog.h"

ControlPlaneServer::ControlPlaneServer (const Common::ServerSettings& settings,
SessionManager& session_manager,
StopCallback on_stop,
const std::shared_ptr<spdlog::logger>& logger)
: _http_port (settings.http_port), _session_mgr (session_manager), _on_stop (std::move (on_stop)), _logger (logger) {
}

ControlPlaneServer::~ControlPlaneServer () {
    stop ();
}

void ControlPlaneServer::start () {
    if (_running.exchange (true))
        return;
    CROW_ROUTE (_app, "/check_subscriber").methods (crow::HTTPMethod::GET, crow::HTTPMethod::POST) ([this] (const crow::request& req) {
        const char* imsi = req.url_params.get ("imsi");
        if (!imsi) {
            return crow::response (400, "Bad request: missing IMSI");
        }
        const bool subscribed = _session_mgr.has_session (imsi);
        return crow::response{ subscribed ? "active" : "not active" };
    });

    CROW_ROUTE (_app, "/stop").methods (crow::HTTPMethod::GET, crow::HTTPMethod::POST) ([this] (const crow::request& req) {
        _logger->info ("HTTP /stop requested — graceful shutdown initiated");
        _on_stop ();
        return crow::response ("Stopping");
    });


    for (std::size_t i = 0; i < 1; ++i)
        _workers.emplace_back ([this] { _app.port (_http_port).run (); });
}

void ControlPlaneServer::stop () {
    if (!_running.exchange (false))
        return;
    _app.stop ();
    for (auto& t : _workers)
        if (t.joinable ())
            t.join ();
    _workers.clear ();

    _logger->info ("HTTP stopped");
}

void ControlPlaneServer::request_graceful_shutdown () const {
    {
        _on_stop ();
    }
}
