add_executable(unit_tests
        test_http_parser.cpp
        test_router.cpp
)

target_link_libraries(unit_tests PRIVATE capi_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(unit_tests)
