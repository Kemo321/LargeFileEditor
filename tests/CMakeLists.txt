file(GLOB_RECURSE TEST_SOURCES "*.cc")

file(GLOB_RECURSE BACKEND_SOURCES "../src/backend/*.cc")

file(GLOB_RECURSE UTIL_SOURCES "../src/util/*.cc")

set(GUI_TEST_SOURCES
    ../src/gui/LineManager.cc ../src/gui/CursorManager.cc
    ../src/gui/EditorController.cc ../include/gui/LineManager.h
    ../include/gui/CursorManager.h ../include/gui/EditorController.h)

find_package(Qt6 REQUIRED COMPONENTS Gui Test)

add_executable(unit_tests ${TEST_SOURCES} ${BACKEND_SOURCES} ${UTIL_SOURCES}
                          ${GUI_TEST_SOURCES})

target_link_libraries(unit_tests PRIVATE GTest::gtest_main Qt6::Core Qt6::Gui
                                         Qt6::Test)

include(GoogleTest)
gtest_discover_tests(unit_tests)
