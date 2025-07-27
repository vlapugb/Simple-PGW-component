#pragma once

#include <array>
#include <atomic>
#include <thread>

#include "SessionManager.h"
#include "arpa/inet.h"
#include "boost/lockfree/queue.hpp"
#include "spdlog/spdlog.h"

#include "../Common/ConfigLoader.h"

class SessionManager;
class BlackListStorer;
class ControlPlaneServer;
class CdrWriter;

namespace Pgw {
constexpr std::size_t QUEUE_CAPACITY   = 4096;
constexpr std::size_t UDP_BUFFER_SIZE  = 1024;
constexpr std::size_t MAX_EPOLL_EVENTS = 16;

struct UdpPacket {
    sockaddr_in client_addr{};
    std::array<uint8_t, UDP_BUFFER_SIZE> bcd{};
    socklen_t addr_len{};
    std::size_t data_len{};

    UdpPacket () : addr_len (sizeof (client_addr)) {
    }
};

struct UdpResponse {
    std::string response;
    sockaddr_in client_addr{};
    socklen_t addr_len{ sizeof (client_addr) };
};

using PacketQueue   = boost::lockfree::queue<UdpPacket*, boost::lockfree::capacity<QUEUE_CAPACITY> >;
using ResponseQueue = boost::lockfree::queue<UdpResponse*, boost::lockfree::capacity<QUEUE_CAPACITY> >;

class UdpServer {
    public:
    explicit UdpServer (const Common::ServerSettings& settings,
    std::shared_ptr<ControlPlaneServer> control_plane_server,
    std::shared_ptr<SessionManager> session_manager,
    std::shared_ptr<BlackListStorer> black_list_storer,
    std::shared_ptr<CdrWriter> cdr_writer);

    ~UdpServer ();

    void start ();

    void stop ();

    void initiate_graceful_shutdown ();

    std::size_t session_count () const {
        return _sessions ? _sessions->session_count () : 0;
    }

    void set_http_server (std::shared_ptr<ControlPlaneServer> http) {
        _http = std::move (http);
    }

    private:
    void init_socket ();

    void event_loop ();

    void process_packets ();

    void send_responses ();

    void offload_sessions ();

    static std::string bcd_to_imsi (const uint8_t* data, std::size_t len);


    const std::string _bind_ip;
    const uint16_t _bind_port;
    const std::size_t _graceful_shutdown_rate;

    int _udp_fd   = -1;
    int _epoll_fd = -1;

    std::shared_ptr<ControlPlaneServer> _http;
    std::shared_ptr<SessionManager> _sessions;
    std::shared_ptr<BlackListStorer> _blacklist;
    std::shared_ptr<CdrWriter> _cdr;

    std::atomic<bool> _running{ false };
    std::atomic<bool> _graceful{ false };

    PacketQueue _recv_queue;
    ResponseQueue _send_queue;

    std::thread _epoll_thread;
    std::thread _worker_thread;
    std::thread _sender_thread;
    std::thread _timeout_thread;
    std::thread _offload_thread;

    std::shared_ptr<spdlog::logger> _logger;
};
} // namespace Pgw
