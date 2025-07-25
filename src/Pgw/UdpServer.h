#pragma once

#include "arpa/inet.h"
#include "spdlog/spdlog.h"

#include "boost/lockfree/queue.hpp"
//#include "../Common/ConfigLoader.h"

class SessionManager;
class BlackListStorer;
class ControlPlaneServer;
class CdrWriter;

// constexpr std::size_t sizeLfQueue = 4096;
// using LfQueue = boost::lockfree::queue<Datagram, boost::lockfree::capacity<sizeLfQueue> >;

namespace Pgw {
    constexpr std::size_t QUEUE_CAPACITY = 4096;


    struct UdpPacket {
        sockaddr_in client_addr{};
        std::string bcd;
        socklen_t addr_len{};
        size_t data_len{};

        UdpPacket() : addr_len(sizeof(client_addr)) {
        }
    };

    struct UdpResponse {
        std::string response{};
        sockaddr_in client_addr{};
        socklen_t addr_len{};

        UdpResponse() : addr_len(sizeof(client_addr)) {
        }
    };

    using PacketQueue =
    boost::lockfree::queue<UdpPacket *, boost::lockfree::capacity<QUEUE_CAPACITY> >;
    using ResponseQueue =
    boost::lockfree::queue<UdpResponse *, boost::lockfree::capacity<QUEUE_CAPACITY> >;

    class UdpServer {
    public:
        explicit UdpServer(
            std::string bind_ip,
            uint16_t bind_port,
            size_t graceful_shutdown_rate,
            std::shared_ptr<ControlPlaneServer> control_plane_server,
            std::shared_ptr<SessionManager> session_manager,
            std::shared_ptr<BlackListStorer> black_list_storer,
            std::shared_ptr<CdrWriter> cdr_writer
        );

        UdpServer(const UdpServer &) = delete;

        UdpServer(UdpServer &&) = delete;

        UdpServer &operator=(const UdpServer &) = delete;

        UdpServer &operator=(UdpServer &&) = delete;

        ~UdpServer();

        void start();

        void stop();

    private:
        void init_socket();

        void event_loop();

        void process_packets();

        void send_responses();

        //static std::string bcd_to_imsi(const uint8_t *bcd, std::size_t len);


        std::string _bind_ip;
        uint16_t _bind_port;
        size_t _graceful_shutdown_rate;

        int _udp_fd{-1};
        int _epoll_fd{-1};

        std::shared_ptr<ControlPlaneServer> _http;
        std::shared_ptr<SessionManager> _sessions;
        std::shared_ptr<BlackListStorer> _blacklist;
        std::shared_ptr<CdrWriter> _cdr;

        std::atomic<bool> _running{false};
        std::atomic<bool> _gracefull_shutting_down{false};

        PacketQueue _recv_queue;
        ResponseQueue _send_queue;

        std::thread _epoll_thread;
        std::thread _worker_thread;
        std::thread _sender_thread;

        static constexpr size_t MAX_EPOLL_EVENTS = 16;
        static constexpr size_t UDP_BUFFER_SIZE = 1024;
    };
}
