cmake_minimum_required(VERSION 3.16)
project(CXX_REST_API)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

add_executable(CXX_REST_API main.cpp
        http/http_parser.cpp
        http/http_parser.h)
