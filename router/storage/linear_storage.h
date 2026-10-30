//
// Created by Aki on 9/14/26.
//

#ifndef CXX_REST_API_LINEAR_STORAGE_H
#define CXX_REST_API_LINEAR_STORAGE_H

#include "base.h"

namespace capi {

class LinearStorage {
private:
    std::vector<Route> routes_;

    static bool parse_route(std::string_view path_sys, std::string_view path_us, RequestParams& params);

public:
    explicit LinearStorage() = default;

    bool add_route(const Route& route);

    [[nodiscard]] RequestParams get_route(const Request& req) const;

    ~LinearStorage() = default;
};

}

#endif //CXX_REST_API_LINEAR_STORAGE_H
