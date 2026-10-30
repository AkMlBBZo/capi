//
// Created by Aki on 9/10/26.
//

#ifndef CXX_REST_API_ROUTER_H
#define CXX_REST_API_ROUTER_H

#include <string>

#include "http/http_parser.h"
#include "router/storage/base.h"

namespace capi {

template<class Storage>
class Router {
private:
    Storage storage_;

    static bool parse_query_params(std::string& query, RequestParams& params);

public:
    explicit Router() = default;

    bool add_route(const Route& route);

    Response route(const Request& request);
};

}

#endif //CXX_REST_API_ROUTER_H
