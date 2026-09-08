#include <iostream>
#include <ostream>

#include "core/CAPI.h"

int main() {
    auto opt_server = capi::CAPI::create(8091);
    if (!opt_server) {
        std::cout << "Failed to create CAPI" << std::endl;
        return 0;
    }
    capi::CAPI& server = *opt_server;
    server.run();
    return 0;
}
