#pragma once
#include <random>

#include "../Common/ConfigLoader.h"
#include "../Pgw/UdpServer.h"
#include "spdlog/logger.h"

namespace client {
class UdpClient {
    public:
    explicit UdpClient (const Common::ClientSettings& settings);

    void init_sockets ();

    void send_imsi (const std::string& imsi_in) const;

    [[nodiscard]] std::string receive () const;

    ~UdpClient ();

    UdpClient (const UdpClient&) = delete;

    UdpClient (const UdpClient&&) = delete;

    UdpClient& operator= (const UdpClient&) = delete;

    UdpClient& operator= (const UdpClient&&) = delete;

    static std::string generate_imsi (const uint16_t mcc = 250, const uint16_t mnc = 99, const uint8_t total_len = 15) {
        if (mcc < 100 || mcc > 999)
            throw std::invalid_argument ("MCC must be 3 digits");
        if (mnc < 10 || mnc > 999)
            throw std::invalid_argument ("MNC must be 2 or 3 digits");
        if (total_len < 12 || total_len > 15)
            throw std::invalid_argument ("IMSI total length must be 12-15");

        std::ostringstream ss;
        ss << std::setw (3) << std::setfill ('0') << mcc;
        ss << std::setw ((mnc < 100) ? 2 : 3) << std::setfill ('0') << mnc;

        const size_t rand_digits = total_len - ss.str ().length ();
        std::random_device rd;
        std::mt19937 gen (rd ());
        std::uniform_int_distribution<> d (0, 9);

        for (int i = 0; i < rand_digits; ++i)
            ss << d (gen);

        return ss.str ();
    }

    private:
    static constexpr size_t MAX_EVENTS_EPOLL = 16;
    static constexpr size_t MAX_BUFFER_SIZE  = 1024;

    int _udp_fd{ -1 };
    int _epoll_fd{ -1 };

    std::string _server_ip;
    uint16_t _server_port;
    std::shared_ptr<spdlog::logger> _logger;


    static std::vector<uint8_t> imsi_to_bcd (const std::string& imsi_in) {
        std::vector<uint8_t> bcd;
        std::string imsi = imsi_in;
        if (imsi.size () > 15)
            throw std::invalid_argument ("IMSI size must be 15 characters");

        if (imsi.size () % 2 != 0) {
            imsi = imsi + 'F';
        }

        for (size_t _char = 0; _char < imsi.size (); _char += 2) {
            const uint8_t low_part  = (imsi[_char] != 'F') ? (imsi[_char] - '0') : 0xF;
            const uint8_t high_part = (imsi[_char + 1] != 'F') ? (imsi[_char + 1] - '0') : 0xF;
            uint8_t bcd_byte        = (high_part << 4) | low_part;
            bcd.push_back (bcd_byte);
        }
        return bcd;
    }
};
} // namespace client
