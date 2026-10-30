//
// Created by Aki on 9/27/26.
//

#include "response.h"

namespace {
    const char* status_line(capi::ResponseStatus status) {
        switch (status) {
            case capi::ResponseStatus::Success: return "HTTP/1.1 200 OK\r\n";
            case capi::ResponseStatus::NotFound: return "HTTP/1.1 404 Not Found\r\n";
            case capi::ResponseStatus::Error: return "HTTP/1.1 500 Internal Server Error\r\n";
        }
        return "HTTP/1.1 500 Internal Server Error\r\n";
    }
}

capi::Response::Response(std::string body) : body(std::move(body)) {}

capi::Response::Response(const char *body) : body(body) {}

capi::Response::Response(ResponseStatus status, std::string body) : status(status), body(std::move(body)) {}

std::string capi::Response::to_http() const {
    return std::string(status_line(status)) +
           "Content-Type: text/plain\r\n"
           "Content-Length: " + std::to_string(body.size()) + "\r\n"
           "Connection: close\r\n"
           "\r\n" + body;
}
