//
// Created by Aki on 9/11/26.
//

#include <gtest/gtest.h>
#include "http/http_parser.h"

using namespace capi;

TEST(HttpParser, FirstLineValid) {
    http_parser http;
    ASSERT_TRUE(http.parse_firstline("GET /foo HTTP/1.1"));
    EXPECT_EQ(http.request.method, HttpMethod::GET);
    EXPECT_EQ(http.request.path, "/foo");
    EXPECT_EQ(http.request.version, HttpVersion::HTTP_1_1);
}

TEST(HttpParser, FirstLineWithQuery) {
    http_parser http;
    ASSERT_TRUE(http.parse_firstline("GET /foo?a=1&b=2 HTTP/1.1"));
    EXPECT_EQ(http.request.path, "/foo");
    EXPECT_EQ(http.request.query, "a=1&b=2");
}

TEST(HttpParser, FirstLineInvalidMethod) {
    http_parser http;
    EXPECT_FALSE(http.parse_firstline("FOOBAR /x HTTP/1.1"));
}

TEST(HttpParser, HeaderBasic) {
    http_parser http;
    std::string_view raw =
        "GET / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "User-Agent: curl/8.0\r\n"
        "Accept: */*\r\n";
    ASSERT_TRUE(http.parse_header(raw));

    auto host = http.get_value("Host");
    ASSERT_TRUE(host.has_value());
    EXPECT_EQ(*host, "example.com");

    auto ua = http.get_value("User-Agent");
    ASSERT_TRUE(ua.has_value());
    EXPECT_EQ(*ua, "curl/8.0");
}

TEST(HttpParser, ContentLengthParsing) {
    {
        http_parser http;
        std::string_view raw =
            "POST / HTTP/1.1\r\n"
            "Content-Length: 42\r\n";
        ASSERT_TRUE(http.parse_header(raw));
        auto cl = http.content_length();
        ASSERT_TRUE(cl.has_value());
        EXPECT_EQ(*cl, 42u);
    }

    {
        http_parser http;
        std::string_view raw =
            "POST / HTTP/1.1\r\n"
            "Content-Length: not-a-number\r\n";
        ASSERT_TRUE(http.parse_header(raw));
        EXPECT_FALSE(http.content_length().has_value());
    }

    {
        http_parser http;
        std::string_view raw =
            "POST / HTTP/1.1\r\n"
            "Accept: */*\r\n";
        ASSERT_TRUE(http.parse_header(raw));
        EXPECT_FALSE(http.content_length().has_value());
    }
}

TEST(HttpParser, SetBodyCopiesViewIntoRequest) {
    http_parser http;
    ASSERT_TRUE(http.parse_firstline("POST / HTTP/1.1"));
    {
        std::string src = "{\"hello\":\"world\"}";
        ASSERT_TRUE(http.set_body(src));
    }
    EXPECT_EQ(http.request.body, "{\"hello\":\"world\"}");
}
