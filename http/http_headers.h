//
// Created by Aki on 9/9/26.
//

#ifndef CXX_REST_API_HTTP_HEADERS_H
#define CXX_REST_API_HTTP_HEADERS_H

namespace capi {

enum class HttpMethod {
    NONE,
    GET,
    HEAD,
    OPTIONS,
    TRACE,
    PUT,
    DELETE,
    POST,
    PATCH,
    CONNECT
};

enum class HttpVersion {
    UNKNOWN,
    HTTP_1_0,
    HTTP_1_1,
};

}

#endif //CXX_REST_API_HTTP_HEADERS_H
