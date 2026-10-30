//
// Created by Aki on 9/18/26.
//

#ifndef CXX_REST_API_BASE_H
#define CXX_REST_API_BASE_H

#include <functional>
#include <string>

#include "http/http_parser.h"
#include "http/response.h"

namespace capi {
    struct Param {
        std::string_view name;
        std::string_view value;
    };

    using Handler = std::function<Response(
        const Request&,
        const std::vector<Param>&,
        const std::vector<Param>&
        )>;

    struct RequestParams {
        const Handler* handler = nullptr;
        std::vector<Param> path_params;
        std::vector<Param> query_params;
        std::string_view body;
    };

    struct Route {
        std::string path;
        HttpMethod method = HttpMethod::NONE;
        Handler handler;
    };
}

#endif //CXX_REST_API_BASE_H
