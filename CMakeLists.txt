cmake_minimum_required(VERSION 3.16)
project(
  LargeFileEditor
  VERSION 1.0
  LANGUAGES CXX)

if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Concurrent)

option(ENABLE_COVERAGE "Build with coverage instrumentation (GCC/Clang)" OFF)

if(ENABLE_COVERAGE)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(STATUS "Coverage instrumentation enabled.")
    add_compile_options(--coverage -fprofile-arcs -ftest-coverage -O0 -g)
    add_link_options(--coverage)
  else()
    message(WARNING "ENABLE_COVERAGE requires GCC or Clang; ignoring.")
  endif()
endif()

include_directories(include)

set(SOURCES
    src/Main.cc
    src/backend/PieceTable.cc
    src/backend/MemoryMappedFile.cc
    src/backend/KmpSearch.cc
    src/backend/HistoryManager.cc
    src/gui/MainWindow.cc
    src/gui/LargeFileViewer.cc
    src/gui/LineManager.cc
    src/gui/FindReplaceDialog.cc
    src/gui/CursorManager.cc
    src/gui/TextRenderer.cc
    src/gui/EditorController.cc
    src/gui/BackgroundTaskManager.cc
    src/util/Utf8Utils.cc
    src/util/FileUtils.cc
    include/gui/MainWindow.h
    include/gui/LargeFileViewer.h
    include/gui/LineManager.h
    include/gui/FindReplaceDialog.h
    include/gui/CursorManager.h
    include/gui/TextRenderer.h
    include/gui/EditorController.h
    include/gui/BackgroundTaskManager.h
    include/backend/PieceTable.h
    include/backend/Piece.h
    include/backend/MemoryMappedFile.h
    include/backend/KmpSearch.h
    include/backend/HistoryManager.h
    include/util/Utf8Utils.h
    include/util/FileUtils.h)
include_directories(include)

add_executable(LargeFileEditor ${SOURCES})
target_link_libraries(LargeFileEditor PRIVATE Qt6::Core Qt6::Widgets
                                              Qt6::Concurrent)

find_package(GTest REQUIRED)

enable_testing()
add_subdirectory(tests)

find_package(Doxygen)

if(DOXYGEN_FOUND)
  set(DOXYGEN_INPUT_DIR ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
  set(DOXYGEN_OUTPUT_DIR ${CMAKE_BINARY_DIR}/docs)

  doxygen_add_docs(
    doc ${DOXYGEN_INPUT_DIR} ALL
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Generating API documentation with Doxygen")

  message(STATUS "Doxygen found. Target 'doc' is available.")
else()
  message(WARNING "Doxygen not found. Target 'doc' will not be created.")
endif()

if(ENABLE_COVERAGE)
  find_program(LCOV_PATH lcov)
  find_program(GENHTML_PATH genhtml)
  find_program(XVFB_RUN xvfb-run)

  if(LCOV_PATH AND GENHTML_PATH)
    if(XVFB_RUN)
      set(_ctest_cmd ${XVFB_RUN} --auto-servernum ${CMAKE_CTEST_COMMAND}
                     --output-on-failure)
    else()
      set(_ctest_cmd ${CMAKE_CTEST_COMMAND} --output-on-failure)
    endif()
    set(_lcov ${LCOV_PATH} --ignore-errors mismatch,unused,empty,gcov)

    add_custom_target(
      coverage
      COMMAND ${_lcov} --directory ${CMAKE_BINARY_DIR} --zerocounters
      COMMAND ${_ctest_cmd}
      COMMAND ${_lcov} --directory ${CMAKE_BINARY_DIR} --capture --output-file
              ${CMAKE_BINARY_DIR}/coverage.info
      COMMAND
        ${_lcov} --remove ${CMAKE_BINARY_DIR}/coverage.info /usr/* */tests/*
        */googletest/* */extern/* --output-file
        ${CMAKE_BINARY_DIR}/coverage.filtered.info
      COMMAND ${GENHTML_PATH} ${CMAKE_BINARY_DIR}/coverage.filtered.info
              --output-directory ${CMAKE_BINARY_DIR}/coverage_report
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      DEPENDS unit_tests
      COMMENT "Running LCOV coverage pipeline -> coverage_report/index.html"
      VERBATIM)
  else()
    message(WARNING "lcov/genhtml not found; 'coverage' target unavailable.")
  endif()
endif()
