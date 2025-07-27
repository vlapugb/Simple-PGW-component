#include "UdpClient.h"

#include <sys/epoll.h>
#include <unistd.h>

client::UdpClient::UdpClient (const Common::ClientSettings& settings)
: _server_ip (settings.server_ip), _server_port (settings.server_port),
  _logger (Common::make_file_logger (settings, "client_log", settings.log_file)) {
}

client::UdpClient::~UdpClient () {
    if (_udp_fd != -1) {
        close (_udp_fd);
    }
    if (_epoll_fd != -1) {
        close (_epoll_fd);
    }
}

void client::UdpClient::init_sockets () {
    _udp_fd = socket (AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port   = htons (_server_port);
    in_addr addr_in{};
    if (inet_aton (_server_ip.c_str (), &addr_in) == 0) {
        throw std::invalid_argument ("Bad server ip in config: " + _server_ip);
    }
    server.sin_addr = addr_in;
    if (connect (_udp_fd, reinterpret_cast<sockaddr*> (&server), sizeof (server)) == -1) {
        throw std::runtime_error ("Failed to connect UDP socket");
    }

    _epoll_fd = epoll_create1 (0);
    if (_epoll_fd == -1) {
        throw std::runtime_error ("Failed to create epoll descriptor");
    }
    epoll_event event{};
    event.events  = EPOLLIN;
    event.data.fd = _udp_fd;
    if (epoll_ctl (_epoll_fd, EPOLL_CTL_ADD, _udp_fd, &event) < 0) {
        throw std::runtime_error ("Failed to add epoll event");
    }

    _logger->info ("UDP socket created, ip {}, port {}", _server_ip, _server_port);
}

void client::UdpClient::send_imsi (const std::string& imsi_in) const {
    const auto bcd = imsi_to_bcd (imsi_in);
    if (const ssize_t sent = send (_udp_fd, bcd.data (), bcd.size (), 0); sent < 0) {
        throw std::runtime_error ("Failed to send imsi request");
    }
    _logger->info ("Send to server IMSI {} ", imsi_in);
}

std::string client::UdpClient::receive () const {
    std::array<epoll_event, MAX_EVENTS_EPOLL> events{};

    const int number_events = epoll_wait (_epoll_fd, events.data (), static_cast<int> (events.size ()), 100);
    if (number_events <= 0) {
        throw std::runtime_error ("Failed to wait on epoll_wait");
    }

    char buffer[MAX_BUFFER_SIZE]{};

    for (int i = 0; i < number_events; ++i) {
        if (events[i].data.fd == _udp_fd && (events[i].events & EPOLLIN)) {
            ssize_t received = recv (_udp_fd, buffer, MAX_BUFFER_SIZE, 0);
            if (received <= 0) {
                throw std::runtime_error ("Failed to receive message from server");
            }
            std::string response (buffer, static_cast<size_t> (received));
            _logger->info ("Received {} bytes from server", received);
            return response;
        }
    }
    throw std::runtime_error ("No data events received from server");
}
