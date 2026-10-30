//
// Created by Aki on 9/27/26.
//

#ifndef CXX_REST_API_TEST_TEMPLATE_STORAGE_H
#define CXX_REST_API_TEST_TEMPLATE_STORAGE_H

#include "core/CAPI.h"

#include "router/storage/linear_storage.h"

using TestStorage = capi::LinearStorage;

capi::Response route_from_other_file(capi::Router<TestStorage>& router,
                                     const capi::Request& request);
std::optional<capi::CAPI<TestStorage>> create_test_server(uint16_t port);

#endif
