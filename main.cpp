#include <iostream>
#include <ostream>

#include "core/CAPI.h"

void add_route(capi::Router& router, std::string path, capi::Handler handler) {
    capi::Route route;

    route.method = capi::HttpMethod::GET;
    route.path = path;
    route.handler = handler;

    router.add_route(route);
}

int main() {
    auto opt_server = capi::CAPI::create(8091);
    if (!opt_server) {
        std::cout << "Failed to create CAPI" << std::endl;
        return 0;
    }
    capi::CAPI& server = *opt_server;

    add_route(server.router(), "/rand",
        [](const capi::Request& req){return std::to_string(rand());});
    add_route(server.router(), "/",
        [](const capi::Request& req){return "Hello world\n";});
    add_route(server.router(), "/one",
        [](const capi::Request& req){return std::to_string(1);});
    add_route(server.router(), "/uwu",
        [](const capi::Request& req){return "uwu\nuwu\nuwu\n";});

    server.run();
    return 0;
}
