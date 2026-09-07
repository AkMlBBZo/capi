//
// Created by Aki on 9/5/26.
//

#ifndef CXX_REST_API_HTTP_PARSER_H
#define CXX_REST_API_HTTP_PARSER_H
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class http_parser {
public:
    struct Header {
        std::string name;
        std::string value;
    };

    std::vector<Header> headers;

    bool parse_header(std::string_view header_view);

    std::optional<std::string> get_value(const std::string& header_name) const;

    std::optional<std::size_t> content_length() const;
};



#endif //CXX_REST_API_HTTP_PARSER_H