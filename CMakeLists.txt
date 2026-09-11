cmake_minimum_required(VERSION 3.16)
project(CXX_REST_API)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

# --- GoogleTest через FetchContent ---
# Первый cmake configure скачает исходники gtest (~30 МБ) и соберёт их.
# Дальше пересборки быстрые, всё кешируется в build-test/_deps/.
include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
)
# gtest по умолчанию подтягивает GoogleMock. Нам не нужно — отключаем.
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# --- Прод-код как библиотека, чтобы тесты могли линковать только её ---
add_library(capi_core STATIC
        http/http_parser.cpp
        http/http_parser.h
        router/router.cpp
        router/router.h
        core/CAPI.cpp
        core/CAPI.h
        core/error.h
        http/http_headers.h
)

target_include_directories(capi_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# --- Бинарь сервера ---
add_executable(CXX_REST_API main.cpp)
target_link_libraries(CXX_REST_API PRIVATE capi_core)

# --- Тесты ---
enable_testing()
add_subdirectory(tests)
