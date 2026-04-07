file(GLOB_RECURSE TEST_SOURCES "*.cc")

file(GLOB_RECURSE BACKEND_SOURCES "../src/backend/*.cc")

add_executable(unit_tests ${TEST_SOURCES} ${BACKEND_SOURCES})

target_link_libraries(unit_tests PRIVATE GTest::gtest_main Qt6::Core)

include(GoogleTest)
gtest_discover_tests(unit_tests)
