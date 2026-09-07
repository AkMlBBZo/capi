#include "http_parser.h"

#include <charconv>

namespace {
    inline unsigned char uchar(const char c) {
        return static_cast<unsigned char>(c);
    }
}

bool http_parser::parse_header(std::string_view header_view) {
    while (true) {
        const size_t pos = header_view.find("\r\n");
        if (pos == std::string_view::npos) break;

        std::string_view line = header_view.substr(0, pos);

        const size_t colon = line.find(':');
        if (colon == std::string_view::npos) return false;
        if (colon == 0) return false;

        Header header;

        std::string_view name_view = line.substr(0, colon);
        header.name.assign(name_view.data(), name_view.size());
        for (auto& c : header.name) c = std::tolower(uchar(c));

        size_t vs = colon + 1;
        while (vs < line.size() && std::isblank(uchar(line[vs]))) ++vs;
        size_t ve = line.size();
        while (ve > vs && std::isblank(uchar(line[ve-1]))) --ve;
        header.value.assign(line.data() + vs, ve - vs);

        headers.push_back(std::move(header));

        header_view = header_view.substr(pos + 2);
    }

    return true;
}

std::optional<std::string> http_parser::get_value(const std::string& header_name) const {
    std::string local_name;
    local_name.reserve(header_name.size());
    for (const auto c:header_name) local_name.push_back(std::tolower(uchar(c)));

    for (const auto&[name, value] : headers) {
        if (name == local_name) return value;
    }
    return std::nullopt;
}

std::optional<std::size_t> http_parser::content_length() const {
    auto value = get_value("content-length");
    if (!value || value->empty()) return std::nullopt;
    auto &s = *value;
    for (char c : s) {
        if (!isdigit(uchar(c))) return std::nullopt;
    }
    std::size_t result = 0;
    if (auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), result);
        ec != std::errc{} || ptr != s.data() + s.size()) {
        return std::nullopt;
    }
    return result;
}
