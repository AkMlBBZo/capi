//
// Created by Aki on 9/27/26.
//

#include <gtest/gtest.h>
#include <type_traits>
#include <utility>

#include "template_storage.h"
#include "router/storage/linear_storage.h"

using Server = capi::CAPI<TestStorage>;
static_assert(std::is_same_v<decltype(std::declval<Server&>().router()),
                             capi::Router<TestStorage>&>);
static_assert(std::is_same_v<decltype(Server::create(0)), std::optional<Server>>);
static_assert(!std::is_copy_constructible_v<Server>);
static_assert(std::is_move_constructible_v<Server>);

TEST(Templates, ExplicitInstantiationLinksAcrossTranslationUnits) {
    capi::Router<TestStorage> router;
    ASSERT_TRUE(router.add_route({"/counter", capi::HttpMethod::GET,
        [count = 0](auto&, auto&, const auto& query) mutable {
            EXPECT_EQ(query.size(), 1u);
            if (query.size() == 1) {
                EXPECT_EQ(query[0].value, "a b");
            }
            return std::to_string(++count);
        }}));
    capi::Request request;
    request.valid = true;
    request.method = capi::HttpMethod::GET;
    request.path = "/counter";
    request.query = "q=a+b";
    EXPECT_EQ(router.route(request).body, "1");
    EXPECT_EQ(route_from_other_file(router, request).body, "2");
    EXPECT_EQ(router.route(request).body, "3");
}

TEST(Templates, MovingRouterPreservesRoutesAndHandlerState) {
    capi::Router<TestStorage> source;
    ASSERT_TRUE(source.add_route({"/counter", capi::HttpMethod::GET,
        [count = 0](auto&, auto&, auto&) mutable { return std::to_string(++count); }}));
    capi::Request request;
    request.valid = true;
    request.method = capi::HttpMethod::GET;
    request.path = "/counter";
    EXPECT_EQ(source.route(request).body, "1");
    capi::Router<TestStorage> moved(std::move(source));
    EXPECT_EQ(route_from_other_file(moved, request).body, "2");
}
