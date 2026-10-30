//
// Created by Aki on 9/11/26.
//

#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "http/http_parser.h"
#include "router/router.h"
#include "router/storage/linear_storage.h"
#include "../router/storage/base.h"

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
    Router<LinearStorage> r;
    r.add_route({"/foo", HttpMethod::GET, [](auto&, auto&, auto&) { return "hello"; }});
    EXPECT_EQ(r.route(make_request(HttpMethod::GET, "/foo")).body, "hello");
}

TEST(Router, MissesOnDifferentMethod) {
    Router<LinearStorage> r;
    r.add_route({"/foo", HttpMethod::GET, [](auto&, auto&, auto&) { return "get-foo"; }});
    EXPECT_NE(r.route(make_request(HttpMethod::POST, "/foo")).body, "get-foo");
}

TEST(Router, ReturnsBadResponseWhenEmpty) {
    Router<LinearStorage> r;
    EXPECT_EQ(r.route(make_request(HttpMethod::GET, "/anything")).body, "404 Not Found\n");
}

TEST(Router, InvokesHandlerWithRequest) {
    Router<LinearStorage> r;
    r.add_route({"/echo", HttpMethod::GET, [](const Request& req, auto&, auto&) {
        return std::string(req.path);
    }});
    EXPECT_EQ(r.route(make_request(HttpMethod::GET, "/echo")).body, "/echo");
}

TEST(Router, MultipleRoutesInOrder) {
    Router<LinearStorage> r;
    r.add_route({"/a", HttpMethod::GET, [](auto&, auto&, auto&) { return "A"; }});
    r.add_route({"/b", HttpMethod::GET, [](auto&, auto&, auto&) { return "B"; }});
    r.add_route({"/c", HttpMethod::GET, [](auto&, auto&, auto&) { return "C"; }});

    EXPECT_EQ(r.route(make_request(HttpMethod::GET, "/a")).body, "A");
    EXPECT_EQ(r.route(make_request(HttpMethod::GET, "/b")).body, "B");
    EXPECT_EQ(r.route(make_request(HttpMethod::GET, "/c")).body, "C");
}

TEST(Router, PathParam) {
    Router<LinearStorage> r;
    r.add_route({"/{id}/id", HttpMethod::GET, [](auto&, auto& pv, auto&)
        {
        for (auto& v : pv) {
            if (v.name == "id") {
                return std::string(v.value);
            }
        }
        return std::string("404 Not Found");
        }});

    r.add_route({"us/{id}/id", HttpMethod::GET, [](auto&, auto& pv, auto&)
    {
    for (auto& v : pv) {
        if (v.name == "id") {
            return std::string(v.value) + "00";
        }
    }
    return std::string("404 Not Found");
    }});

    r.add_route({"/{id}/", HttpMethod::GET, [](auto&, auto& pv, auto&)
    {
    for (auto& v : pv) {
        if (v.name == "id") {
            return std::string(v.value) + "--";
        }
    }
    return std::string("404 Not Found");
    }});

    EXPECT_EQ(r.route(make_request(HttpMethod::GET, "/1234/id")).body, "1234");
    EXPECT_EQ(r.route(make_request(HttpMethod::GET, "/1234/id/")).body, "1234");
    EXPECT_EQ(r.route(make_request(HttpMethod::GET, "us/1234/id")).body, "123400");
    EXPECT_EQ(r.route(make_request(HttpMethod::GET, "us/1234/id/")).body, "123400");
    EXPECT_EQ(r.route(make_request(HttpMethod::GET, "/1234")).body, "1234--");
    EXPECT_EQ(r.route(make_request(HttpMethod::GET, "/1234/")).body, "1234--");
    EXPECT_EQ(r.route(make_request(HttpMethod::GET, "/")).body, "404 Not Found\n");
    EXPECT_EQ(r.route(make_request(HttpMethod::GET, "")).body, "404 Not Found\n");
}

TEST(LinearStorage, MatchesRootButNotEmptyPath) {
    LinearStorage storage;
    storage.add_route({"/", HttpMethod::GET, [](auto&, auto&, auto&) { return "root"; }});
    EXPECT_TRUE(storage.get_route(make_request(HttpMethod::GET, "/")).handler);
    EXPECT_FALSE(storage.get_route(make_request(HttpMethod::GET, "")).handler);
    EXPECT_FALSE(storage.get_route(make_request(HttpMethod::GET, "/extra")).handler);
}

TEST(LinearStorage, RequiresAllSegmentsToMatch) {
    LinearStorage storage;
    storage.add_route({"/users/{id}/posts", HttpMethod::GET,
        [](auto&, auto&, auto&) { return "posts"; }});
    for (auto path : {"/users", "/users/42", "/users/42/comments", "/users/42/posts/extra"}) {
        auto match = storage.get_route(make_request(HttpMethod::GET, path));
        EXPECT_FALSE(match.handler) << path;
        EXPECT_TRUE(match.path_params.empty()) << path;
    }
}

TEST(LinearStorage, DiscardsParamsFromFailedCandidate) {
    LinearStorage storage;
    storage.add_route({"/{wrong}/missing", HttpMethod::GET,
        [](auto&, auto&, auto&) { return "wrong"; }});
    storage.add_route({"/users/{id}", HttpMethod::GET,
        [](auto&, auto&, auto&) { return "right"; }});
    auto match = storage.get_route(make_request(HttpMethod::GET, "/users/42"));
    ASSERT_TRUE(match.handler);
    ASSERT_EQ(match.path_params.size(), 1u);
    EXPECT_EQ(match.path_params[0].name, "id");
    EXPECT_EQ(match.path_params[0].value, "42");
}

TEST(LinearStorage, KeepsMultipleParamsValidAfterLookup) {
    LinearStorage storage;
    storage.add_route({"/users/{user_identifier}/posts/{post_identifier}", HttpMethod::GET,
        [](auto&, auto&, auto&) { return "post"; }});
    const std::string user(128, 'u');
    const std::string post(128, 'p');
    const std::string path = "/users/" + user + "/posts/" + post;
    auto match = storage.get_route(make_request(HttpMethod::GET, path));
    ASSERT_TRUE(match.handler);
    ASSERT_EQ(match.path_params.size(), 2u);
    EXPECT_EQ(match.path_params[0].name, "user_identifier");
    EXPECT_EQ(match.path_params[0].value, user);
    EXPECT_EQ(match.path_params[1].name, "post_identifier");
    EXPECT_EQ(match.path_params[1].value, post);
}

TEST(LinearStorage, PreservesSlashNormalization) {
    LinearStorage storage;
    storage.add_route({"/users/{id}/", HttpMethod::GET,
        [](auto&, auto&, auto&) { return "user"; }});
    const std::string path = std::string(16384, '/') + "users///42///";
    auto match = storage.get_route(make_request(HttpMethod::GET, path));
    ASSERT_TRUE(match.handler);
    ASSERT_EQ(match.path_params.size(), 1u);
    EXPECT_EQ(match.path_params[0].value, "42");
}

TEST(Router, PassesQueryAndPathParamsToHandler) {
    Router<LinearStorage> router;
    router.add_route({"/users/{id}", HttpMethod::GET,
        [](const Request& request, const auto& path, const auto& query) {
            EXPECT_EQ(request.query, "sort=name&limit=10");
            EXPECT_EQ(path.size(), 1u);
            EXPECT_EQ(query.size(), 2u);
            if (path.size() == 1 && query.size() == 2) {
                EXPECT_EQ(path[0].value, "42");
                EXPECT_EQ(query[0].name, "sort");
                EXPECT_EQ(query[0].value, "name");
                EXPECT_EQ(query[1].name, "limit");
                EXPECT_EQ(query[1].value, "10");
            }
            return "ok";
        }});
    http_parser parser;
    ASSERT_TRUE(parser.parse_firstline("GET /users/42?sort=name&limit=10 HTTP/1.1"));
    EXPECT_EQ(router.route(parser.request).body, "ok");
}

TEST(Router, PreservesDuplicateKeysAndEmptyValues) {
    Router<LinearStorage> router;
    router.add_route({"/", HttpMethod::GET, [](auto&, auto&, const auto& query) {
        EXPECT_EQ(query.size(), 5u);
        if (query.size() == 5) {
            EXPECT_EQ(query[0].name, "tag");
            EXPECT_EQ(query[0].value, "a");
            EXPECT_EQ(query[1].name, "tag");
            EXPECT_EQ(query[1].value, "b=c");
            EXPECT_EQ(query[2].name, "flag");
            EXPECT_TRUE(query[2].value.empty());
            EXPECT_EQ(query[3].name, "empty");
            EXPECT_TRUE(query[3].value.empty());
            EXPECT_TRUE(query[4].name.empty());
            EXPECT_EQ(query[4].value, "value");
        }
        return "ok";
    }});
    auto request = make_request(HttpMethod::GET, "/");
    request.query = "&tag=a&&tag=b=c&flag&empty=&=value&";
    EXPECT_EQ(router.route(request).body, "ok");
}

TEST(Router, DecodesQueryWithoutSplittingEncodedDelimiters) {
    Router<LinearStorage> router;
    router.add_route({"/", HttpMethod::GET, [](const Request& request, auto&, const auto& query) {
        EXPECT_EQ(request.query, "first+name=Ada+Lovelace&%26%3d=%2B%26%3D%2520&word=%D0%AF&nul=%00");
        EXPECT_EQ(query.size(), 4u);
        if (query.size() == 4) {
            EXPECT_EQ(query[0].name, "first name");
            EXPECT_EQ(query[0].value, "Ada Lovelace");
            EXPECT_EQ(query[1].name, "&=");
            EXPECT_EQ(query[1].value, "+&=%20");
            EXPECT_EQ(query[2].value, "\xD0\xAF");
            EXPECT_EQ(query[3].value, std::string_view("\0", 1));
        }
        return "ok";
    }});
    auto request = make_request(HttpMethod::GET, "/");
    request.query = "first+name=Ada+Lovelace&%26%3d=%2B%26%3D%2520&word=%D0%AF&nul=%00";
    EXPECT_EQ(router.route(request).body, "ok");
    EXPECT_EQ(router.route(request).body, "ok");
}

TEST(Router, RejectsMalformedQueryEscapesBeforeCallingHandler) {
    Router<LinearStorage> router;
    bool called = false;
    router.add_route({"/", HttpMethod::GET, [&](auto&, auto&, auto&) {
        called = true;
        return "ok";
    }});
    auto request = make_request(HttpMethod::GET, "/");
    for (auto query : {"a=%", "a=%1", "a=%GG", "a=%0g", "a=%-1", "a=%+1", "a=%0x", "%=x", "a=ok&b=%2"}) {
        request.query = query;
        EXPECT_EQ(router.route(request).body, "Bad Response\n") << query;
        EXPECT_FALSE(called) << query;
    }
}

TEST(Router, EmptyQueryDoesNotReusePreviousParams) {
    Router<LinearStorage> router;
    router.add_route({"/", HttpMethod::GET, [](auto&, auto&, const auto& query) {
        return std::to_string(query.size());
    }});
    auto request = make_request(HttpMethod::GET, "/");
    request.query = "a=1";
    EXPECT_EQ(router.route(request).body, "1");
    request.query = "";
    EXPECT_EQ(router.route(request).body, "0");
    request.query = "&&";
    EXPECT_EQ(router.route(request).body, "0");
}

TEST(Router, KeepsDecodedViewsValidAcrossManyParams) {
    Router<LinearStorage> router;
    router.add_route({"/", HttpMethod::GET, [](auto&, auto&, const auto& query) {
        EXPECT_EQ(query.size(), 64u);
        for (std::size_t i = 0; i < query.size(); ++i) {
            EXPECT_EQ(query[i].name, "key" + std::to_string(i));
            EXPECT_EQ(query[i].value, std::string(128, 'x') + " ");
        }
        return "ok";
    }});
    std::string query;
    for (int i = 0; i < 64; ++i) {
        if (i != 0) query += '&';
        query += "key" + std::to_string(i) + '=' + std::string(128, 'x') + "%20";
    }
    auto request = make_request(HttpMethod::GET, "/");
    request.query = query;
    EXPECT_EQ(router.route(request).body, "ok");
}

TEST(Router, StaticRouteWinsRegardlessOfRegistrationOrder) {
    for (bool static_first : {false, true}) {
        Router<LinearStorage> router;
        const Route dynamic{"/users/{id}", HttpMethod::GET,
            [](auto&, const auto& path, auto&) {
                EXPECT_EQ(path.size(), 1u);
                return "dynamic";
            }};
        const Route exact{"/users/me", HttpMethod::GET,
            [](auto&, const auto& path, auto&) {
                EXPECT_TRUE(path.empty());
                return "static";
            }};
        ASSERT_TRUE(router.add_route(static_first ? exact : dynamic));
        ASSERT_TRUE(router.add_route(static_first ? dynamic : exact));
        EXPECT_EQ(router.route(make_request(HttpMethod::GET, "/users/me")).body, "static");
        EXPECT_EQ(router.route(make_request(HttpMethod::GET, "/users/42")).body, "dynamic");
    }
}

TEST(Router, ComparesSpecificityLeftToRightRatherThanCountingStaticSegments) {
    for (bool left_first : {false, true}) {
        Router<LinearStorage> router;
        const Route left{"/a/{second}/{third}", HttpMethod::GET,
            [](auto&, const auto& path, auto&) {
                EXPECT_EQ(path.size(), 2u);
                if (path.size() == 2) {
                    EXPECT_EQ(path[0].name, "second");
                    EXPECT_EQ(path[0].value, "b");
                    EXPECT_EQ(path[1].value, "c");
                }
                return "left";
            }};
        const Route right{"/{first}/b/c", HttpMethod::GET,
            [](auto&, auto&, auto&) { return "right"; }};
        ASSERT_TRUE(router.add_route(left_first ? left : right));
        ASSERT_TRUE(router.add_route(left_first ? right : left));
        EXPECT_EQ(router.route(make_request(HttpMethod::GET, "/a/b/c")).body, "left");
    }
}

TEST(Router, FallsBackWhenMoreSpecificRouteDoesNotFullyMatch) {
    for (bool static_first : {false, true}) {
        Router<LinearStorage> router;
        const Route exact{"/users/me/settings", HttpMethod::GET,
            [](auto&, auto&, auto&) { return "settings"; }};
        const Route dynamic{"/users/{id}/posts", HttpMethod::GET,
            [](auto&, const auto& path, auto&) {
                EXPECT_EQ(path.size(), 1u);
                if (path.size() == 1) {
                    EXPECT_EQ(path[0].value, "me");
                }
                return "posts";
            }};
        ASSERT_TRUE(router.add_route(static_first ? exact : dynamic));
        ASSERT_TRUE(router.add_route(static_first ? dynamic : exact));
        EXPECT_EQ(router.route(make_request(HttpMethod::GET, "/users/me/posts")).body, "posts");
    }
}

TEST(Router, RejectsEquivalentPatternsWithoutReplacingHandler) {
    Router<LinearStorage> router;
    ASSERT_TRUE(router.add_route({"/users/{id}", HttpMethod::GET,
        [](auto&, const auto& path, auto&) {
            EXPECT_EQ(path[0].name, "id");
            return "original";
        }}));
    const auto other = [](auto&, auto&, auto&) { return "other"; };
    EXPECT_FALSE(router.add_route({"/users/{id}", HttpMethod::GET, other}));
    EXPECT_FALSE(router.add_route({"users///{name}/", HttpMethod::GET, other}));
    EXPECT_EQ(router.route(make_request(HttpMethod::GET, "/users/42")).body, "original");
    ASSERT_TRUE(router.add_route({"/fixed", HttpMethod::GET, other}));
    EXPECT_FALSE(router.add_route({"//fixed//", HttpMethod::GET, other}));
    ASSERT_TRUE(router.add_route({"/", HttpMethod::GET, other}));
    EXPECT_FALSE(router.add_route({"///", HttpMethod::GET, other}));
}

TEST(Router, FiltersMethodBeforeChoosingSpecificity) {
    Router<LinearStorage> router;
    ASSERT_TRUE(router.add_route({"/users/me", HttpMethod::GET,
        [](auto&, auto&, auto&) { return "get"; }}));
    ASSERT_TRUE(router.add_route({"/users/{id}", HttpMethod::POST,
        [](auto&, auto&, auto&) { return "post"; }}));
    ASSERT_TRUE(router.add_route({"/users/{name}", HttpMethod::GET,
        [](auto&, auto&, auto&) { return "get-user"; }}));
    EXPECT_EQ(router.route(make_request(HttpMethod::POST, "/users/me")).body, "post");
    EXPECT_EQ(router.route(make_request(HttpMethod::GET, "/users/me")).body, "get");
    EXPECT_EQ(router.route(make_request(HttpMethod::HEAD, "/users/me")).body, "404 Not Found\n");
}

TEST(Router, RejectsInvalidRegistrations) {
    Router<LinearStorage> router;
    const auto handler = [](auto&, auto&, auto&) { return "ok"; };
    for (auto path : {"", "/users/{}", "/users/{id", "/users/id}", "/users/{a{b}}",
                      "/users/prefix{id}", "/users?x=1", "/users#fragment"}) {
        EXPECT_FALSE(router.add_route({path, HttpMethod::GET, handler})) << path;
    }
    EXPECT_FALSE(router.add_route({"/users", HttpMethod::NONE, handler}));
    EXPECT_FALSE(router.add_route({"/users", HttpMethod::GET, {}}));
    EXPECT_EQ(router.route(make_request(HttpMethod::GET, "/users")).body, "404 Not Found\n");
    EXPECT_TRUE(router.add_route({"/users", HttpMethod::GET, handler}));
}

TEST(Router, KeepsPathCaseSensitiveAndDoesNotDecodePathSegments) {
    Router<LinearStorage> router;
    ASSERT_TRUE(router.add_route({"/users/{id}", HttpMethod::GET,
        [](auto&, const auto& path, auto&) { return std::string(path[0].value); }}));
    EXPECT_EQ(router.route(make_request(HttpMethod::GET, "/Users/42")).body, "404 Not Found\n");
    EXPECT_EQ(router.route(make_request(HttpMethod::GET, "/users/a%2Fb")).body, "a%2Fb");
    EXPECT_EQ(router.route(make_request(HttpMethod::GET, "/users/..")).body, "..");
    EXPECT_EQ(router.route(make_request(HttpMethod::GET, "/users//")).body, "404 Not Found\n");
}

TEST(Router, KeepsRepeatedPathNamesInSegmentOrder) {
    Router<LinearStorage> router;
    ASSERT_TRUE(router.add_route({"/{id}/{id}", HttpMethod::GET,
        [](auto&, const auto& path, auto&) {
            EXPECT_EQ(path.size(), 2u);
            if (path.size() == 2) {
                EXPECT_EQ(path[0].name, "id");
                EXPECT_EQ(path[1].name, "id");
                EXPECT_EQ(path[0].value, "a");
                EXPECT_EQ(path[1].value, "b");
            }
            return "ok";
        }}));
    EXPECT_EQ(router.route(make_request(HttpMethod::GET, "/a/b")).body, "ok");
}

TEST(Router, PreservesMutableHandlerStateWithoutCopiesDuringDispatch) {
    struct CopyProbe {
        int& copies;
        explicit CopyProbe(int& count) : copies(count) {}
        CopyProbe(const CopyProbe& other) : copies(other.copies) { ++copies; }
    };
    int copies = 0;
    Router<LinearStorage> router;
    ASSERT_TRUE(router.add_route({"/counter", HttpMethod::GET,
        [probe = CopyProbe(copies), count = 0](auto&, auto&, auto&) mutable {
            (void)probe;
            return std::to_string(++count);
        }}));
    copies = 0;
    EXPECT_EQ(router.route(make_request(HttpMethod::GET, "/counter")).body, "1");
    EXPECT_EQ(router.route(make_request(HttpMethod::GET, "/counter")).body, "2");
    EXPECT_EQ(router.route(make_request(HttpMethod::GET, "/counter")).body, "3");
    EXPECT_EQ(copies, 0);
}

TEST(LinearStorage, BorrowsSameHandlerAcrossLookups) {
    LinearStorage storage;
    ASSERT_TRUE(storage.add_route({"/counter", HttpMethod::GET,
        [count = 0](auto&, auto&, auto&) mutable { return std::to_string(++count); }}));
    const auto request = make_request(HttpMethod::GET, "/counter");
    const auto first = storage.get_route(request);
    const auto second = storage.get_route(request);
    ASSERT_NE(first.handler, nullptr);
    EXPECT_EQ(first.handler, second.handler);
    EXPECT_EQ((*first.handler)(request, first.path_params, first.query_params).body, "1");
    EXPECT_EQ((*second.handler)(request, second.path_params, second.query_params).body, "2");
}
