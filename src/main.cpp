#include <csignal>
#include <iostream>
#include <memory>
#include <thread>

#include "Common/ConfigLoader.h"
#include "Pgw/BlackListStorer.h"
#include "Pgw/CdrWriter.h"
#include "Pgw/ControlPlaneServer.h"
#include "Pgw/SessionManager.h"
#include "Pgw/UdpServer.h"

static std::atomic g_stop{ false };

static void on_signal (int) {
    g_stop = true;
}

int main () {
    try {
        const auto settings = Common::SettingsLoader::load<Common::ServerSettings> ("SERVER_SETTINGS");
        auto log            = Common::make_file_logger (settings, "pgw_main", settings.log_file);
        log->flush_on (spdlog::level::info);
        log->info ("=== pgw_server starting ===");
        auto cdr_writer = std::make_shared<CdrWriter> (settings, log);
        auto blacklist  = std::make_shared<BlackListStorer> (1'000'003, log);
        auto sessions   = std::make_shared<SessionManager> (settings.session_timeout_sec, *cdr_writer, log);
        auto udp_srv    = std::make_shared<Pgw::UdpServer> (settings, nullptr, sessions, blacklist, cdr_writer);
        auto stop_cb    = [udp_srv] () {
            udp_srv->initiate_graceful_shutdown ();
            g_stop.store (true);
        };
        const auto http = std::make_shared<ControlPlaneServer> (settings, *sessions, stop_cb, log);
        udp_srv->set_http_server (http);
        http->start ();
        udp_srv->start ();
        std::signal (SIGINT, on_signal);
        std::signal (SIGTERM, on_signal);

        while (!g_stop)
            std::this_thread::sleep_for (std::chrono::milliseconds (200));
        log->info ("Signal received → graceful shutdown");
        stop_cb ();
        http->stop ();
        udp_srv.reset ();

        log->info ("=== pgw_server stopped ===");
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Fatal: " << ex.what () << '\n';
        return 1;
    }
}
