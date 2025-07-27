#include "CdrWriter.h"

#include <sstream>
#include <stdexcept>

#include "../Common/ConfigLoader.h"

CdrWriter::CdrWriter (const Common::ServerSettings& settings, const std::shared_ptr<spdlog::logger>& logger)
: _logger (logger) {
    const char* env = std::getenv ("PROJECT_DIR");
    const std::filesystem::path cdr_path =
    env ? std::filesystem::path{ env } / "logs" / settings.cdr_file : std::filesystem::path{ settings.cdr_file };

    _stream.open (cdr_path, std::ios::out | std::ios::app);
    if (!_stream.is_open ()) {
        throw std::runtime_error ("Failed to open CDR file: " + settings.cdr_file);
    }
    _logger->info ("CDR writer started, file {}", settings.cdr_file);
}

CdrWriter::~CdrWriter () {
    try {
        flush ();
        _stream.close ();
        _logger->info ("CDR writer stopped");
    } catch (...) {
        if (_logger)
            _logger->error ("Exception during CDR writer shutdown");
    }
}

void CdrWriter::write (const std::string& imsi, const std::string& action) {
    const std::string ts = _timestamp_iso_utc ();

    std::lock_guard lock (_mutex);
    _stream << ts << ", " << imsi << ", " << action << '\n';
    if (!_stream.good ()) {
        _logger->error ("Failed to write CDR for IMSI {} action {}", imsi, action);
    }
    _stream.flush ();
}

void CdrWriter::flush () {
    std::lock_guard lock (_mutex);
    _stream.flush ();
}

std::string CdrWriter::_timestamp_iso_utc () {
    using namespace std::chrono;
    const auto now   = system_clock::now ();
    const auto now_t = system_clock::to_time_t (now);
    std::tm tm{};
    gmtime_r (&now_t, &tm);
    std::ostringstream ss;
    ss << std::put_time (&tm, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str ();
}
