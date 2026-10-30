//
// Created by Aki on 9/27/26.
//

#include "template_storage.h"

capi::Response route_from_other_file(capi::Router<TestStorage>& router,
                                     const capi::Request& request) {
    return router.route(request);
}

std::optional<capi::CAPI<TestStorage>> create_test_server(uint16_t port) {
    return capi::CAPI<TestStorage>::create(port);
}
