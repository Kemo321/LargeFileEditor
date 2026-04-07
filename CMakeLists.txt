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

set(SOURCES src/Main.cc src/backend/PieceTable.cc src/gui/MainWindow.cc
            include/gui/MainWindow.h include/backend/PieceTable.h)
include_directories(include)

add_executable(LargeFileEditor ${SOURCES})
target_link_libraries(LargeFileEditor PRIVATE Qt6::Core Qt6::Widgets)

set(INSTALL_GTEST
    OFF
    CACHE BOOL "" FORCE)
add_subdirectory(extern/googletest)

enable_testing()
add_subdirectory(tests)
