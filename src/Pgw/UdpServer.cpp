#include "UdpServer.h"

#include <sys/epoll.h>
#include <unistd.h>

#include "BlackListStorer.h"
#include "CdrWriter.h"
#include "ControlPlaneServer.h"
#include "SessionManager.h"
#include "spdlog/sinks/stdout_color_sinks.h"

using namespace Pgw;

UdpServer::UdpServer (const Common::ServerSettings& settings,
std::shared_ptr<ControlPlaneServer> control_plane_server,
std::shared_ptr<SessionManager> session_manager,
std::shared_ptr<BlackListStorer> black_list_storer,
std::shared_ptr<CdrWriter> cdr_writer)
: _bind_ip (settings.udp_ip), _bind_port (settings.udp_port), _graceful_shutdown_rate (settings.graceful_shutdown_rate),
  _http (std::move (control_plane_server)), _sessions (std::move (session_manager)),
  _blacklist (std::move (black_list_storer)), _cdr (std::move (cdr_writer)),
  _logger (Common::make_file_logger (settings, "server_log", settings.log_file)) {
    _blacklist->store (settings.blacklist);
}

UdpServer::~UdpServer () {
    stop ();
}

void UdpServer::start () {
    _running.store (true);

    init_socket ();
    _timeout_thread = std::thread ([this] {
        while (_running) {
            _sessions->remove_timeout ();
            std::this_thread::sleep_for (std::chrono::seconds (1));
        }
    });
    _epoll_thread   = std::thread (&UdpServer::event_loop, this);
    _worker_thread  = std::thread (&UdpServer::process_packets, this);
    _sender_thread  = std::thread (&UdpServer::send_responses, this);

    _logger->info ("UDP‑Server started on {}:{}", _bind_ip, _bind_port);
}

void UdpServer::stop () {
    _running.store (false);

    _logger->info ("UDP‑Server stopping…");

    const auto this_id = std::this_thread::get_id ();

    if (_epoll_thread.joinable ())
        _epoll_thread.join ();
    if (_worker_thread.joinable ())
        _worker_thread.join ();
    if (_sender_thread.joinable ())
        _sender_thread.join ();
    if (_timeout_thread.joinable ())
        _timeout_thread.join ();
    if (_offload_thread.joinable () && _offload_thread.get_id () != this_id)
        _offload_thread.join ();

    if (_udp_fd != -1) {
        close (_udp_fd);
        _udp_fd = -1;
    }
    if (_epoll_fd != -1) {
        close (_epoll_fd);
        _epoll_fd = -1;
    }

    _logger->info ("UDP‑Server stopped.");
}


void UdpServer::initiate_graceful_shutdown () {
    if (_graceful.exchange (true))
        return;

    _logger->info ("Graceful shutdown requested");
    _offload_thread = std::thread (&UdpServer::offload_sessions, this);
}

void UdpServer::init_socket () {
    _udp_fd = socket (AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    if (_udp_fd == -1)
        throw std::runtime_error ("can't create UDP socket");

    sockaddr_in addr{};
    in_addr addr_in{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons (_bind_port);
    if (inet_aton (_bind_ip.c_str (), &addr_in) == 0)
        throw std::invalid_argument ("bad udp_ip " + _bind_ip);
    addr.sin_addr = addr_in;

    if (bind (_udp_fd, reinterpret_cast<sockaddr*> (&addr), sizeof addr) == -1)
        throw std::runtime_error ("bind() failed");

    _epoll_fd = epoll_create1 (EPOLL_CLOEXEC);
    if (_epoll_fd == -1)
        throw std::runtime_error ("epoll_create1() failed");

    epoll_event ev{};
    ev.events  = EPOLLIN;
    ev.data.fd = _udp_fd;
    if (epoll_ctl (_epoll_fd, EPOLL_CTL_ADD, _udp_fd, &ev) == -1)
        throw std::runtime_error ("epoll_ctl() failed");
}

void UdpServer::event_loop () {
    epoll_event evs[MAX_EPOLL_EVENTS];

    while (_running) {
        const int n = epoll_wait (_epoll_fd, evs, MAX_EPOLL_EVENTS, 10);
        if (n <= 0)
            continue;

        for (int i = 0; i < n; ++i) {
            if (!(evs[i].events & EPOLLIN))
                continue;

            auto* pkt     = new UdpPacket;
            pkt->addr_len = sizeof (pkt->client_addr);

            const ssize_t len = recvfrom (_udp_fd, pkt->bcd.data (), pkt->bcd.size (), MSG_DONTWAIT,
            reinterpret_cast<sockaddr*> (&pkt->client_addr), &pkt->addr_len);
            if (len <= 0) {
                delete pkt;
                continue;
            }

            pkt->data_len = static_cast<std::size_t> (len);
            while (!_recv_queue.push (pkt)) {
            }
        }
    }
}

void UdpServer::process_packets () {
    while (_running) {
        UdpPacket* pkt{};
        if (!_recv_queue.pop (pkt)) {
            std::this_thread::sleep_for (std::chrono::milliseconds (1));
            continue;
        }

        const std::string imsi = bcd_to_imsi (pkt->bcd.data (), pkt->data_len);

        std::string action;
        if (_blacklist->is_in_blacklist (imsi)) {
            action = "rejected";
        } else if (_sessions->create_session (imsi)) {
            action = "created";
        } else {
            action = "exists";
        }
        if (action != "exists")
            _cdr->write (imsi, action);

        auto* rsp        = new UdpResponse;
        rsp->response    = action;
        rsp->client_addr = pkt->client_addr;
        rsp->addr_len    = pkt->addr_len;
        while (!_send_queue.push (rsp)) {
        }

        delete pkt;
    }
}

void UdpServer::send_responses () {
    while (_running || !_send_queue.empty ()) {
        UdpResponse* rsp{};
        if (!_send_queue.pop (rsp)) {
            std::this_thread::sleep_for (std::chrono::milliseconds (1));
            continue;
        }
        sendto (_udp_fd, rsp->response.c_str (), rsp->response.size (), MSG_DONTWAIT,
        reinterpret_cast<sockaddr*> (&rsp->client_addr), rsp->addr_len);
        delete rsp;
    }
}

void UdpServer::offload_sessions () {
    _logger->info ("Starting graceful offload… rate={} sess/s", _graceful_shutdown_rate);

    while (true) {
        std::size_t removed = 0;
        for (std::size_t i = 0; i < _graceful_shutdown_rate; ++i) {
            if (std::string victim; !_sessions->pop_one (victim))
                break;
            ++removed;
        }
        if (removed == 0)
            break;
        std::this_thread::sleep_for (std::chrono::seconds (1));
    }

    _logger->info ("Graceful offload finished, signalling main thread");
    _running.store (false);
}


std::string UdpServer::bcd_to_imsi (const uint8_t* data, std::size_t len) {
    std::string imsi;
    imsi.reserve (len * 2);

    for (std::size_t i = 0; i < len; ++i) {
        const uint8_t low  = data[i] & 0x0F;
        const uint8_t high = (data[i] >> 4) & 0x0F;
        if (low != 0x0F)
            imsi.push_back ('0' + low);
        if (high != 0x0F)
            imsi.push_back ('0' + high);
    }
    return imsi;
}
