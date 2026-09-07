#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <unordered_map>

#include "http/http_parser.h"

constexpr bool is_debug = true;
void log(const std::string& message) {
    if (is_debug) {
        std::cerr << message << std::endl;
    }
}

int epoll_ctl_set(int epoll_fd, int op, int fd);
int set_nonblock(int fd);
void close_connection(std::unordered_map<int, std::string>& messages, int fd, int epoll_fd);
int accept_connection(int server, std::unordered_map<int, std::string> &messages, int epoll_fd);
ssize_t send_response(const std::string& body, int fd);
int recv_processing(std::unordered_map<int, std::string> &messages, int epoll_fd, int fd);

int main() {
    const int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == -1) {
        return 1;
    }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8091);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int one = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    if (bind(server, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
        return 1;
    }
    if (listen(server, 512) == -1) {
        return 1;
    }

    if (set_nonblock(server)) {
        return 1;
    }

    const int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        return 1;
    }

    if (epoll_ctl_set(epoll_fd, EPOLL_CTL_ADD, server)) {
        return 1;
    }

    epoll_event events[64];

    std::unordered_map<int, std::string> messages;

    while (true) {
        int n = epoll_wait(epoll_fd, events, 64, -1);
        if (n == -1) {
            if (errno == EINTR) continue;
            return 1;
        }
        for (int i = 0; i < n; ++i) {
            if (events[i].data.fd == server) {
                if (accept_connection(server, messages, epoll_fd) == 1) return 1;
            } else {
                if (recv_processing(messages, epoll_fd, events[i].data.fd) == 1) return 1;
            }
        }
    }

    return 0;
}

int epoll_ctl_set(int epoll_fd, int op, int fd) {
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    return epoll_ctl(epoll_fd, op, fd, &ev);
}

int set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        return 1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        return 1;
    }
    return 0;
}

void close_connection(std::unordered_map<int, std::string> &messages, int fd, int epoll_fd) {
    epoll_ctl_set(epoll_fd, EPOLL_CTL_DEL, fd);
    close(fd);
    messages.erase(fd);
}

/**
 * @return  1 - epoll_ctl error
 * @return  0 - ok
 * @return -1 - set_nonblock/epoll_ctl_set err
 */
int accept_connection(int server, std::unordered_map<int, std::string> &messages, int epoll_fd) {
    sockaddr_in client_addr{};
    socklen_t len = sizeof(client_addr);
    const int client_fd = accept(server,
        reinterpret_cast<struct sockaddr*>(&client_addr), &len);
    if (client_fd == -1) return -1;

    if (set_nonblock(client_fd)) {
        std::cerr << "set_nonblock(client) failed: " << strerror(errno) << std::endl;
        close_connection(messages, client_fd, epoll_fd);
        return -1;
    }

    if (epoll_ctl_set(epoll_fd, EPOLL_CTL_ADD, client_fd)) {
        return 1;
    }

    messages[client_fd] = "";

    log("\tNew client: " + std::to_string(client_fd));

    return 0;
}

// TODO: other responses
ssize_t send_response(const std::string& body, int fd) {
    std::string response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: " + std::to_string(body.size()) + "\r\n"
                "Connection: close\r\n"
                "\r\n" + body;
    return send(fd, response.data(), response.size(), MSG_NOSIGNAL);
}

/**
 * TODO: add read function (instead of recv)
 * @return 0 - ok
 * @return 1 - message shorter
 * @return -1 - recv error
 */
int content_processing(std::string_view header, std::string& message, int fd) {
    http_parser http;
    http.parse_header(header);

    log("\t\tHEADER:\n\n" + std::string(header) + "\n");

    auto opt_len = http.content_length();
    if (!opt_len) {
        send_response("empty body\n", fd);
        return 0;
    }
    std::size_t rem = opt_len.value() - message.size();
    char buffer[4096] = {};
    while (rem > 0) {
        std::size_t want = std::min(rem, sizeof(buffer));
        ssize_t buffer_size = recv(fd, buffer, want, 0);
        if (buffer_size == 0) {

            log("Content processing: message_length is " +
                std::to_string(message.size()) + " need " +
                std::to_string(message.size() + rem));

            return 1;
            // TODO: <ERROR> message shorter than need
        } else if (buffer_size < 0) {
            return -1;
            // TODO: <ERROR> recv error
        }
        message.append(buffer, buffer_size);
        rem -= static_cast<std::size_t>(buffer_size);
    }

    if (send_response(std::to_string(message.size()) + "\n", fd) == -1) {
        std::cerr << "send failed: " << strerror(errno) << std::endl;
    }
    return 0;
}

int recv_processing(std::unordered_map<int, std::string> &messages, int epoll_fd, int fd) {
    auto& message = messages[fd];
    char buffer[4096] = {};
    ssize_t buffer_size;

    while ((buffer_size = recv(fd, buffer, sizeof(buffer), 0)) > 0) {
        message.append(buffer, buffer_size);

        auto pos = message.find("\r\n\r\n");
        if (pos != std::string::npos) {
            size_t first_crlf = message.find("\r\n");
            std::string_view header_view;
            if (first_crlf != std::string::npos && first_crlf < pos) {
                header_view = std::string_view(message).substr(first_crlf + 2, pos - first_crlf - 2);
            }
            message.erase(0, pos + 4);

            log("\tClient processing: " + std::to_string(content_processing(header_view, message, fd)));

            close_connection(messages, fd, epoll_fd);

            break;
        }
    }

    if (buffer_size == 0
        || (buffer_size == -1
            && errno != EAGAIN && errno != EWOULDBLOCK)) {
        close_connection(messages, fd, epoll_fd);
    }
    return 0;
}