cmake_minimum_required(VERSION 3.25)
include(xcmake/scripts/Init.cmake)
project(SpectralLiveVideoStreamer
        VERSION 0.0.0
        DESCRIPTION "An ultra low-latency video streamer"
        HOMEPAGE_URL "https://spectralcompute.co.uk/hyperstream"
        LANGUAGES CXX)
include(XCMake)

# LTO.
if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

# We want to figure out if the only stuff is Javascript (i.e: client only).
set(JS_ONLY 1 CACHE INTERNAL "" FORCE)

# Sub-projects.
add_subdirectory(server)
add_subdirectory(js) # Must be last for JS_ONLY to work properly.

# Documentation.
if (JS_ONLY)
    set(DOCS_PREPROCESSOR_FLAG_NAMES PREPROCESSOR_FLAG_NAMES CLIENT_ONLY)
else()
    set(DOCS_PREPROCESSOR_FLAG_NAMES)
endif()
add_manual(global_docs_customer INSTALL_DESTINATION docs/live-video-streamer MANUAL_SRC ${CMAKE_CURRENT_LIST_DIR}/docs
           ${DOCS_PREPROCESSOR_FLAG_NAMES})
if (XCMAKE_PRIVATE_DOCS)
    add_manual(global_docs_internal INSTALL_DESTINATION private_docs/live-video-streamer
               MANUAL_SRC ${CMAKE_CURRENT_LIST_DIR}/private_docs)
endif()

# Configure the packaging.
if (JS_ONLY)
    set(CPACK_PACKAGE_NAME "spectral-live-video-streamer-client") # Client only.
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}") # Target independent name.
else()
    set(CPACK_PACKAGE_NAME "spectral-live-video-streamer")
endif()
