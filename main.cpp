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

int main() {
    const int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == -1) {
        return 1;
    }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8091);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
        return 1;
    }
    if (listen(server, 512) == -1) {
        return 1;
    }

    int flags = fcntl(server, F_GETFL, 0);
    if (flags == -1) {
        return 1;
    }
    if (fcntl(server, F_SETFL, flags | O_NONBLOCK) == -1) {
        return 1;
    }

    const int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        return 1;
    }

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = server;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server, &ev) == -1) {
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
                sockaddr_in client_addr{};
                socklen_t len = sizeof(client_addr);
                const int client_fd = accept(server,
                    reinterpret_cast<struct sockaddr*>(&client_addr), &len);
                if (client_fd == -1) continue;

                int cflags = fcntl(client_fd, F_GETFL, 0);
                fcntl(client_fd, F_SETFL, cflags | O_NONBLOCK);

                epoll_event cev{};
                cev.events = EPOLLIN;
                cev.data.fd = client_fd;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &cev);

                messages[client_fd] = "";
            } else {
                auto& message = messages[events[i].data.fd];
                char buffer[4096] = {};
                ssize_t buffer_size;

                while ((buffer_size = recv(events[i].data.fd, buffer, sizeof(buffer), 0)) > 0) {
                    message.append(buffer, buffer_size);

                    auto pos = message.find("\r\n\r\n");
                    if (pos != std::string::npos) {
                        // TODO: Content-Length
                        std::string body = std::to_string(pos) + "\n";

                        std::string response =
                            "HTTP/1.1 200 OK\r\n"
                            "Content-Type: text/plain\r\n"
                            "Content-Length: " + std::to_string(body.size()) + "\r\n"
                            "Connection: close\r\n"
                            "\r\n" + body;

                        if (send(events[i].data.fd, response.data(), response.size(), MSG_NOSIGNAL) == -1) {
                            std::cerr << "send failed: " << strerror(errno) << std::endl;
                        }

                        // TODO: create function
                        close(events[i].data.fd);
                        messages.erase(events[i].data.fd);
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, nullptr);

                        break;
                    }
                }

                bool closed = false;
                if (buffer_size == 0
                    || (buffer_size == -1
                        && errno != EAGAIN && errno != EWOULDBLOCK)) {
                    closed = true;
                }
                if (closed) {
                    close(events[i].data.fd);
                    messages.erase(events[i].data.fd);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL,
                        events[i].data.fd, nullptr);
                }
            }
        }
    }

    return 0;
}