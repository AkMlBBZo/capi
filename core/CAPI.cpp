//
// Created by Aki on 9/8/26.
//

#include "CAPI.h"
#include "../http/http_parser.h"

#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/epoll.h>

void capi::CAPI::set_last_error(OpState state, OpCode code, int sys_errno, std::string message) {
    std::cout << message << std::endl;
    last_error_ = {
        state,
        code,
        sys_errno,
        std::move(message)
    };
}

int capi::CAPI::epoll_ctl_set(int op, int fd) {
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    int res = epoll_ctl(epoll_fd_, op, fd, &ev);
    if (res) {
        set_last_error(OpState::FATAL, OpCode::EPOLL_CTL_FAILED, errno,
            "epoll ctl op " + std::to_string(op) + " failed for fd " + std::to_string(fd));
    }
    return res;
}

int capi::CAPI::set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        set_last_error(OpState::WARN, OpCode::SET_NONBLOCK_FAILED, errno,
            "fcntl F_GETFL failed for fd " + std::to_string(fd));
        return 1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        set_last_error(OpState::WARN, OpCode::SET_NONBLOCK_FAILED, errno,
            "fcntl F_SETFL failed for fd " + std::to_string(fd));
        return 1;
    }
    return 0;
}

ssize_t capi::CAPI::readn(int fd, char *buffer, size_t size) {
    ssize_t n = recv(fd, buffer, size, 0);
    if (n >= 0) return n;

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        set_last_error(OpState::WARN, OpCode::RECV_EAGAIN, errno,
            "recv EAGAIN for fd " + std::to_string(fd));
    } else {
        set_last_error(OpState::WARN, OpCode::RECV_CONN_RESET, errno,
            "recv failed for fd " + std::to_string(fd));
    }
    return -1;
}

capi::OpState capi::CAPI::on_accept() {
    sockaddr_in client_addr{};
    socklen_t len = sizeof(client_addr);
    const int client_fd = accept(server_fd_, reinterpret_cast<struct sockaddr*>(&client_addr), &len);
    if (client_fd == -1) {
        if (errno == EAGAIN) {
            set_last_error(OpState::WARN, OpCode::ACCEPT_EAGAIN, errno, "accept: would block");
        } else {
            set_last_error(OpState::WARN, OpCode::ACCEPT_CONN_ABORTED, errno, "accept: connection aborted");
        }
        return OpState::WARN;
    }

    if (set_nonblock(client_fd)) {
        ::close(client_fd);
        return OpState::WARN;
    }

    if (epoll_ctl_set(EPOLL_CTL_ADD, client_fd)) {
        ::close(client_fd);
        return OpState::FATAL;
    }

    messages_[client_fd] = "";

    return OpState::OK;
}

capi::OpState capi::CAPI::on_recv(int fd) {
    auto& message = messages_[fd];
    char buffer[4096];

    while (true) {
        ssize_t buffer_size = readn(fd, buffer, sizeof(buffer));
        if (buffer_size < 0) {
            return (last_error_.code == OpCode::RECV_EAGAIN) ? OpState::OK : OpState::WARN;
        }
        if (buffer_size == 0) {
            close_conn(fd);
            return OpState::OK;
        }
        message.append(buffer, buffer_size);

        auto pos = message.find("\r\n\r\n");
        if (pos != std::string::npos) {
            size_t first_crlf = message.find("\r\n");
            std::string_view header_view;
            if (first_crlf != std::string::npos && first_crlf < pos) {
                header_view = std::string_view(message).substr(first_crlf + 2, pos - first_crlf - 2);
            }
            message.erase(0, pos + 4);

            OpState res = content_processing(header_view, fd);

            close_conn(fd);

            return res;
        }
    }
    return OpState::OK;
}

capi::OpState capi::CAPI::content_processing(std::string_view header, int fd) {
    http_parser http;
    http.parse_header(header);

    auto opt_len = http.content_length();
    if (!opt_len) {
        return send_response("empty body\n", fd);
    }

    auto& message = messages_[fd];

    std::size_t rem = opt_len.value() - message.size();
    char buffer[4096];
    while (rem > 0) {
        std::size_t want = std::min(rem, sizeof(buffer));
        ssize_t buffer_size = readn(fd, buffer, want);
        if (buffer_size == 0) {
            set_last_error(OpState::WARN, OpCode::BODY_INCOMPLETE, errno,
                "content processing: message_length is " +
                std::to_string(message.size()) + " need " +
                std::to_string(message.size() + rem) +
                " for fd " + std::to_string(fd));
            return OpState::WARN;
        } else if (buffer_size < 0) {
            return OpState::WARN;
        }
        message.append(buffer, buffer_size);
        rem -= static_cast<std::size_t>(buffer_size);
    }

    return send_response(std::to_string(message.size()) + "\n", fd);
}

capi::OpState capi::CAPI::close_conn(int fd) {
    epoll_ctl_set(EPOLL_CTL_DEL, fd);
    close(fd);
    messages_.erase(fd);
    return OpState::OK;
}

// TODO: other responses
capi::OpState capi::CAPI::send_response(const std::string &body, int fd) {
    std::string response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: " + std::to_string(body.size()) + "\r\n"
                "Connection: close\r\n"
                "\r\n" + body;
    if (send(fd, response.data(), response.size(), MSG_NOSIGNAL) == -1) {
        set_last_error(OpState::WARN, OpCode::SEND_FAILED, errno, "send: send failed");
        return OpState::WARN;
    }
    return OpState::OK;
}

capi::CAPI::CAPI(uint16_t port) {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ == -1) {
        throw std::runtime_error("socket failed: " + std::string(strerror(errno)));
    }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int one = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    if (bind(server_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
        ::close(server_fd_);
        throw std::runtime_error("bind failed: " + std::string(strerror(errno)));
    }

    if (listen(server_fd_, 512) == -1) {
        ::close(server_fd_);
        throw std::runtime_error("listen failed: " + std::string(strerror(errno)));
    }

    if (set_nonblock(server_fd_)) {
        ::close(server_fd_);
        throw std::runtime_error("set_nonblock(server) failed: " + last_error_.msg);
    }

    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ == -1) {
        ::close(server_fd_);
        throw std::runtime_error("epoll_create1 failed: " + std::string(strerror(errno)));
    }

    if (epoll_ctl_set(EPOLL_CTL_ADD, server_fd_)) {
        ::close(server_fd_);
        ::close(epoll_fd_);
        throw std::runtime_error("epoll_ctl ADD server failed: " + last_error_.msg);
    }
}

std::optional<capi::CAPI> capi::CAPI::create(uint16_t port) {
    try {
        return CAPI(port);
    } catch (const std::exception& e) {
        std::cerr << "FATAL: " << e.what() << std::endl;
        return std::nullopt;
    }
}

capi::CAPI::~CAPI() {
    if (server_fd_ != -1)
        ::close(server_fd_);
    if (epoll_fd_ != -1)
        ::close(epoll_fd_);
    messages_.clear();
}

capi::CAPI::CAPI(CAPI&& other) noexcept
    : epoll_fd_(other.epoll_fd_),
      server_fd_(other.server_fd_),
      messages_(std::move(other.messages_)),
      last_error_(std::move(other.last_error_)) {
    other.epoll_fd_ = -1;
    other.server_fd_ = -1;
}

void capi::CAPI::run() {
    epoll_event events[64];

    while (true) {
        int n = epoll_wait(epoll_fd_, events, 64, -1);
        if (n == -1) {
            if (errno == EINTR) continue;
            return;
        }
        for (int i = 0; i < n; ++i) {
            if (events[i].data.fd == server_fd_) {
                if (on_accept() == OpState::FATAL)
                    return;
            } else {
                if (on_recv(events[i].data.fd) == OpState::FATAL)
                    return;
            }
        }
    }
}

const capi::OpError & capi::CAPI::last_error() const {
    return last_error_;
}
