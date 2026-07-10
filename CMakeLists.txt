cmake_minimum_required(VERSION 3.19 FATAL_ERROR)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    # Bundled FFmpeg and the current Qt packages require macOS 14 or newer.
    set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "Minimum macOS deployment version" FORCE)
endif()

project(all)
include(CTest)

add_subdirectory(QtScrcpy)
