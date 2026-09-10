//
// Created by Aki on 9/10/26.
//

#ifndef CXX_REST_API_ROUTER_H
#define CXX_REST_API_ROUTER_H

#include <functional>
#include <vector>
#include <string>

#include "../http/http_headers.h"
#include "../http/http_parser.h"

namespace capi {

using Handler = std::function<std::string(const Request&)>;

struct Route {
    std::string path;
    HttpMethod method = HttpMethod::NONE;
    Handler handler;
};

class Router {
private:
    std::vector<Route> routes_;

public:
    void add_route(const Route& route);

    std::string route(const Request& request);
};

}

#endif //CXX_REST_API_ROUTER_H
