//
// Created by Aki on 9/11/26.
//

#include <gtest/gtest.h>
#include "http/http_parser.h"
#include "router/router.h"

using namespace capi;

namespace {

Request make_request(HttpMethod method, std::string_view path) {
    Request r;
    r.method = method;
    r.path = path;
    r.valid = true;
    return r;
}

}

TEST(Router, FindsExactMatch) {
    Router r;
    r.add_route({"/foo", HttpMethod::GET, [](const Request&) { return "hello"; }});
    EXPECT_EQ(r.route(make_request(HttpMethod::GET, "/foo")), "hello");
}

TEST(Router, MissesOnDifferentMethod) {
    Router r;
    r.add_route({"/foo", HttpMethod::GET, [](const Request&) { return "get-foo"; }});
    EXPECT_NE(r.route(make_request(HttpMethod::POST, "/foo")), "get-foo");
}

TEST(Router, ReturnsBadResponseWhenEmpty) {
    Router r;
    EXPECT_EQ(r.route(make_request(HttpMethod::GET, "/anything")), "Bad Response\n");
}

TEST(Router, InvokesHandlerWithRequest) {
    Router r;
    r.add_route({"/echo", HttpMethod::GET, [](const Request& req) {
        return std::string(req.path);
    }});
    EXPECT_EQ(r.route(make_request(HttpMethod::GET, "/echo")), "/echo");
}

TEST(Router, MultipleRoutesInOrder) {
    Router r;
    r.add_route({"/a", HttpMethod::GET, [](const Request&) { return "A"; }});
    r.add_route({"/b", HttpMethod::GET, [](const Request&) { return "B"; }});
    r.add_route({"/c", HttpMethod::GET, [](const Request&) { return "C"; }});

    EXPECT_EQ(r.route(make_request(HttpMethod::GET, "/a")), "A");
    EXPECT_EQ(r.route(make_request(HttpMethod::GET, "/b")), "B");
    EXPECT_EQ(r.route(make_request(HttpMethod::GET, "/c")), "C");
}
