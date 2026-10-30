//
// Created by Aki on 9/27/26.
//

#include <gtest/gtest.h>
#include <stdexcept>

#include "http/response.h"
#include "router/router.h"
#include "router/storage/linear_storage.h"

using namespace capi;

namespace {
    Request request_for(std::string_view path) {
        Request request;
        request.valid = true;
        request.method = HttpMethod::GET;
        request.path = path;
        return request;
    }
}

TEST(Response, SerializesAllSupportedStatuses) {
    for (auto status : {ResponseStatus::Success, ResponseStatus::NotFound, ResponseStatus::Error}) {
        const char* line = status == ResponseStatus::Success ? "HTTP/1.1 200 OK\r\n" :
                           status == ResponseStatus::NotFound ? "HTTP/1.1 404 Not Found\r\n" :
                                "HTTP/1.1 500 Internal Server Error\r\n";
        EXPECT_EQ((Response{status, "hello"}.to_http()),
                  std::string(line) + "Content-Type: text/plain\r\n"
                  "Content-Length: 5\r\nConnection: close\r\n\r\nhello");
    }
}

TEST(Response, SupportsEmptyAndBinaryBodies) {
    EXPECT_EQ(Response{}.to_http(), "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n"
                                   "Content-Length: 0\r\nConnection: close\r\n\r\n");
    const std::string body("\xD0\xAF\0x", 4);
    EXPECT_EQ((Response{ResponseStatus::Success, body}.to_http()),
              std::string("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n"
                          "Content-Length: 4\r\nConnection: close\r\n\r\n") + body);
}

TEST(Response, ConvertsExistingTextHandlersToSuccess) {
    Handler literal = [](auto&, auto&, auto&) { return "literal"; };
    Handler string = [](auto&, auto&, auto&) { return std::string("string"); };
    const auto request = request_for("/");
    const auto a = literal(request, {}, {});
    const auto b = string(request, {}, {});
    EXPECT_EQ(a.status, ResponseStatus::Success);
    EXPECT_EQ(a.body, "literal");
    EXPECT_EQ(b.status, ResponseStatus::Success);
    EXPECT_EQ(b.body, "string");
}

TEST(RouterResponse, PreservesExplicitHandlerStatusAndBody) {
    Router<LinearStorage> router;
    ASSERT_TRUE(router.add_route({"/", HttpMethod::GET, [](auto&, auto&, auto&) {
        return Response{ResponseStatus::NotFound, "User not found\n"};
    }}));
    const auto response = router.route(request_for("/"));
    EXPECT_EQ(response.status, ResponseStatus::NotFound);
    EXPECT_EQ(response.body, "User not found\n");
}

TEST(RouterResponse, MissingRouteReturnsNotFound) {
    Router<LinearStorage> router;
    const auto response = router.route(request_for("/missing"));
    EXPECT_EQ(response.status, ResponseStatus::NotFound);
    EXPECT_EQ(response.body, "404 Not Found\n");
}

TEST(RouterResponse, EmptyHandlerBodyIsSuccessful) {
    Router<LinearStorage> router;
    ASSERT_TRUE(router.add_route({"/", HttpMethod::GET,
        [](auto&, auto&, auto&) { return ""; }}));
    const auto response = router.route(request_for("/"));
    EXPECT_EQ(response.status, ResponseStatus::Success);
    EXPECT_TRUE(response.body.empty());
}

TEST(RouterResponse, InvalidRequestAndQueryReturnErrorWithoutCallingHandler) {
    Router<LinearStorage> router;
    bool called = false;
    ASSERT_TRUE(router.add_route({"/", HttpMethod::GET, [&](auto&, auto&, auto&) {
        called = true;
        return "ok";
    }}));
    auto request = request_for("/");
    request.valid = false;
    EXPECT_EQ(router.route(request).status, ResponseStatus::Error);
    request.valid = true;
    request.query = "q=%GG";
    EXPECT_EQ(router.route(request).status, ResponseStatus::Error);
    EXPECT_FALSE(called);
}

TEST(RouterResponse, CatchesHandlerExceptionsAndContinuesServing) {
    Router<LinearStorage> router;
    ASSERT_TRUE(router.add_route({"/exception", HttpMethod::GET,
        [](auto&, auto&, auto&) -> Response { throw std::runtime_error("private details"); }}));
    ASSERT_TRUE(router.add_route({"/other", HttpMethod::GET,
        [](auto&, auto&, auto&) -> Response { throw 42; }}));
    ASSERT_TRUE(router.add_route({"/ok", HttpMethod::GET,
        [](auto&, auto&, auto&) { return "ok"; }}));
    for (auto path : {"/exception", "/other"}) {
        const auto response = router.route(request_for(path));
        EXPECT_EQ(response.status, ResponseStatus::Error);
        EXPECT_EQ(response.body, "Internal Server Error\n");
    }
    EXPECT_EQ(router.route(request_for("/ok")).status, ResponseStatus::Success);
}
