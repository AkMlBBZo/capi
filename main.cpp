#include <iostream>
#include <ostream>
#include <utility>

#include "core/CAPI.h"
#include "router/storage/linear_storage.h"

template<class Storage>
void add_route(capi::Router<Storage>& router, std::string path, capi::Handler handler) {
    capi::Route route;

    route.method = capi::HttpMethod::GET;
    route.path = std::move(path);
    route.handler = std::move(handler);

    router.add_route(route);
}

int main() {
    using Server = capi::CAPI<capi::LinearStorage>;
    auto opt_server = Server::create(8091);
    if (!opt_server) {
        std::cout << "Failed to create CAPI" << std::endl;
        return 0;
    }
    Server& server = *opt_server;

    add_route(server.router(), "/rand",
        [](auto&, auto&, auto&){return std::to_string(rand());});
    add_route(server.router(), "/",
        [](auto&, auto&, auto&){return "Hello world\n";});
    add_route(server.router(), "/one",
        [](auto&, auto&, auto&){return std::to_string(1);});
    add_route(server.router(), "/uwu",
        [](auto&, auto&, auto&){return "uwu\nuwu\nuwu\n";});
    add_route(server.router(), "/{id}/get_id",
    [](auto&, auto& pv, auto&) -> capi::Response {
        for (auto &v:pv) {
            if (v.name == "id") {
                return std::string(v.value) + "\n";
            }
        }
        return {capi::ResponseStatus::NotFound, "404 NotFound Your id\n"};
    });

    add_route(server.router(), "/{id}/",
        [](auto&, auto& pv, auto&) -> capi::Response {
            for (auto& v : pv) {
                if (v.name == "id") {
                    return std::string(v.value) + "--";
                }
            }
            return {capi::ResponseStatus::NotFound, "404 Not Found"};
    });

    add_route(server.router(), "/ppar1/{ppar1}/ppar2/{ppar2}", // /ppar1/{ppar1}/ppar2/{ppar2}?qpar1={qpar1}&qpar2={qpar2}
        [](const auto&, const auto& path_params, const auto& query_params) {
            std::string response;
            for (const auto& param : path_params) {
                response += "path." + std::string(param.name) + "=" +
                            std::string(param.value) + "\n";
            }
            for (const auto& param : query_params) {
                response += "query." + std::string(param.name) + "=" +
                            std::string(param.value) + "\n";
            }
            return response;
    });

    server.run();
    return 0;
}
