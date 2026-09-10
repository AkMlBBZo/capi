//
// Created by Aki on 9/10/26.
//

#include "router.h"

void capi::Router::add_route(const Route &route) {
    routes_.push_back(route);
}

std::string capi::Router::route(const Request &request) {
    if (!request.valid)
        return "Bad Response\n";
    for (const auto& route : routes_) {
        if (route.path == request.path) {
            return route.handler(request);
        }
    }
    return "Bad Response\n";
}
