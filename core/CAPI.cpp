//
// Created by Aki on 9/8/26.
//

#include "CAPI.h"

#include <csignal>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
#include <utility>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>

namespace {
    volatile sig_atomic_t g_running = 1;

    void on_signal(int /*signo*/) {
        g_running = 0;
    }
}

template<class Storage>
void capi::CAPI<Storage>::set_sigaction() {
    struct sigaction act{};
    act.sa_handler = on_signal;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGINT, &act, nullptr);
    sigaction(SIGTERM, &act, nullptr);
}

template<class Storage>
void capi::CAPI<Storage>::set_last_error(OpState state, OpCode code, int sys_errno, std::string message) {
    std::cerr << message << std::endl;
    last_error_ = {
        state,
        code,
        sys_errno,
        std::move(message)
    };
}

template<class Storage>
int capi::CAPI<Storage>::epoll_ctl_set(int op, int fd) {
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

template<class Storage>
int capi::CAPI<Storage>::set_nonblock(int fd) {
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

template<class Storage>
ssize_t capi::CAPI<Storage>::try_recv(int fd, char *buffer, size_t size) {
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

template<class Storage>
capi::OpState capi::CAPI<Storage>::on_accept() {
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

template<class Storage>
capi::OpState capi::CAPI<Storage>::on_recv(int fd) {
    auto& message = messages_[fd];
    char buffer[4096];

    while (true) {
        ssize_t buffer_size = try_recv(fd, buffer, sizeof(buffer));
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
            std::string header = message.substr(0, pos + 2);
            message.erase(0, pos + 4);

            OpState res = content_processing(header, fd);

            close_conn(fd);

            return res;
        }
    }
    return OpState::OK;
}

template<class Storage>
capi::OpState capi::CAPI<Storage>::content_processing(std::string_view header, int fd) {
    http_parser http;
    if (!http.parse_header(header))
        return send_response({ResponseStatus::Error, "Bad Response\n"}, fd);

    auto opt_len = http.content_length();
    if (!opt_len) {
        Response result = router_.route(http.request);

        return send_response(result, fd);
    }

    // TODO: move it to another function
    auto& message = messages_[fd];

    std::size_t rem = opt_len.value() - message.size();
    char buffer[4096];
    while (rem > 0) {
        std::size_t want = std::min(rem, sizeof(buffer));
        ssize_t buffer_size = try_recv(fd, buffer, want);
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

    http.set_body(message);

    Response result = router_.route(http.request);

    return send_response(result, fd);
}

template<class Storage>
capi::OpState capi::CAPI<Storage>::send_response(const Response &result, int fd) {
    std::string response = result.to_http();

    if (send(fd, response.data(), response.size(), MSG_NOSIGNAL) == -1) {
        set_last_error(OpState::WARN, OpCode::SEND_FAILED, errno, "send: send failed " + response);
        return OpState::WARN;
    }

    return OpState::OK;
}

template<class Storage>
capi::OpState capi::CAPI<Storage>::close_conn(int fd) {
    epoll_ctl_set(EPOLL_CTL_DEL, fd);
    close(fd);
    messages_.erase(fd);
    return OpState::OK;
}

template<class Storage>
capi::CAPI<Storage>::CAPI(uint16_t port) {
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

template<class Storage>
std::optional<capi::CAPI<Storage>> capi::CAPI<Storage>::create(uint16_t port) {
    try {
        return CAPI(port);
    } catch (const std::exception& e) {
        std::cerr << "FATAL: " << e.what() << std::endl;
        return std::nullopt;
    }
}

template<class Storage>
capi::CAPI<Storage>::~CAPI() {
    if (server_fd_ != -1)
        ::close(server_fd_);
    if (epoll_fd_ != -1)
        ::close(epoll_fd_);
    messages_.clear();
}

template<class Storage>
capi::Router<Storage> & capi::CAPI<Storage>::router() {
    return router_;
}

template<class Storage>
capi::CAPI<Storage>::CAPI(CAPI&& other) noexcept(std::is_nothrow_move_constructible_v<Router<Storage>>)
    : epoll_fd_(other.epoll_fd_),
      server_fd_(other.server_fd_),
      messages_(std::move(other.messages_)),
      last_error_(std::move(other.last_error_)),
      router_(std::move(other.router_)) {
    other.epoll_fd_ = -1;
    other.server_fd_ = -1;
}

template<class Storage>
void capi::CAPI<Storage>::run() {
    set_sigaction();
    epoll_event events[64];

    while (g_running) {
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

template<class Storage>
const capi::OpError & capi::CAPI<Storage>::last_error() const {
    return last_error_;
}

// TODO: move it to other file
#include "router/storage/linear_storage.h"
template class capi::CAPI<capi::LinearStorage>;