cmake_minimum_required(VERSION 3.16)
project(CXX_REST_API)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

# GoogleTest
include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
)
# GoogleMock off
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_library(capi_core STATIC
        http/http_parser.cpp
        http/http_parser.h
        http/response.cpp
        http/response.h
        router/router.h
        router/router.cpp
        core/CAPI.cpp
        core/CAPI.h
        core/error.h
        http/http_headers.h
        router/storage/linear_storage.cpp
        router/storage/linear_storage.h
        router/storage/base.h
)

target_include_directories(capi_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(CXX_REST_API main.cpp)
target_link_libraries(CXX_REST_API PRIVATE capi_core)

enable_testing()
add_subdirectory(tests)
