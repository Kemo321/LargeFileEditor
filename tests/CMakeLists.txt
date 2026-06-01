file(GLOB_RECURSE TEST_SOURCES "*.cc")

file(GLOB_RECURSE BACKEND_SOURCES "../src/backend/*.cc")

file(GLOB_RECURSE UTIL_SOURCES "../src/util/*.cc")

# Display-free presentation-logic units exercised by the headless tests in tests/gui.
# The widget-heavy classes (MainWindow, LargeFileViewer, TextRenderer, FindReplaceDialog,
# BackgroundTaskManager) need a real display and stay out of the unit-test binary.
# The headers are listed alongside the sources so AUTOMOC generates the meta-object code for the
# Q_OBJECT classes (CursorManager, EditorController); without them the moc symbols never link.
set(GUI_TEST_SOURCES
    ../src/gui/LineManager.cc
    ../src/gui/CursorManager.cc
    ../src/gui/EditorController.cc
    ../include/gui/LineManager.h
    ../include/gui/CursorManager.h
    ../include/gui/EditorController.h)

# QKeyEvent lives in Qt6::Gui; QSignalSpy lives in Qt6::Test.
find_package(Qt6 REQUIRED COMPONENTS Gui Test)

add_executable(unit_tests ${TEST_SOURCES} ${BACKEND_SOURCES} ${UTIL_SOURCES}
                          ${GUI_TEST_SOURCES})

target_link_libraries(unit_tests PRIVATE GTest::gtest_main Qt6::Core Qt6::Gui
                                         Qt6::Test)

include(GoogleTest)
gtest_discover_tests(unit_tests)
