#pragma once
#include "CdrWriter.h"
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>


class CdrWriter;

class SessionManager {
    public:
    explicit SessionManager (size_t timeout_sec, CdrWriter& writer, const std::shared_ptr<spdlog::logger>& logger);
    ~SessionManager () = default;

    bool create_session (const std::string& imsi);

    bool has_session (const std::string& imsi) const;

    void remove_timeout ();

    bool remove_session (const std::string& imsi, std::string_view reason);

    size_t session_count () const;

    bool pop_one (std::string& imsi_out);

    private:
    struct SessionInfo {
        std::chrono::steady_clock::time_point start_time;
    };
    size_t _timeout_sec = 0;
    mutable std::mutex _mutex;
    std::unordered_map<std::string, SessionInfo> _sessions;
    CdrWriter& _cdr_writer;
    std::shared_ptr<spdlog::logger> _logger;
};
