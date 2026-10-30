//
// Created by Aki on 9/27/26.
//

#ifndef CXX_REST_API_RESPONSE_H
#define CXX_REST_API_RESPONSE_H

#include <cstdint>
#include <string>
#include <vector>

namespace capi {

enum class ResponseStatus : std::uint16_t {
    Success = 200,
    NotFound = 404,
    Error = 500
};

struct Response {

    ResponseStatus status = ResponseStatus::Success;

    std::string body;

    Response() = default;
    Response(std::string body);
    Response(const char* body);
    Response(ResponseStatus status, std::string body);

    [[nodiscard]] std::string to_http() const;
};

}

#endif //CXX_REST_API_RESPONSE_H
