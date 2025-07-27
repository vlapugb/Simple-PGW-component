#pragma once
#include "../Common/ConfigLoader.h"
#include "spdlog/spdlog.h"
#include <fstream>

class CdrWriter {
    public:
    explicit CdrWriter (const Common::ServerSettings& settings, const std::shared_ptr<spdlog::logger>& logger);

    ~CdrWriter ();

    void write (const std::string& imsi, const std::string& action);

    void flush ();

    CdrWriter (const CdrWriter&) = delete;

    CdrWriter& operator= (const CdrWriter&) = delete;

    CdrWriter (CdrWriter&&) = delete;

    CdrWriter& operator= (CdrWriter&&) = delete;

    private:
    std::ofstream _stream;
    std::mutex _mutex;
    std::shared_ptr<spdlog::logger> _logger;

    static std::string _timestamp_iso_utc ();
};
