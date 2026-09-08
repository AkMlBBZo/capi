//
// Created by Aki on 9/8/26.
//

#ifndef CXX_REST_API_ERROR_H
#define CXX_REST_API_ERROR_H
#include <string>

namespace capi {
    enum class OpState {
        OK, WARN, ERROR, FATAL
    };

    enum class OpCode {
        NONE,
        ACCEPT_EAGAIN,
        ACCEPT_CONN_ABORTED,
        RECV_EAGAIN,
        RECV_CONN_RESET,
        RECV_BAD_FD,
        SEND_FAILED,
        EPOLL_CTL_FAILED,
        SET_NONBLOCK_FAILED,
        BODY_INCOMPLETE
    };

    struct OpError {
        OpState state = OpState::OK;
        OpCode code = OpCode::NONE;
        int sys_errno = 0;
        std::string msg;
    };
}

#endif //CXX_REST_API_ERROR_H
