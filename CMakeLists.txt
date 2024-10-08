cmake_minimum_required(VERSION 3.25)
find_package(XCmake REQUIRED HINTS ./xcmake)

project(SpectralLiveVideoStreamer
        VERSION 0.0.0
        DESCRIPTION "An ultra low-latency video streamer"
        HOMEPAGE_URL "https://spectralcompute.co.uk/hyperstream"
        LANGUAGES CXX)

# Compiler flags used by every target of this project.
add_compile_options("-Wno-missing-field-initializers") # New warning we need to decide how to handle.

# Something that's On for debug builds and Off otherwise.
if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(DEBUG_ON_OFF On)
else()
    set(DEBUG_ON_OFF Off)
endif()

set(CMAKE_CXX_STANDARD 20)

# Options.
include(CMakeDependentOption)
option(ENABLE_SERVER "Enable building the C++ server." ON)
option(ENABLE_JS "Enable Javascript stuff (e.g: the client library and demo client)." OFF)
cmake_dependent_option(JS_DEV "Build the Javascript stuff in development mode rather than production mode."
        ${DEBUG_ON_OFF} ENABLE_JS Off)
set(CLIENT_STREAM_SERVER Off CACHE STRING "Set the client application (not library) server URL to this.")
set(CLIENT_INFO_URL Off CACHE STRING "Set the client application (not library) info.json URL to this.")

# LTO.
if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

# Sub-projects.
add_subdirectory(ffmpeg-tools)
add_subdirectory(server)

# Documentation.
set(DOCS_PREPROCESSOR_FLAG_NAMES)

add_manual(global_docs_customer INSTALL_DESTINATION docs/live-video-streamer MANUAL_SRC ${CMAKE_CURRENT_LIST_DIR}/docs
           ${DOCS_PREPROCESSOR_FLAG_NAMES})

if (XCMAKE_PRIVATE_DOCS)
    add_manual(global_docs_internal INSTALL_DESTINATION private_docs/live-video-streamer
               MANUAL_SRC ${CMAKE_CURRENT_LIST_DIR}/private_docs)
endif()

# Configure the packaging.
set(CPACK_PACKAGE_NAME "spectral-live-video-streamer")

if (ENABLE_JS)
    include(js.cmake)
endif()
