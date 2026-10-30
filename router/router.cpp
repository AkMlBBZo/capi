//
// Created by Aki on 9/10/26.
//

#include "router.h"
#include "storage/linear_storage.h"

#include <charconv>

namespace {
    inline bool decode(std::string& query, std::string_view raw, std::string_view& decoded) {
        const std::size_t start = raw.data() - query.data();
        std::size_t size = 0;
        for (std::size_t i = 0; i < raw.size(); ++i) {
            char c = raw[i];
            if (c == '%') {

                if (raw.size() - i < 3)
                    return false;

                unsigned value = 0;
                const char* first = raw.data() + i + 1;
                const char* last = first + 2;
                auto [ptr, ec] = std::from_chars(first, last, value, 16);

                if (ec != std::errc{} || ptr != last)
                    return false;

                c = static_cast<char>(value);
                i += 2;
            } else if (c == '+') {
                c = ' ';
            }
            query[start + size++] = c;
        }
        decoded = std::string_view(query.data() + start, size);
        return true;
    }
}

template<class Storage>
bool capi::Router<Storage>::parse_query_params(std::string& query, RequestParams& params) {
    params.query_params.clear();
    std::string_view rest = query;
    while (!rest.empty()) {
        const auto amp = rest.find('&');
        const auto pair = rest.substr(0, amp);
        rest = amp == std::string_view::npos ? std::string_view{} : rest.substr(amp + 1);

        if (pair.empty())
            continue;

        const auto eq = pair.find('=');
        const auto name = pair.substr(0, eq);
        const auto value = pair.substr(eq == std::string_view::npos ? pair.size() : eq + 1);
        Param param{};

        if (!decode(query, name, param.name) || !decode(query, value, param.value)) {
            params.query_params.clear();
            return false;
        }

        params.query_params.push_back(param);
    }
    return true;
}

template<class Storage>
bool capi::Router<Storage>::add_route(const Route &route) {
    return storage_.add_route(route);
}

template<class Storage>
capi::Response capi::Router<Storage>::route(const Request &request) {
    if (!request.valid)
        return {ResponseStatus::Error, "Bad Response\n"};

    try {
        RequestParams requestParams = storage_.get_route(request);
        if (!requestParams.handler)
            return {ResponseStatus::NotFound, "404 Not Found\n"};

        std::string query(request.query);
        if (!parse_query_params(query, requestParams))
            return {ResponseStatus::Error, "Bad Response\n"};

        return (*requestParams.handler)(
                request,
                requestParams.path_params,
                requestParams.query_params
        );
    } catch (...) {
        return {ResponseStatus::Error, "Internal Server Error\n"};
    }
}

template class capi::Router<capi::LinearStorage>;
