#include "SessionManager.h"


SessionManager::SessionManager (const size_t timeout_sec, CdrWriter& writer, const std::shared_ptr<spdlog::logger>& logger)
: _timeout_sec (timeout_sec), _cdr_writer (writer), _logger (logger) {
}

bool SessionManager::create_session (const std::string& imsi) {
    std::lock_guard lock (_mutex);
    const auto now = std::chrono::steady_clock::now ();

    auto [it, inserted] = _sessions.try_emplace (imsi, SessionInfo{ now });
    if (!inserted) {
        it->second.start_time = now;
        return false;
    }

    _cdr_writer.write (imsi, "created");
    _logger->info ("Session created: {}", imsi);
    return true;
}


bool SessionManager::has_session (const std::string& imsi) const {
    std::lock_guard lock (_mutex);
    return _sessions.contains (imsi);
}

void SessionManager::remove_timeout () {
    const auto now = std::chrono::steady_clock::now ();
    std::vector<std::string> to_remove;
    {
        std::lock_guard lock (_mutex);
        for (auto& [imsi, info] : _sessions) {
            if (const auto age = std::chrono::duration_cast<std::chrono::seconds> (now - info.start_time).count ();
            age >= static_cast<long> (_timeout_sec))
                to_remove.push_back (imsi);
        }
        for (auto& imsi : to_remove)
            _sessions.erase (imsi);
    }
    for (auto& imsi : to_remove) {
        _cdr_writer.write (imsi, "timeout");
        _logger->info ("Session timed‑out and removed: {}", imsi);
    }
}

bool SessionManager::remove_session (const std::string& imsi, std::string_view reason) {
    std::lock_guard lock (_mutex);
    const auto it = _sessions.find (imsi);
    if (it == _sessions.end ())
        return false;
    _sessions.erase (it);
    _cdr_writer.write (imsi, std::string (reason));
    _logger->info ("Session removed: {} (reason={})", imsi, reason);
    return true;
}


size_t SessionManager::session_count () const {
    std::lock_guard lock (_mutex);
    return _sessions.size ();
}

bool SessionManager::pop_one (std::string& imsi_out) {
    std::lock_guard lock (_mutex);
    if (_sessions.empty ())
        return false;

    const auto it = _sessions.begin ();
    imsi_out      = it->first;
    _sessions.erase (it);

    _cdr_writer.write (imsi_out, "offload");
    _logger->info ("Session removed (offload): {}", imsi_out);
    return true;
}
