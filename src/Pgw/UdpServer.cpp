#include "UdpServer.h"
#include <sys/epoll.h>

#include <utility>

Pgw::UdpServer::UdpServer(std::string bind_ip, const uint16_t bind_port, size_t graceful_shutdown_rate,
                          std::shared_ptr<ControlPlaneServer> control_plane_server,
                          std::shared_ptr<SessionManager> session_manager,
                          std::shared_ptr<BlackListStorer> black_list_storer,
                          std::shared_ptr<CdrWriter> cdr_writer) : _bind_ip(std::move(bind_ip)), _bind_port(bind_port),
                                                                   _graceful_shutdown_rate(graceful_shutdown_rate),
                                                                   _http(std::move(control_plane_server)),
                                                                   _sessions(std::move(session_manager)),
                                                                   _blacklist(std::move(black_list_storer)),
                                                                   _cdr(std::move(cdr_writer)) {
}


Pgw::UdpServer::~UdpServer() {
    stop();
}

void Pgw::UdpServer::start() {
    if (_running.exchange(true)) {
        return;
    }
    init_socket();

    _epoll_thread = std::thread(&Pgw::UdpServer::event_loop, this);
    _sender_thread = std::thread(&Pgw::UdpServer::send_responses, this);
    _worker_thread = std::thread(&Pgw::UdpServer::process_packets, this);

    spdlog::info("Pgw::UdpServer started with: ip {}, port {}", _bind_ip, _bind_port);
}

void Pgw::UdpServer::stop() {
    if (!_running.exchange(false)) {
        return;
    }
    _gracefull_shutting_down = true;
    spdlog::info("UdpServer stopping...");
    if (_epoll_thread.joinable()) _epoll_thread.join();
    if (_sender_thread.joinable()) _sender_thread.join();
    if (_worker_thread.joinable()) _worker_thread.join();

    if (_udp_fd == -1) close(_udp_fd);
    if (_epoll_fd != -1) close(_epoll_fd);

    spdlog::info("UdpServer stopped.");
}

void Pgw::UdpServer::init_socket() {
    _udp_fd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    if (_udp_fd == -1) {
        throw std::runtime_error("Failed to create UDP socket");
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_bind_port);
    addr.sin_addr.s_addr = inet_addr(_bind_ip.c_str());

    if (bind(_udp_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == -1) {
        throw std::runtime_error("Failed to bind UDP socket");
    }

    _epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (_epoll_fd == -1) {
        throw std::runtime_error("Failed to create epoll fd");
    }

    epoll_event event{};
    event.events = EPOLLIN;
    event.data.fd = _udp_fd;

    if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _udp_fd, &event) == -1) {
        throw std::runtime_error("Failed to add epoll event");
    }
}

void Pgw::UdpServer::event_loop() {
    epoll_event events[MAX_EPOLL_EVENTS];
    while (_running.load()) {
        const int num_events = epoll_wait(_epoll_fd, events, MAX_EPOLL_EVENTS, 0);
        if (num_events < 0) continue;
        for (int i = 0; i < num_events; i++) {
            if (events[i].data.fd == _udp_fd && events[i].events & EPOLLIN) {
                auto *packet = new UdpPacket;
                packet->addr_len = sizeof(packet->client_addr);
                const ssize_t len = recvfrom(_udp_fd, packet->bcd.data(), sizeof(packet->bcd.data()), MSG_DONTWAIT,
                                             reinterpret_cast<sockaddr *>(&packet->client_addr), &packet->addr_len);
                if (len <= 0) {
                    delete packet;
                    spdlog::info("UdpServer received an invalid UDP packet");
                    continue;
                }
                packet->data_len = static_cast<size_t>(len);
                while (!_recv_queue.push(packet)) {
                }
            }
        }
    }
}

void Pgw::UdpServer::process_packets() {
    while (_running.load() || !_recv_queue.empty()) {
        UdpPacket *packet{};
        std::string action;

        while (_recv_queue.pop(packet)) {
            //auto imsi = bcd_to_imsi(packet->bcd);
            //blacklist logic.
            //_blacklist->...
            std::string imsi;
            auto *response = new UdpResponse;
            response->response = action;
            response->client_addr = packet->client_addr;
            response->addr_len = packet->addr_len;
            while (!_send_queue.push(response)) {
            }
            //cdr_writer logic
            spdlog::info("IMSI {} and action {}", imsi, action);

            delete packet;
            delete response;
        }

        if (_gracefull_shutting_down.load()) {
            for (size_t i = 0; i < _graceful_shutdown_rate; i++) {
                //delete sessions logic;
            }
        }
    }
}

void Pgw::UdpServer::send_responses() {
    while (_running.load() || !_send_queue.empty()) {
        auto *response = new UdpResponse;
        while (_send_queue.pop(response)) {
            const ssize_t len = sendto(_udp_fd, response->response.c_str(), response->response.size(), MSG_DONTWAIT,
                                       reinterpret_cast<sockaddr *>(&response->client_addr), response->addr_len);
            if (len <= 0) {
                delete response;
                spdlog::info("UdpServer send response failed");
            }
        }
    }
}
