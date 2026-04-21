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

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

include_directories(include)

set(SOURCES
    src/Main.cc
    src/backend/PieceTable.cc
    src/gui/MainWindow.cc
    src/gui/LargeFileViewer.cc
    src/gui/FindReplaceDialog.cc
    include/gui/MainWindow.h
    include/gui/LargeFileViewer.h
    include/gui/FindReplaceDialog.h
    include/backend/PieceTable.h)
include_directories(include)

add_executable(LargeFileEditor ${SOURCES})
target_link_libraries(LargeFileEditor PRIVATE Qt6::Core Qt6::Widgets)

set(INSTALL_GTEST
    OFF
    CACHE BOOL "" FORCE)

if(EXISTS "${CMAKE_SOURCE_DIR}/extern/googletest/CMakeLists.txt")
  add_subdirectory(extern/googletest)
else()
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
  )
  FetchContent_MakeAvailable(googletest)
endif()

enable_testing()
add_subdirectory(tests)

# Doxygen Documentation
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

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_options(--coverage)
  add_link_options(--coverage)
endif()
