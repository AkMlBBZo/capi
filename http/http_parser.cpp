#include "http_parser.h"

#include <charconv>
#include <cctype>

namespace capi {

namespace {
    inline unsigned char uchar(const char c) {
        return static_cast<unsigned char>(c);
    }

    inline std::string_view trim(std::string_view sv) {
        while (!sv.empty() && std::isspace(uchar(sv.front())))
            sv.remove_prefix(1);
        while (!sv.empty() && std::isspace(uchar(sv.back())))
            sv.remove_suffix(1);
        return sv;
    }
}

HttpMethod http_parser::parse_method(std::string_view token) {
    if (token == "GET")     return HttpMethod::GET;
    if (token == "POST")    return HttpMethod::POST;
    if (token == "PUT")     return HttpMethod::PUT;
    if (token == "DELETE")  return HttpMethod::DELETE;
    if (token == "PATCH")   return HttpMethod::PATCH;
    if (token == "HEAD")    return HttpMethod::HEAD;
    if (token == "OPTIONS") return HttpMethod::OPTIONS;
    return HttpMethod::NONE;
}

HttpVersion http_parser::parse_version(std::string_view token) {
    token = trim(token);

    if (token.size() < 8 || token.substr(0, 5) != "HTTP/")
        return HttpVersion::UNKNOWN;

    auto dot = token.find('.', 5);
    if (dot == std::string_view::npos)
        return HttpVersion::UNKNOWN;

    std::string_view major_sv = token.substr(5, dot - 5);
    std::string_view minor_sv = token.substr(dot + 1);
    unsigned major = 0, minor = 0;
    auto [p1, e1] = std::from_chars(major_sv.data(), major_sv.data() + major_sv.size(), major);
    auto [p2, e2] = std::from_chars(minor_sv.data(), minor_sv.data() + minor_sv.size(), minor);

    if (e1 != std::errc{} || e2 != std::errc{}) return HttpVersion::UNKNOWN;
    if (p1 != major_sv.data() + major_sv.size()) return HttpVersion::UNKNOWN;
    if (p2 != minor_sv.data() + minor_sv.size()) return HttpVersion::UNKNOWN;

    if (major == 1 && minor == 0) return HttpVersion::HTTP_1_0;
    if (major == 1 && minor == 1) return HttpVersion::HTTP_1_1;
    return HttpVersion::UNKNOWN;
}

bool http_parser::parse_firstline(std::string_view line) {
    line = trim(line);
    if (line.empty()) return false;

    std::size_t sp1 = line.find(' ');
    if (sp1 == std::string_view::npos || sp1 == 0) return false;
    request.method = parse_method(line.substr(0, sp1));

    auto rest = line.substr(sp1 + 1);
    auto sp2 = rest.find(' ');
    if (sp2 == std::string_view::npos || sp2 == 0) return false;

    std::string_view target = rest.substr(0, sp2);

    auto qpos = target.find('?');
    if (qpos == std::string_view::npos) {
        request.path = target;
        request.query = {};
    } else {
        request.path = target.substr(0, qpos);
        request.query = target.substr(qpos + 1);
    }

    request.version = parse_version(rest.substr(sp2 + 1));

    if (request.method == HttpMethod::NONE) return false;
    if (request.version == HttpVersion::UNKNOWN) return false;
    if (request.path.empty()) return false;

    request.valid = true;
    return true;
}

bool http_parser::parse_header(std::string_view header_view) {
    auto pos = header_view.find("\r\n");
    if (pos == std::string_view::npos) return false;

    std::string_view line = header_view.substr(0, pos);
    if (!parse_firstline(line))
        return false;

    header_view = header_view.substr(pos + 2);

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

bool http_parser::set_body(std::string_view body) {
    request.body = body;
    return true;
}

std::optional<std::string> http_parser::get_value(const std::string& header_name) const {
    std::string local_name;
    local_name.reserve(header_name.size());
    for (const auto c : header_name) local_name.push_back(std::tolower(uchar(c)));

    for (const auto& [name, value] : headers) {
        if (name == local_name) return value;
    }
    return std::nullopt;
}

std::optional<std::size_t> http_parser::content_length() const {
    auto value = get_value("content-length");
    if (!value || value->empty()) return std::nullopt;
    auto& s = *value;
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

}
