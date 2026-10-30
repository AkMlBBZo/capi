//
// Created by Aki on 9/8/26.
//

#ifndef CXX_REST_API_CAPI_H
#define CXX_REST_API_CAPI_H
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <sys/types.h>
#include <unordered_map>

#include "error.h"
#include "../router/router.h"

namespace capi {

template<class Storage>
class CAPI {
private:
    int epoll_fd_ = -1;
    int server_fd_ = -1;
    std::unordered_map<int, std::string> messages_;
    OpError last_error_;
    Router<Storage> router_;

    static void set_sigaction();

    void set_last_error(OpState state, OpCode code, int sys_errno, std::string message);

    int epoll_ctl_set(int op, int fd);
    int set_nonblock(int fd);

    ssize_t try_recv(int fd, char *buffer, size_t size);

    OpState on_accept();
    OpState on_recv(int fd);
    OpState content_processing(std::string_view header_view, int fd);
    OpState close_conn(int fd);
    OpState send_response(const Response& result, int fd);

public:
    explicit CAPI(uint16_t port);
    static std::optional<CAPI> create(uint16_t port);
    ~CAPI();

    Router<Storage> &router();

    CAPI(const CAPI&) = delete;
    CAPI& operator=(const CAPI&) = delete;
    CAPI(CAPI&& other) noexcept(std::is_nothrow_move_constructible_v<Router<Storage>>);
    CAPI& operator=(CAPI&& other) noexcept;

    void run();
    const OpError& last_error() const;
};

}

#endif //CXX_REST_API_CAPI_H
