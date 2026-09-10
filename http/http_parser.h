//
// Created by Aki on 9/5/26.
//

#ifndef CXX_REST_API_HTTP_PARSER_H
#define CXX_REST_API_HTTP_PARSER_H
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "http_headers.h"

namespace capi {

struct Request {
    HttpMethod method = HttpMethod::NONE;
    std::string_view path;
    std::string_view query;
    HttpVersion version = HttpVersion::UNKNOWN;
    std::string_view body;
    bool valid = false;
};

class http_parser {
public:
    struct Header {
        std::string name;
        std::string value;
    };

    std::vector<Header> headers;
    Request request;

    bool parse_firstline(std::string_view line);
    bool parse_header(std::string_view header_view);
    bool set_body(std::string_view body);

    [[nodiscard]] std::optional<std::string> get_value(const std::string& header_name) const;
    [[nodiscard]] std::optional<std::size_t> content_length() const;

private:
    static HttpMethod parse_method(std::string_view token);
    static HttpVersion parse_version(std::string_view token);
};

} // namespace capi

#endif //CXX_REST_API_HTTP_PARSER_H
